#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace videosdk {

enum class ErrorCode {
    Success = 0,
    InvalidConfig = 1,
    InvalidArgument = 2,
    ConnectionFailed = 3,
    SignalingFailed = 4,
    NotJoined = 5,
    AlreadyJoined = 6,
    SendFailed = 7,
    InternalError = 8,
    Panic = 9,
};

using ErrorCallback =
    std::function<void(ErrorCode code, const std::string& message)>;
using ParticipantJoinedCallback =
    std::function<void(const std::string& participant_id,
                       const std::string& name)>;
using ParticipantLeftCallback =
    std::function<void(const std::string& participant_id)>;
// Received data-channel message. `data`/`len` are valid only for the duration
// of the callback (copy out to keep them). `is_binary` mirrors the sender's
// frame flag; for text the bytes are UTF-8.
using DataCallback =
    std::function<void(const uint8_t* data, size_t len, bool is_binary)>;

struct MeetingConfig {
    std::string meetingId;
    std::string token;
    std::string name = "VideoSDK";
    std::string participantId;
};

// One VP8 simulcast layer. Pass 1–3 to setSimulcastLayers(), ordered
// smallest → largest; the top layer's resolution should match the camera
// capture size. A bitrate/fps of 0 means no explicit cap for that layer.
struct SimulcastLayer {
    uint32_t width;
    uint32_t height;
    uint32_t maxBitrateKbps;
    uint32_t maxFps;
    bool active = true;
};

struct MeetingStats {
    uint64_t audioFramesSent = 0;
    uint64_t audioFramesPlayed = 0;
    uint64_t videoFramesSent = 0;
    uint64_t videoFramesReceived = 0;
    uint64_t videoFramesRendered = 0;
    uint64_t videoFramesDropped = 0;
};

class Meeting {
public:
    Meeting(std::string meeting_id, std::string token);
    explicit Meeting(const MeetingConfig& config);
    ~Meeting();

    Meeting(const Meeting&) = delete;
    Meeting& operator=(const Meeting&) = delete;

    void onError(ErrorCallback cb);
    void onParticipantJoined(ParticipantJoinedCallback cb);
    void onParticipantLeft(ParticipantLeftCallback cb);
    void onData(DataCallback cb);

    // Send a payload to other peers over the data channel. `data`/`len` is an
    // opaque byte range; `binary` selects the frame type (false = UTF-8 text,
    // true = arbitrary bytes). Returns false if the payload exceeds 15 KiB or
    // the data channel isn't ready.
    bool sendData(const void* data, size_t len, bool binary = true);

    bool join();
    void leave();

    bool enableMic(const std::string& source_name = "default");
    void disableMic();

    bool enableSpeaker();
    void disableSpeaker();

    bool enableCamera(const std::string& device_path = "/dev/video0",
                      int width = 640,
                      int height = 480,
                      int fps = 30);
    void disableCamera();

    void setVideoMaxBitrate(uint32_t kbps);

    // Configure 1–3 VP8 simulcast layers for the outgoing camera video.
    // Must be called before join(). The top layer's resolution should match
    // the enableCamera() capture size.
    void setSimulcastLayers(const std::vector<SimulcastLayer>& layers);

    bool enableVideoDisplay(int width_hint = 1280,
                            int height_hint = 720,
                            bool force_fullscreen = false);
    void disableVideoDisplay();

    MeetingStats stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace videosdk
