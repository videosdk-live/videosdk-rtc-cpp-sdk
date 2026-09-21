#pragma once

// App-owned microphone adapter: captures 16-bit PCM and publishes each 10 ms
// frame via Meeting::publishAudioFrame. PulseAudio on Linux, WASAPI on Windows
// (source = mic-name substring), AVFoundation on macOS (source ignored).
//
//   videosdk::adapters::PulseMic mic(meeting, "default");
//   mic.start();  /* ... */  mic.stop();

#include "videosdk/AudioTrack.hpp"

namespace videosdk::adapters {

using PulseMic = ::videosdk::AudioTrack;

}  // namespace videosdk::adapters
