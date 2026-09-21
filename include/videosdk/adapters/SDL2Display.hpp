#pragma once

// App-owned display adapter: subscribes to Meeting::onRemoteVideoFrame in
// start() and blits each decoded I420 frame to an SDL2 window (latest wins).
// On macOS the app must also call videosdk::runEventLoop() from main().
//
//   videosdk::adapters::SDL2Display disp(meeting, 1280, 720);
//   disp.start();  /* ... */  disp.stop();

#include "videosdk/VideoDisplay.hpp"

namespace videosdk::adapters {

using SDL2Display = ::videosdk::VideoDisplay;

}  // namespace videosdk::adapters
