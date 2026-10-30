#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "videosdk/AudioFrame.hpp"
#include "videosdk/E2EEAlgorithm.hpp"
#include "videosdk/Export.hpp"
#include "videosdk/VideoFrame.hpp"

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
// Received data-channel message, shaped like sendData(). `data` is valid only
// during the call; `is_binary` is the sender's frame type (text is UTF-8).
using DataCallback =
    std::function<void(const uint8_t* data, size_t len, bool is_binary)>;
// Cloud lifecycle callbacks. Strings in all of them are valid only during the
// call. Recording: `status` goes RECORDING_STARTING → STARTED → STOPPING →
// STOPPED; Started/Stopped carry the backend JSON (recording id, S3 path).
using RecordingStateCallback = std::function<void(const std::string& status)>;
using RecordingEventCallback = std::function<void(const std::string& payload_json)>;

// HLS: HLS_STARTING → STARTED → STOPPING → STOPPED. PlayableStateChanged fires
// once the m3u8 is servable (~15-30 s after STARTED), with `playbackHlsUrl`.
using HlsStateCallback = std::function<void(const std::string& status)>;
using HlsEventCallback = std::function<void(const std::string& payload_json)>;

// RTMP livestream: LIVESTREAM_STARTING → STARTED → STOPPING → STOPPED.
using LivestreamStateCallback = std::function<void(const std::string& status)>;
using LivestreamEventCallback = std::function<void(const std::string& payload_json)>;

struct MeetingConfig {
    std::string meetingId;
    std::string token;
    std::string name = "VideoSDK";
    std::string participantId;
};

// One VP8 simulcast layer; pass 1–3 to setSimulcastLayers(), smallest first.
// Sizes are absolute (the top layer should match the capture); 0 = no cap.
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

class VIDEOSDK_API Meeting {
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
    // Recording events — fired on the SDK worker thread. Don't block > ~10 ms.
    void onRecordingStateChanged(RecordingStateCallback cb);
    void onRecordingStarted(RecordingEventCallback cb);
    void onRecordingStopped(RecordingEventCallback cb);
    // HLS events — same threading contract as recording.
    void onHlsStateChanged(HlsStateCallback cb);
    void onHlsStarted(HlsEventCallback cb);
    void onHlsStopped(HlsEventCallback cb);
    // Fires once HLS is playable; payload carries `playbackHlsUrl`.
    void onHlsPlayableStateChanged(HlsEventCallback cb);
    // RTMP livestream events — same threading contract.
    void onLivestreamStateChanged(LivestreamStateCallback cb);
    void onLivestreamStarted(LivestreamEventCallback cb);
    void onLivestreamStopped(LivestreamEventCallback cb);

    // Send any byte range over the data channel. `binary` picks the SCTP frame
    // type (false = UTF-8 text). False if over the 15 KiB limit or the data
    // producer isn't ready.
    bool sendData(const void* data, size_t len, bool binary = true);

    // ── Cloud recording / HLS / livestream ────────────────────────────────
    // All three run server-side, MUST be called after join(), and need a
    // token with recording permission (one shared subsystem). Empty strings
    // mean backend defaults. Mutually exclusive with E2EE.
    //
    // Recording: config/transcription JSON match videosdk::RecordingConfig /
    // PostTranscriptionConfig.
    bool startRecording(const std::string& webhook_url = "",
                        const std::string& aws_dir_path = "",
                        const std::string& config_json = "",
                        const std::string& transcription_json = "");

    bool stopRecording();

    // HLS: config JSON matches videosdk::HlsConfig. The m3u8 URL arrives via
    // onHlsPlayableStateChanged, ~15-30 s after HLS_STARTED.
    bool startHls(const std::string& config_json = "",
                  const std::string& transcription_json = "");

    bool stopHls();

    // Livestream: `outputs_json` is a required array of {url, streamKey} —
    // camelCase; `stream_key` is rejected:
    //   [{"url":"rtmp://a.rtmp.youtube.com/live2","streamKey":"xxxx-yyyy"}]
    bool startLivestream(const std::string& outputs_json,
                         const std::string& config_json = "");

