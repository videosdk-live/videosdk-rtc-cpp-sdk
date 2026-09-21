#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "videosdk_ffi.h"

namespace videosdk
{

    class Meeting;

    /// Captures camera frames, converts them to I420 and pushes them to the SDK
    /// core; the media engine owns the encoder. V4L2 on Linux (first of MJPEG, NV12,
    /// I420, YUYV, UYVY, RGB24, BGR24 the camera offers, converted via libyuv),
    /// Media Foundation on Windows, AVFoundation on macOS.
    class VideoTrack
    {
    public:
        static constexpr int kDefaultWidth = 640;
        static constexpr int kDefaultHeight = 480;
        static constexpr int kDefaultFps = 30;

        /// Preferred constructor. Push path goes through Meeting::publishVideoFrame.
        VideoTrack(Meeting &meeting,
                   std::string device_path,
                   int width = kDefaultWidth,
                   int height = kDefaultHeight,
                   int fps = kDefaultFps);

        /// Legacy constructor. Prefer the Meeting& overload above.
        VideoTrack(VsdkMeeting *meeting,
                   std::string device_path,
                   int width = kDefaultWidth,
                   int height = kDefaultHeight,
                   int fps = kDefaultFps);

        ~VideoTrack();

        VideoTrack(const VideoTrack &) = delete;
        VideoTrack &operator=(const VideoTrack &) = delete;

        /// Open the camera and start the capture thread. Returns false on
        /// any failure (logged to stderr).
        bool start();

        /// Stop the capture thread and release the camera. Idempotent.
        void stop();

        uint64_t framesSent() const { return frames_sent_.load(); }
        uint64_t bytesSent() const { return bytes_sent_.load(); }

    private:
        struct MmapBuffer
        {
            void *start{nullptr};
            size_t length{0};
        };

        bool openDevice();
        void captureLoop();
        bool mjpegToI420(const uint8_t *jpeg, size_t jpeg_size, uint8_t *i420);
        void closeDevice();

        Meeting *meeting_wrapper_{nullptr};
        VsdkMeeting *meeting_ffi_{nullptr};
        std::string device_path_;
        int width_;
        int height_;
        int fps_;

        int fd_{-1};
        std::vector<MmapBuffer> buffers_;

        // V4L2 fourcc agreed at openDevice(); picks the libyuv converter.
        uint32_t pixel_format_{0};

        std::vector<uint8_t> i420_;
        int64_t pts_us_{0};

        std::thread thread_;
        std::atomic<bool> running_{false};
        std::atomic<uint64_t> frames_sent_{0};
        std::atomic<uint64_t> bytes_sent_{0};

#if defined(__APPLE__) || defined(_WIN32)
        // Darwin / Windows backend state (src/platform/<os>/). Guarded to leave
        // the Linux layout untouched.
        void *impl_{nullptr};
#endif
    };

} // namespace videosdk
