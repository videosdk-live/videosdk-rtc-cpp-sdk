// Audio/video console example.
//
// Usage: main [meeting_id] [token] [audio_source] [video_device]
//   meeting_id / token : or $VIDEOSDK_MEETING_ID / $VIDEOSDK_TOKEN
//   audio_source       : PulseAudio source (Linux) or mic-name substring (Windows)
//   video_device       : /dev/videoN (Linux, autodetected by default) or camera
//                        index / name substring (Windows, default 0)
// Device args are ignored on macOS. $VIDEOSDK_E2EE_KEY turns on E2EE.

#include <videosdk/videosdk.hpp>

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <conio.h>
#else
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#endif

#if defined(__linux__)
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#endif

#if defined(_MSC_VER)
#define VSDK_PRINTF_FMT(a, b)
#else
#define VSDK_PRINTF_FMT(a, b) __attribute__((format(printf, a, b)))
#endif

namespace {

// One profile everywhere. The UNO Q (software VP8) is the tightest fit.
constexpr int kCamWidth = 1280;
constexpr int kCamHeight = 720;
constexpr int kCamFps = 30;

std::atomic<bool> g_running{true};
// Set once before the console thread starts.
std::atomic<bool> g_e2ee{false};
#if !defined(_WIN32)
termios g_saved_term{};
bool g_term_saved = false;
#endif
std::mutex g_console_mutex;

#if defined(__linux__)
// ── V4L2 camera auto-detection (Linux only) ───────────────────────────────
int xioctl(int fd, unsigned long request, void *arg) {
  int r;
  do {
    r = ioctl(fd, request, arg);
  } while (r == -1 && errno == EINTR);
  return r;
}

// Formats the SDK converts. Mirrors kCandidates in src/VideoTrack.cpp.
bool is_supported_format(uint32_t fourcc) {
  switch (fourcc) {
  case V4L2_PIX_FMT_MJPEG:
  case V4L2_PIX_FMT_NV12:
  case V4L2_PIX_FMT_YUV420:
  case V4L2_PIX_FMT_YUYV:
  case V4L2_PIX_FMT_UYVY:
  case V4L2_PIX_FMT_RGB24:
  case V4L2_PIX_FMT_BGR24:
    return true;
  default:
    return false;
  }
}

// A capture node offering one of those — not only MJPEG, since laptop cams
// often offer just YUYV. Metadata nodes (Pi bcm2835, UNO Q Venus) offer no
// formats, and CSI nodes (Pi unicam) offer only raw Bayer; taking either would
// pass over the USB camera sitting on the next node.
bool is_usable_camera(const char *path) {
  int fd = ::open(path, O_RDWR | O_NONBLOCK);
  if (fd < 0) return false;

  v4l2_capability cap{};
  bool ok = false;
  if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == 0 &&
      (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
      (cap.capabilities & V4L2_CAP_STREAMING)) {
    for (uint32_t i = 0; !ok; ++i) {
      v4l2_fmtdesc fmt{};
      fmt.index = i;
      fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      if (xioctl(fd, VIDIOC_ENUM_FMT, &fmt) != 0) break;
      ok = is_supported_format(fmt.pixelformat);
    }
  }
  ::close(fd);
  return ok;
}

// First usable /dev/videoN, or empty.
std::string autodetect_camera() {
  for (int i = 0; i < 64; ++i) {
    char path[32];
    std::snprintf(path, sizeof(path), "/dev/video%d", i);
    if (is_usable_camera(path)) return path;
  }
  return std::string();
}
#endif  // __linux__

const char *env_or(const char *name, const char *fallback) {
  const char *v = std::getenv(name);
  return (v && v[0]) ? v : fallback;
}

#if defined(_WIN32)
// _getch() already reads keys unbuffered and unechoed; nothing to set up.
void restore_terminal() {}
void raw_terminal() {}

// Runs on a console control thread, so only flag it; the loop polls every 50 ms.
// Note: CTRL_CLOSE_EVENT ends the process soon after this returns, so
// closing the window may skip leave(). Ctrl+C is clean.
BOOL WINAPI console_ctrl_handler(DWORD type) {
  switch (type) {
  case CTRL_C_EVENT:
  case CTRL_BREAK_EVENT:
  case CTRL_CLOSE_EVENT:
    g_running.store(false);
    return TRUE;
  default:
    return FALSE;
  }
}

void install_signal_handlers() {
  SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
}
#else
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

// No SA_RESTART (glibc's std::signal sets it): the signal must interrupt the
// console's read() with EINTR, or Ctrl-C waits for a keypress. The handler only
// sets an atomic — stopEventLoop() takes a mutex and isn't async-signal-safe.
void install_signal_handlers() {
  struct sigaction sa {};
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
}
#endif  // _WIN32

// Serialized output: SDK callbacks fire on their own threads and would
// otherwise splice into the console's lines.
VSDK_PRINTF_FMT(1, 2) void emit(const char *fmt, ...) {
  std::lock_guard<std::mutex> lock(g_console_mutex);
  va_list ap;
  va_start(ap, fmt);
  std::vprintf(fmt, ap);
  va_end(ap);
  std::fflush(stdout);
}

void print_help() {
  emit("\n  m  mic        c  camera      s  speaker\n"
       "  d  display    i  stats       h  help       q  quit\n");
  if (g_e2ee.load()) {
    emit("  e  ratchet the E2EE key one step\n");
  }
  emit("\n");
}

void print_status(bool mic, bool cam, bool spk, bool disp) {
  emit("  [mic %s] [camera %s] [speaker %s] [display %s]\n",
       mic ? "on" : "off", cam ? "on" : "off", spk ? "on" : "off",
       disp ? "on" : "off");
}

// Keyboard console, on its own thread: main belongs to runEventLoop().
void console_loop(videosdk::Meeting *meeting, std::string audio_src,
                  std::string video_dev, bool mic, bool cam, bool spk,
                  bool disp) {
#if !defined(_WIN32)
  // Main blocked these; take them here so they interrupt our read().
  sigset_t sigs;
  sigemptyset(&sigs);
  sigaddset(&sigs, SIGINT);
  sigaddset(&sigs, SIGTERM);
  pthread_sigmask(SIG_UNBLOCK, &sigs, nullptr);
#endif

  while (g_running.load()) {
    char ch = 0;
#if defined(_WIN32)
    // _getch() can't be interrupted, so poll; Ctrl+C is seen within 50 ms.
    if (!_kbhit()) {
      Sleep(50);
      continue;
    }
    ch = static_cast<char>(_getch());
#else
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n < 0) {
      if (errno == EINTR)
        continue;  // interrupted by a signal — re-check g_running and loop
      break;       // genuine read error
    }
    if (n == 0)
      break;       // EOF: stdin closed / non-interactive
#endif

    switch (ch) {
    // ── Audio ──
    case 'm':  // toggle mic capture
      if (mic) {
        meeting->disableMic();
        mic = false;
      } else {
        mic = meeting->enableMic(audio_src);
      }
      print_status(mic, cam, spk, disp);
      break;
    case 's':  // toggle remote-audio playback
      if (spk) {
        meeting->disableSpeaker();
        spk = false;
      } else {
        spk = meeting->enableSpeaker();
      }
      print_status(mic, cam, spk, disp);
      break;
    // ── Video ──
    case 'c':  // toggle camera capture
      if (cam) {
        meeting->disableCamera();
        cam = false;
      } else {
        cam = meeting->enableCamera(video_dev, kCamWidth, kCamHeight, kCamFps);
      }
      print_status(mic, cam, spk, disp);
      break;
    case 'd':  // toggle remote-video display window
      // The window may have been closed from its title bar.
      disp = meeting->isVideoDisplayEnabled();
      if (disp) {
        meeting->disableVideoDisplay();
        disp = false;
      } else {
        disp = meeting->enableVideoDisplay(kCamWidth, kCamHeight);
      }
      print_status(mic, cam, spk, disp);
      break;
    case 'i': {  // meeting stats
      disp = meeting->isVideoDisplayEnabled();
      const videosdk::MeetingStats st = meeting->stats();
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
    case 'e':  // advance the E2EE key one PBKDF2 step
      if (!g_e2ee.load()) {
        emit("  E2EE is not enabled — set $VIDEOSDK_E2EE_KEY before starting\n");
      } else if (meeting->ratchetE2EEKey()) {
        // Peers catch up within the 16-epoch window.
        emit("  E2EE key ratcheted one step\n");
      } else {
        emit("  E2EE ratchet failed\n");
      }
      break;
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

  // 'q', EOF or a signal: release main from runEventLoop().
  g_running.store(false);
  videosdk::stopEventLoop();
}

} // namespace

int main(int argc, char **argv) {
#if !defined(_WIN32)
  // Block these before ANY thread exists — threads inherit the mask, and
  // join() spawns the runtime pool. Otherwise Ctrl-C can land on a Tokio
  // worker and never interrupt the console's read().
  sigset_t sigs;
  sigemptyset(&sigs);
  sigaddset(&sigs, SIGINT);
  sigaddset(&sigs, SIGTERM);
  pthread_sigmask(SIG_BLOCK, &sigs, nullptr);
#endif

  const std::string meeting_id =
      (argc >= 2) ? argv[1] : env_or("VIDEOSDK_MEETING_ID", "");
  const std::string token =
      (argc >= 3) ? argv[2] : env_or("VIDEOSDK_TOKEN", "");
  const std::string audio_src =
      (argc >= 4) ? argv[3] : env_or("PULSE_SOURCE", "default");

  // Camera: 4th arg > $VIDEOSDK_V4L2_DEVICE > autodetect > /dev/video0.
  // Unused on macOS; on Windows "" is camera index 0.
  std::string video_dev;
  if (argc >= 5) {
    video_dev = argv[4];
  } else if (const char *env = std::getenv("VIDEOSDK_V4L2_DEVICE"); env && env[0]) {
    video_dev = env;
  } else {
#if defined(__APPLE__) || defined(_WIN32)
    video_dev = "";  // default camera
#else
    video_dev = autodetect_camera();
    if (video_dev.empty()) {
      emit("no usable V4L2 camera detected under /dev/video*, "
           "falling back to /dev/video0\n");
      video_dev = "/dev/video0";
    } else {
      emit("autodetected camera: %s\n", video_dev.c_str());
    }
#endif
  }

  if (meeting_id.empty() || token.empty()) {
    // argv[0] can be NULL under a bare execve.
    const char *prog = (argc > 0 && argv[0]) ? argv[0] : "main";
    std::fprintf(stderr,
                 "set VIDEOSDK_TOKEN and VIDEOSDK_MEETING_ID, or run:\n"
                 "  %s <meeting_id> <token> [audio_source] [video_device]\n",
                 prog);
    return 1;
  }

  videosdk::MeetingConfig config;
  config.meetingId = meeting_id;
  config.token = token;
  config.name = "VideoSDK C++";
  videosdk::Meeting meeting(config);

  // ── Events (SDK threads, so through emit) ─────────────────────────────
  meeting.onParticipantJoined([](const std::string &id, const std::string &name) {
    emit("\r* %s joined (%s)\n", name.c_str(), id.c_str());
  });
  meeting.onParticipantLeft([](const std::string &id) {
    emit("\r* %s left\n", id.c_str());
  });
  meeting.onError([](videosdk::ErrorCode, const std::string &msg) {
    // Same lock, so errors don't splice into stdout.
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::fprintf(stderr, "\r! error: %s\n", msg.c_str());
    std::fflush(stderr);
  });

  // ── Simulcast ─────────────────────────────────────────────────────────
  // Three layers so the media server can serve each subscriber what its downlink fits.
  // The top layer matches the capture size; the media engine downscales the rest.
  // {width, height, maxBitrateKbps, maxFps} — positional, as MSVC's C++17
  // rejects designated initializers.
  meeting.setSimulcastLayers({
      {kCamWidth / 4, kCamHeight / 4, 300, 15},   // q: 320x180
      {kCamWidth / 2, kCamHeight / 2, 1500, 20},  // h: 640x360
      {kCamWidth, kCamHeight, 3000, 30},          // f: 1280x720
  });

  // ── E2EE (opt-in via $VIDEOSDK_E2EE_KEY) ──────────────────────────────
  // MUST precede join(), or the first frames leave unencrypted. Peers with a
  // different key stay connected but can't decode each other — that's the
  // test that it's really on.
  if (const char *e2ee_key = std::getenv("VIDEOSDK_E2EE_KEY");
      e2ee_key && e2ee_key[0]) {
    meeting.setE2EEAlgorithm(videosdk::E2EEAlgorithm::VsdkAesGcm128);
    if (meeting.setE2EEKey(std::string(e2ee_key))) {
      g_e2ee.store(true);
      emit("E2EE enabled (AES-GCM-128, shared key from $VIDEOSDK_E2EE_KEY)\n");
    } else {
      // Never fall back to plaintext after encryption was requested.
      std::fprintf(stderr,
                   "failed to install the E2EE key — refusing to join "
                   "unencrypted\n");
      return 3;
    }
  }

  // Before join(), so Ctrl-C mid-connect shuts down cleanly.
  install_signal_handlers();

  emit("joining %s ...\n", meeting_id.c_str());
  if (!meeting.join()) {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::fprintf(stderr, "join failed\n");
    return 2;
  }

  // A signal during join(): skip the devices and leave.
  if (!g_running.load()) {
    meeting.leave();
    return 0;
  }
  emit("joined as \"%s\".\n", config.name.c_str());

  // On main before the loop, so the window is created inline; later toggles
  // from the console are marshalled to the loop.
  bool spk = meeting.enableSpeaker();
  bool mic = meeting.enableMic(audio_src);
  bool cam = meeting.enableCamera(video_dev, kCamWidth, kCamHeight, kCamFps);
  bool disp = meeting.enableVideoDisplay(kCamWidth, kCamHeight);
  print_status(mic, cam, spk, disp);
  print_help();

  raw_terminal();
  // Backstop: restore the terminal on every exit path.
  std::atexit(restore_terminal);

  std::thread console(console_loop, &meeting, audio_src, video_dev, mic, cam,
                      spk, disp);

  // Services the video window on macOS; just waits on Linux and Windows.
  videosdk::runEventLoop();

  if (console.joinable()) console.join();

#if defined(_WIN32)
  // Drop our Ctrl+C handler (it swallows the event) so teardown stays killable.
  SetConsoleCtrlHandler(console_ctrl_handler, FALSE);
#else
  // The console was the only thread taking these; with it gone they're blocked
  // process-wide. Restore defaults so a wedged teardown stays killable.
  std::signal(SIGINT, SIG_DFL);
  std::signal(SIGTERM, SIG_DFL);
  pthread_sigmask(SIG_UNBLOCK, &sigs, nullptr);
#endif

  restore_terminal();
  emit("\nleaving ...\n");
  meeting.disableVideoDisplay();
  meeting.disableCamera();
  meeting.disableMic();
  meeting.disableSpeaker();
  meeting.leave();
  return 0;
}
