#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

#include "videosdk_ffi.h"
#include "videosdk/VideoFrame.hpp"

namespace videosdk {

class VideoDisplay;

/// Sits behind the SDK `vsdk_meeting_on_video` callback. Receives
/// **already-decoded I420 frames** from the media engine and fans them out to the
/// user callback and/or attached display. No codec work — the media engine owns
/// the VP8/VP9/H264 decode + jitter buffer.
///
/// One instance per session — the active instance is dispatched to by a
/// static bridge (same pattern as AudioPlayer).
class VideoSubscriber {
public:
    VideoSubscriber();
    ~VideoSubscriber();

    VideoSubscriber(const VideoSubscriber&) = delete;
    VideoSubscriber& operator=(const VideoSubscriber&) = delete;

    /// No-op except for flipping running_ on. Kept for API parity with the
    /// old VP8-decoder version.
    bool start();

    /// Mark inactive and clear the global active pointer if bound.
    void stop();

    /// Register this instance as the active sink for the C bridge.
    void bindAsActive();

    /// Replace the user-supplied callback. Pass nullptr to disable. Thread-safe.
    void setUserCallback(VideoFrameCallback cb);

    /// Attach a display (nullptr detaches). Shared ownership, because
    /// onDecodedFrame calls into it after releasing the lock — a raw pointer
    /// could be freed by a concurrent detach mid-call.
    void setDisplay(std::shared_ptr<VideoDisplay> display);

    bool hasUserCallback() const;
    bool hasDisplay() const;

    /// Called by the C bridge on each inbound (already-decoded) I420
    /// frame. Fans out to user callback and display.
    void onDecodedFrame(const VsdkVideoFrame* frame);

    uint64_t framesReceived() const { return frames_received_.load(); }
    /// Kept for source-compat with code that read these counters; with
    /// this backend the decode happens inside the SDK so received == decoded.
    uint64_t framesDecoded()  const { return frames_received_.load(); }
    uint64_t decodeErrors()   const { return 0; }

private:
    void unbindIfActive();

    mutable std::mutex sinks_mu_;
    VideoFrameCallback callback_;
    std::shared_ptr<VideoDisplay> display_;

    std::atomic<bool> running_{false};
    std::atomic<uint64_t> frames_received_{0};
};

}  // namespace videosdk

/// Plain-C bridge registered with `vsdk_meeting_on_video`. Forwards
/// each decoded I420 frame to the currently bound `VideoSubscriber`.
extern "C" void videosdk_video_subscriber_trampoline(const VsdkVideoFrame* frame);
