#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "videosdk/AudioFrame.hpp"
#include "videosdk_ffi.h"

struct pa_simple;

namespace videosdk {

class Meeting;

/// Plays decoded remote PCM through the default output (PulseAudio on Linux,
/// WASAPI on Windows, CoreAudio on macOS). Decoding and jitter smoothing are built in.
///
/// `onAudioFrame` runs on a runtime worker and only copies into a bounded
/// FIFO; a dedicated thread does the blocking device write. Keeping that write
/// off the worker avoids underruns and guarantees a single writer.
///
/// `AudioPlayer(Meeting&)` auto-subscribes in start(). `AudioPlayer()` is
/// standalone — feed it via onAudioFrame().
class AudioPlayer {
public:
    static constexpr int kSampleRate = 48000;
    static constexpr int kChannels = 1;          // matches AUDIO_CHANNELS in peer.rs

    AudioPlayer();
    explicit AudioPlayer(Meeting& meeting);
    ~AudioPlayer();

    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    /// Open the output and start playback; registers as the meeting's sink if
    /// constructed with one.
    bool start();

    /// Stop playback and release the device. Idempotent.
    void stop();

    /// Make this the bridge's target (legacy standalone path).
    void bindAsActive();

    uint64_t framesPlayed() const { return frames_played_.load(); }
    uint64_t samplesWritten() const { return samples_written_.load(); }

    /// Feed one decoded frame. Copies and returns; never blocks on the device.
    void onAudioFrame(const VsdkAudioFrame* frame);
    void onAudioFrame(const AudioFrame& frame);

private:
    /// Drains the FIFO into the blocking device write.
    void playbackLoop();

    Meeting* meeting_wrapper_{nullptr};
    [[maybe_unused]] pa_simple* pa_handle_{nullptr};
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> frames_played_{0};
    std::atomic<uint64_t> samples_written_{0};

    // Bounded FIFO: runtime worker -> playback thread (sole device writer).
    std::thread playback_thread_;
    std::mutex buf_mu_;
    std::condition_variable buf_cv_;
    std::deque<std::vector<int16_t>> queue_;
    [[maybe_unused]] bool stop_requested_{false};
    std::atomic<uint64_t> frames_dropped_{0};

#if defined(__APPLE__) || defined(_WIN32)
    // Darwin / Windows backend state (src/platform/<os>/). Guarded to leave the
    // Linux layout untouched.
    void* impl_{nullptr};
#endif
};

/// C ABI bridge: routes a user_data-less callback to the active player.
extern "C" void videosdk_audio_player_trampoline(const VsdkAudioFrame* frame);

}  // namespace videosdk
