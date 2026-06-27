// Interactive audio/video console for the NVIDIA Jetson Orin Nano (and other
// Linux devices). Toggle each media path at runtime with single keypresses.
// Usage: jetson_orin_nano [meeting_id] [token] [pulse_source] [v4l2_device]

#include <videosdk/videosdk.hpp>

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <termios.h>
#include <unistd.h>

namespace {

constexpr const char *kV4l2Device = "/dev/video0";
constexpr int kCamWidth = 1280;
constexpr int kCamHeight = 720;
constexpr int kCamFps = 30;

std::atomic<bool> g_running{true};
termios g_saved_term{};
bool g_term_saved = false;
std::mutex g_console_mutex;

const char *env_or(const char *name, const char *fallback) {
  const char *v = std::getenv(name);
  return (v && v[0]) ? v : fallback;
}

void restore_terminal() {
  if (g_term_saved)
    tcsetattr(STDIN_FILENO, TCSANOW, &g_saved_term);
}

void raw_terminal() {
  if (tcgetattr(STDIN_FILENO, &g_saved_term) != 0)
    return;
  g_term_saved = true;
  termios t = g_saved_term;
  t.c_lflag &= ~(ICANON | ECHO);
  t.c_cc[VMIN] = 1;
  t.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

extern "C" void handle_signal(int) { g_running.store(false); }

// Install SIGINT/SIGTERM handlers WITHOUT SA_RESTART. std::signal() on glibc
// uses BSD semantics (SA_RESTART set), which auto-restarts the blocking read()
// in the main loop on signal delivery — so Ctrl-C / `systemctl stop` would set
// g_running=false but the loop would never wake to see it until a keypress.
// With SA_RESTART cleared, the signal interrupts read() (EINTR), the loop
// re-checks g_running, and shutdown runs cleanly.
void install_signal_handlers() {
  struct sigaction sa {};
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
}

// Serialize all console output. The SDK invokes onParticipantJoined/Left,
// onError and onData on its own internal threads, which would otherwise
// interleave with — and splice into the middle of — the main thread's prints.
// Every write, from callbacks and from main, goes through emit() (or locks
// g_console_mutex directly) so each logical line is emitted atomically. The
// printf format attribute preserves -Wformat checking on call sites.
__attribute__((format(printf, 1, 2))) void emit(const char *fmt, ...) {
  std::lock_guard<std::mutex> lock(g_console_mutex);
  va_list ap;
  va_start(ap, fmt);
  std::vprintf(fmt, ap);
  va_end(ap);
  std::fflush(stdout);
}

void print_help() {
  emit("\n  m  mic        c  camera      s  speaker\n"
       "  d  display    i  stats       t  send text\n"
       "  b  send binary blob          h  help       q  quit\n\n");
}

void print_status(bool mic, bool cam, bool spk, bool disp) {
  emit("  [mic %s] [camera %s] [speaker %s] [display %s]\n",
       mic ? "on" : "off", cam ? "on" : "off", spk ? "on" : "off",
       disp ? "on" : "off");
}

} // namespace

int main(int argc, char **argv) {
  const std::string meeting_id =
      (argc >= 2) ? argv[1] : env_or("VIDEOSDK_MEETING_ID", "");
  const std::string token =
      (argc >= 3) ? argv[2] : env_or("VIDEOSDK_TOKEN", "");
  const std::string pulse_src =
      (argc >= 4) ? argv[3] : env_or("PULSE_SOURCE", "default");
  const std::string v4l_dev = (argc >= 5) ? argv[4] : kV4l2Device;

  if (meeting_id.empty() || token.empty()) {
    // argv[0] may be NULL when argc == 0 (a legal execve invocation); fall back
    // to a fixed name so the usage message never dereferences a null pointer.
    const char *prog = (argc > 0 && argv[0]) ? argv[0] : "jetson_orin_nano";
    std::fprintf(stderr,
                 "set VIDEOSDK_TOKEN and VIDEOSDK_MEETING_ID, or run:\n"
                 "  %s <meeting_id> <token> [pulse_source] [v4l2_device]\n",
                 prog);
    return 1;
  }

  videosdk::MeetingConfig config;
  config.meetingId = meeting_id;
  config.token = token;
  config.name = "Jetson Orin Nano";
  videosdk::Meeting meeting(config);

  meeting.onParticipantJoined([](const std::string &id, const std::string &name) {
    emit("\r* %s joined (%s)\n", name.c_str(), id.c_str());
  });
  meeting.onParticipantLeft([](const std::string &id) {
    emit("\r* %s left\n", id.c_str());
  });
  meeting.onError([](videosdk::ErrorCode, const std::string &msg) {
    // stderr, but under the same lock so error lines don't splice into stdout.
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::fprintf(stderr, "\r! error: %s\n", msg.c_str());
    std::fflush(stderr);
  });
  meeting.onData([](const uint8_t *data, size_t len, bool is_binary) {
    if (is_binary) {
      // Binary frame — hex preview (bytes may contain NULs). Build the whole
      // line first, then emit() once, so the hex bytes can't be interleaved.
      std::string out = "\r[received data] " + std::to_string(len) +
                        " binary bytes:";
      char byte[8];
      for (size_t i = 0; i < len && i < 32; ++i) {
        std::snprintf(byte, sizeof(byte), " %02x", data[i]);
        out += byte;
      }
      out += len > 32 ? " ...\n" : "\n";
      emit("%s", out.c_str());
    } else {
      emit("\r[received data] %.*s\n", static_cast<int>(len),
           reinterpret_cast<const char *>(data));
    }
  });

  // Publish 3 VP8 simulcast layers (smallest → largest). The top layer
  // matches the camera capture size (kCamWidth x kCamHeight); the SDK
  // downscales for the lower layers. Each layer caps its own bitrate/fps.
  // Per-field named initializers make each parameter explicit; pass 1 entry
  // for a single stream, or 2 for bottom+top.
  meeting.setSimulcastLayers({
      // {.width = kCamWidth / 4, .height = kCamHeight / 4,
      //  .maxBitrateKbps = 300, .maxFps = 15},   // q: 320x180
      // {.width = kCamWidth / 2, .height = kCamHeight / 2,
      //  .maxBitrateKbps = 1500, .maxFps = 20},   // h: 640x360
      {.width = kCamWidth, .height = kCamHeight,
       .maxBitrateKbps = 3000, .maxFps = 30},  // f: 1280x720
  });

  // Install handlers BEFORE join()/enable* so a Ctrl-C or SIGTERM during the
  // (potentially slow) connect or device-open phase triggers a clean shutdown
  // instead of killing the process with default disposition mid-join.
  install_signal_handlers();

  emit("joining %s ...\n", meeting_id.c_str());
  if (!meeting.join()) {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::fprintf(stderr, "join failed\n");
    return 2;
  }

  // A signal may have arrived during the (blocking) join(). If so, don't bother
  // opening the camera/mic/display — just leave cleanly.
  if (!g_running.load()) {
    meeting.leave();
    return 0;
  }
  emit("joined as \"%s\".\n", config.name.c_str());

  bool spk = meeting.enableSpeaker();
  bool mic = meeting.enableMic(pulse_src);
  bool cam = meeting.enableCamera(v4l_dev, kCamWidth, kCamHeight, kCamFps);
  bool disp = meeting.enableVideoDisplay(kCamWidth, kCamHeight);
  print_status(mic, cam, spk, disp);
  print_help();

  raw_terminal();
  // Backstop: guarantee the terminal is restored on every exit path (normal
  // return, 'q', a signal-interrupted loop exit, or std::exit), even if the
  // explicit restore_terminal() below is somehow skipped. No-op when stdin is
  // not a TTY (g_term_saved == false).
  std::atexit(restore_terminal);

  while (g_running.load()) {
    char ch = 0;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n < 0) {
      if (errno == EINTR)
        continue;  // interrupted by a signal — re-check g_running and loop
      break;       // genuine read error
    }
    if (n == 0)
      break;       // EOF: stdin closed / non-interactive

    switch (ch) {
    case 'm':
      mic = mic ? (meeting.disableMic(), false) : meeting.enableMic(pulse_src);
      print_status(mic, cam, spk, disp);
      break;
    case 'c':
      cam = cam ? (meeting.disableCamera(), false)
                : meeting.enableCamera(v4l_dev, kCamWidth, kCamHeight, kCamFps);
      print_status(mic, cam, spk, disp);
      break;
    case 's':
      spk = spk ? (meeting.disableSpeaker(), false) : meeting.enableSpeaker();
      print_status(mic, cam, spk, disp);
      break;
    case 'd':
      disp = disp ? (meeting.disableVideoDisplay(), false)
                  : meeting.enableVideoDisplay(kCamWidth, kCamHeight);
      print_status(mic, cam, spk, disp);
      break;
    case 'i': {
      const videosdk::MeetingStats st = meeting.stats();
      emit("  audio tx=%llu rx=%llu | video tx=%llu rx=%llu render=%llu "
           "dropped=%llu\n",
           static_cast<unsigned long long>(st.audioFramesSent),
           static_cast<unsigned long long>(st.audioFramesPlayed),
           static_cast<unsigned long long>(st.videoFramesSent),
           static_cast<unsigned long long>(st.videoFramesReceived),
           static_cast<unsigned long long>(st.videoFramesRendered),
           static_cast<unsigned long long>(st.videoFramesDropped));
      break;
    }
    case 't': {
      // Temporarily restore cooked mode to read a full line, then send it.
      // Note: the mutex is taken only per emit() call, not across fgets() — we
      // must never block an SDK callback thread while waiting on user input.
      restore_terminal();
      emit("message> ");
      char line[1024];
      if (std::fgets(line, sizeof(line), stdin)) {
        std::string msg(line);
        if (!msg.empty() && msg.back() == '\n')
          msg.pop_back();
        if (!msg.empty())
          emit(meeting.sendData(msg.data(), msg.size(), /*binary=*/false)
                   ? "  [sent text] %s\n"
                   : "  [send failed] %s\n",
               msg.c_str());
      }
      raw_terminal();
      break;
    }
    case 'b': {
      // Demonstrate arbitrary binary: a 16-byte blob with non-UTF-8 bytes.
      unsigned char blob[16];
      for (int i = 0; i < 16; ++i)
        blob[i] = static_cast<unsigned char>(0xF0 + (i & 0x0F));
      bool ok = meeting.sendData(blob, sizeof(blob), /*binary=*/true);
      emit("  [%s] 16 binary bytes\n", ok ? "sent" : "send failed");
      break;
    }
    case 'h':
    case '?':
      print_help();
      break;
    case 'q':
      g_running.store(false);
      break;
    default:
      break;
    }
  }

  restore_terminal();
  emit("\nleaving ...\n");
  meeting.disableVideoDisplay();
  meeting.disableCamera();
  meeting.disableMic();
  meeting.disableSpeaker();
  meeting.leave();
  return 0;
}
