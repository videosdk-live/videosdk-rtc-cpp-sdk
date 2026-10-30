#pragma once

#include <functional>

namespace videosdk {

/// Run the SDK's main-thread event loop. Blocks until `stopEventLoop()`.
///
/// **Call this from `main()`, and only from `main()`.** On macOS it is not
/// optional: SDL and Cocoa allow a window to be created and pumped only on the
/// main thread, so `enableVideoDisplay()` has nowhere to draw until this runs.
/// On Linux and Windows the display owns a render thread and this just parks,
/// so the same program shape works everywhere:
///
/// ```cpp
///     std::thread ui(console_loop, &meeting);
///     videosdk::runEventLoop();   // returns on stopEventLoop()
///     ui.join();
/// ```
void runEventLoop();

/// Ask `runEventLoop()` to return. Any thread, idempotent, and safe to call
/// before the loop starts — the request is remembered.
///
/// **Not async-signal-safe**: it takes a mutex. From a signal handler, set a
/// `volatile sig_atomic_t` flag and call this from normal thread context.
void stopEventLoop();

/// True while `runEventLoop()` is executing.
bool eventLoopRunning();

namespace detail {

// Internal plumbing — not part of the supported API.

/// Run `fn` on the event-loop thread and wait. Runs inline if we are already
/// there, or if no loop is running (so enabling the display from `main()`
/// before `runEventLoop()` doesn't deadlock).
void runOnMainThread(std::function<void()> fn);

/// Queue `fn` and return immediately; never runs it inline. For work found
/// *inside* a pump or render thread that must not run until that caller has
/// unwound — tearing down a display from its own pump would destroy the object
/// mid-method. Warns and drops if no loop is running.
void postToMainThread(std::function<void()> fn);

/// Callback invoked every loop iteration (~120 Hz). Re-registering a key
/// replaces the previous callback.
void registerMainPump(const void* key, std::function<void()> pump);
void unregisterMainPump(const void* key);

}  // namespace detail

}  // namespace videosdk
