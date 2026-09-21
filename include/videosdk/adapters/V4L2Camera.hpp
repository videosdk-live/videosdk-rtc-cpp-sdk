#pragma once

// App-owned camera adapter: publishes I420 frames via Meeting::publishVideoFrame.
// V4L2 on Linux, Media Foundation on Windows (device = index or name substring),
// AVFoundation on macOS (device ignored).
//
//   videosdk::adapters::V4L2Camera cam(meeting, "/dev/video0", 1280, 720, 30);
//   cam.start();  /* ... */  cam.stop();

#include "videosdk/VideoTrack.hpp"

namespace videosdk::adapters {

using V4L2Camera = ::videosdk::VideoTrack;

}  // namespace videosdk::adapters
