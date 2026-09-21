#pragma once

#include <cstdint>
#include <functional>

namespace videosdk {

struct VideoFrame {
    int width;
    int height;
    const uint8_t* y_plane;
    int y_stride;
    const uint8_t* u_plane;
    int u_stride;
    const uint8_t* v_plane;
    int v_stride;
    int64_t timestamp_us;
};

using VideoFrameCallback = std::function<void(const VideoFrame&)>;

}  // namespace videosdk
