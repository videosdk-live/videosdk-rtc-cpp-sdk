#pragma once

// App-owned speaker adapter: subscribes to Meeting::onRemoteAudioFrame in
// start() and plays every decoded remote frame. PulseAudio on Linux, WASAPI on
// Windows, AVFoundation on macOS.
//
//   videosdk::adapters::PulseSpeaker spk(meeting);
//   spk.start();  /* ... */  spk.stop();

#include "videosdk/AudioPlayer.hpp"

namespace videosdk::adapters {

using PulseSpeaker = ::videosdk::AudioPlayer;

}  // namespace videosdk::adapters