    bool stopLivestream();

    // ── End-to-end encryption ─────────────────────────────────────────────
    // MUST be called BEFORE join(). Shared secret for the frame encryption
    // (PBKDF2); wire-compatible with iOS / Flutter / Android peers holding the
    // same secret. An empty key is unsupported — recreate the Meeting to turn
    // E2EE off.
    bool setE2EEKey(const uint8_t* key, size_t len);
    bool setE2EEKey(const std::string& password);  // convenience — bytes

    // Advisory today (always AES-GCM). Call before setE2EEKey().
    void setE2EEAlgorithm(E2EEAlgorithm algorithm);

    // One PBKDF2 ratchet step, safe mid-meeting: peers catch up within the
    // 16-epoch receive window. False if no key was set.
    bool ratchetE2EEKey();

    bool join();
    void leave();

    // ── Tracing ───────────────────────────────────────────────────────────
    // Chrome trace-format JSON; see videosdk/Trace.hpp. False if the path is
    // empty or the core was built without the `chrome-trace` feature.
    bool startTracing(const std::string& path);
    void stopTracing();

    // ── App-owned publish ─────────────────────────────────────────────────
    // I420 frame; the core copies it, so the buffer need only outlive the
    // call. Strides are bytes per row. NOT_JOINED (before join completes) is
    // benign and returns true.
    bool publishVideoFrame(const uint8_t* y,
                           const uint8_t* u,
                           const uint8_t* v,
                           int y_stride,
                           int u_stride,
                           int v_stride,
                           int width,
                           int height,
                           int64_t timestamp_us);

    // S16LE interleaved PCM. Same contract as publishVideoFrame.
    bool publishAudioFrame(const int16_t* pcm,
                           uint32_t samples_per_channel,
                           uint32_t sample_rate,
                           uint32_t channels);

    // ── App-owned receive ─────────────────────────────────────────────────
    // One sink per stream type, last writer wins; empty function unsubscribes.
    // Fires on a runtime worker — don't block (>10 ms backs up the pipeline).
    // Frame pointers are valid only during the call.
    void onRemoteVideoFrame(std::function<void(const VideoFrame&)> cb);
    void onRemoteAudioFrame(std::function<void(const AudioFrame&)> cb);

    // ── Built-in devices ──────────────────────────────────────────────────
    // The Meeting opens and owns the platform's mic, speaker, camera and video
    // window. Apps doing their own capture or rendering use publish*/onRemote*
    // or the adapters in videosdk/adapters/ instead.

    bool enableMic(const std::string& source_name = "default");
    // Unpublishes, so the other participants see the mic turn off. Also ends
    // audio sent with publishAudioFrame(); stop pushing frames first.
    void disableMic();

    bool enableSpeaker();
    void disableSpeaker();

    bool enableCamera(const std::string& device_path = "/dev/video0",
                      int width = 640,
                      int height = 480,
                      int fps = 30);
    // Unpublishes, so the other participants see the camera turn off. Also
    // ends video sent with publishVideoFrame(); stop pushing frames first.
    void disableCamera();

    void setVideoMaxBitrate(uint32_t kbps);

    // 1–3 VP8 layers, smallest first. Call before join(). The top layer
    // should match the resolution you actually publish.
    void setSimulcastLayers(const std::vector<SimulcastLayer>& layers);

    [[deprecated("use onRemoteVideoFrame")]]
    bool enableVideoSubscriber(VideoFrameCallback cb);
    void disableVideoSubscriber();

    bool enableVideoDisplay(int width_hint = 1280,
                            int height_hint = 720,
                            bool force_fullscreen = false);
    void disableVideoDisplay();

    /// True while the video window is up. Query it rather than tracking a bool:
    /// the user can close the window from its title bar.
    bool isVideoDisplayEnabled() const;

    MeetingStats stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace videosdk
