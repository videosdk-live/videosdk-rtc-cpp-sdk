#pragma once

#include <cstdint>
#include <functional>

namespace videosdk {

// Decoded PCM audio frame delivered from the media engine to the app.
// `pcm` is interleaved 16-bit signed PCM, length `samples_per_channel * channels`.
// Pointer valid only for the duration of the callback — copy out to keep.
struct AudioFrame {
    const int16_t* pcm;
    uint32_t samples_per_channel;
    uint32_t sample_rate;
    uint32_t channels;
    int64_t timestamp_us;
};

using AudioFrameCallback = std::function<void(const AudioFrame&)>;

}  // namespace videosdk
