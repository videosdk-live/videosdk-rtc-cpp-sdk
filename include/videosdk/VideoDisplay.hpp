#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace videosdk {

struct VideoFrame;
class Meeting;

/// Renders decoded I420 frames to an SDL2 window. Frames land in one
/// latest-wins slot, so a slow renderer drops frames rather than lagging.
/// Rendered by its own thread on Linux and Windows, by runEventLoop() on macOS.
///
/// The Meeting& constructor auto-subscribes in start(); the other is
/// standalone — feed it via submitFrame(). KMSDRM (Pi OS Lite) forces
/// fullscreen automatically.
class VideoDisplay {
public:
    VideoDisplay(int initial_w, int initial_h, bool force_fullscreen = false);
    VideoDisplay(Meeting& meeting,
                 int initial_w,
                 int initial_h,
                 bool force_fullscreen = false);
    ~VideoDisplay();

    VideoDisplay(const VideoDisplay&) = delete;
    VideoDisplay& operator=(const VideoDisplay&) = delete;

    /// Create the window and renderer. False on any init failure.
    bool start();

    /// Destroy the window and release SDL's video subsystem. Idempotent.
    void stop();

    /// Copy one decoded frame into the latest-wins slot (strips stride
    /// padding).
    void submitFrame(const VideoFrame& f);

    /// Called once when the user closes the window. Rendering stops at once;
    /// this is where the owner tears the display down. Runs on the event-loop
    /// thread after the pump has unwound, so destroying the display here is
    /// safe. Needs runEventLoop() — without it rendering just stops.
    void onCloseRequested(std::function<void()> cb);

    /// True once the user has closed the window.
    bool closeRequested() const { return close_requested_.load(); }

    uint64_t framesRendered() const { return frames_rendered_.load(); }
    uint64_t framesDropped()  const { return frames_dropped_.load(); }

private:
    void renderLoop();
    bool initSDL();
    void shutdownSDL();
    void ensureTexture(int w, int h);
    // Upload and present one packed I420 frame (both platforms).
    void drawI420(const uint8_t* packed, int w, int h);
    // Drain SDL events; true if the user closed this window.
    bool pollEventsForClose();
    // Latch the close and defer teardown to the event loop.
    void dispatchCloseRequest();
#if defined(__APPLE__)
    // One non-blocking iteration, driven by runEventLoop() because Cocoa only
    // allows it on the main thread. render_thread_ is unused on macOS.
    void pumpOnce();
#endif

    Meeting* meeting_wrapper_{nullptr};
    int initial_w_;
    int initial_h_;
    bool force_fullscreen_;

    // SDL2 handles. Opaque to keep SDL out of this header.
    void* sdl_window_{nullptr};
    void* sdl_renderer_{nullptr};
    void* sdl_texture_{nullptr};
    int tex_w_{0};
    int tex_h_{0};

    std::mutex frame_mu_;
    std::condition_variable frame_cv_;
    std::vector<uint8_t> pending_;
    int pending_w_{0};
    int pending_h_{0};
    bool has_pending_{false};
    bool stop_requested_{false};

    // Per-instance swap buffer (a shared thread_local would rotate between
    // displays and defeat the reuse).
    std::vector<uint8_t> scratch_;

    std::thread render_thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> frames_rendered_{0};
    std::atomic<uint64_t> frames_dropped_{0};

    // close_dispatched_ keeps the callback strictly once-only.
    std::function<void()> on_close_;
    std::atomic<bool> close_requested_{false};
    std::atomic<bool> close_dispatched_{false};
};

}  // namespace videosdk
