#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

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

struct MeetingConfig {
    std::string meetingId;
    std::string token;
    std::string name = "VideoSDK";
    std::string participantId;
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
