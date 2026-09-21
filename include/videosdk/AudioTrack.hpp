#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include "videosdk_ffi.h"

// Forward-declare PulseAudio's opaque handle so consumers don't need PA headers.
struct pa_simple;

namespace videosdk {

class Meeting;

/// Captures 16-bit mic PCM (PulseAudio on Linux, WASAPI on Windows, AVFoundation
/// on macOS) and pushes it to the SDK core. No encoding here: the media engine encodes,
/// and audio processing (echo cancellation, noise suppression, AGC) runs inside the SDK.
class AudioTrack {
public:
    static constexpr int kSampleRate = 48000;
    static constexpr int kChannels = 1;
    static constexpr int kFrameSamples = 480;   // 10 ms @ 48 kHz — the media engine's
                                                // canonical processing/encoder period.

    /// Preferred constructor. Push path goes through Meeting::publishAudioFrame
    /// so there is a single, typed publish surface.
    AudioTrack(Meeting& meeting, std::string source_name);

    /// Legacy constructor. Prefer the
    /// Meeting& overload above.
    AudioTrack(VsdkMeeting* meeting, std::string source_name);

    ~AudioTrack();

    AudioTrack(const AudioTrack&) = delete;
    AudioTrack& operator=(const AudioTrack&) = delete;

    /// Open the mic and start the capture thread.
    /// Returns false on any failure (logged to stderr).
    bool start();

    /// Stop and join the capture thread, release the mic. Idempotent.
    void stop();

    /// Number of PCM frames (10 ms each) pushed to the SDK core since start().
    uint64_t framesSent() const { return frames_sent_.load(); }

    /// Total PCM samples pushed (per-channel count × channels).
    uint64_t samplesSent() const { return samples_sent_.load(); }

private:
    void captureLoop();

    Meeting* meeting_wrapper_{nullptr};   // preferred path (public API)
    VsdkMeeting* meeting_ffi_{nullptr};   // legacy fallback
    std::string source_name_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> frames_sent_{0};
    std::atomic<uint64_t> samples_sent_{0};

    pa_simple* pa_handle_{nullptr};

#if defined(__APPLE__) || defined(_WIN32)
    // Darwin / Windows backend state (src/platform/<os>/). Guarded to leave the
    // Linux layout untouched.
    void* impl_{nullptr};
#endif
};

}  // namespace videosdk
