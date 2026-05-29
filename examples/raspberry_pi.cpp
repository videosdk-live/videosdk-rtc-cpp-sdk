// Interactive audio/video console for Raspberry Pi and other Linux devices.
// Toggle each media path at runtime with single keypresses.
// Usage: raspberry_pi [meeting_id] [token] [pulse_source] [v4l2_device]

#include <videosdk/videosdk.hpp>

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
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

void print_help() {
  std::printf("\n  m  mic        c  camera      s  speaker\n"
              "  d  display    i  stats       h  help       q  quit\n\n");
}

void print_status(bool mic, bool cam, bool spk, bool disp) {
  std::printf("  [mic %s] [camera %s] [speaker %s] [display %s]\n",
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
    std::fprintf(stderr,
                 "set VIDEOSDK_TOKEN and VIDEOSDK_MEETING_ID, or run:\n"
                 "  %s <meeting_id> <token> [pulse_source] [v4l2_device]\n",
                 argv[0]);
    return 1;
  }

  videosdk::MeetingConfig config;
  config.meetingId = meeting_id;
  config.token = token;
  config.name = "Raspberry Pi";
  videosdk::Meeting meeting(config);

  meeting.onParticipantJoined([](const std::string &id, const std::string &name) {
    std::printf("\r* %s joined (%s)\n", name.c_str(), id.c_str());
  });
  meeting.onParticipantLeft([](const std::string &id) {
    std::printf("\r* %s left\n", id.c_str());
  });
  meeting.onError([](videosdk::ErrorCode, const std::string &msg) {
    std::fprintf(stderr, "\r! error: %s\n", msg.c_str());
  });

  meeting.setVideoMaxBitrate(500);

  std::printf("joining %s ...\n", meeting_id.c_str());
  if (!meeting.join()) {
    std::fprintf(stderr, "join failed\n");
    return 2;
  }
  std::printf("joined as \"%s\".\n", config.name.c_str());

  bool spk = meeting.enableSpeaker();
  bool mic = meeting.enableMic(pulse_src);
  bool cam = meeting.enableCamera(v4l_dev, kCamWidth, kCamHeight, kCamFps);
  bool disp = meeting.enableVideoDisplay(kCamWidth, kCamHeight);
  print_status(mic, cam, spk, disp);
  print_help();

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  raw_terminal();

  while (g_running.load()) {
    char ch = 0;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n <= 0) {
      if (errno == EINTR)
        continue;
      break;
    }

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
      std::printf("  audio tx=%llu rx=%llu | video tx=%llu rx=%llu render=%llu\n",
                  static_cast<unsigned long long>(st.audioFramesSent),
                  static_cast<unsigned long long>(st.audioFramesPlayed),
                  static_cast<unsigned long long>(st.videoFramesSent),
                  static_cast<unsigned long long>(st.videoFramesReceived),
                  static_cast<unsigned long long>(st.videoFramesRendered));
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
  std::printf("\nleaving ...\n");
  meeting.disableVideoDisplay();
  meeting.disableCamera();
  meeting.disableMic();
  meeting.disableSpeaker();
  meeting.leave();
  return 0;
}
