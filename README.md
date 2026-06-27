# VideoSDK RTC C++ SDK

Official C++ SDK of [videosdk.live](https://www.videosdk.live/)

## Features

- Add real-time audio and video to native C++ apps on Linux and embedded devices such as Raspberry Pi and NVIDIA Jetson.
- Built on WebRTC. Captures from V4L2 cameras and PulseAudio, renders with SDL2.
- Ships as a prebuilt shared library, so there is no toolchain or source build to set up.

## Requirements

- Linux on `arm64`, `armv7`, or `x86_64`
- CMake 3.16+
- A C++17 compiler

## Installation

```bash
curl -fsSL https://raw.githubusercontent.com/videosdk-live/videosdk-rtc-cpp-sdk/main/install.sh | sudo sh
```

This installs the headers and `libvideosdk.so` into `/usr/local`. Then install the system libraries it depends on:

```bash
sudo apt-get install -y libpulse0 libsdl2-2.0-0 libjpeg-turbo8
```

## Usage

Generate a token from the [dashboard](https://app.videosdk.live/) and create a room with the [Create Room API](https://docs.videosdk.live/api-reference/realtime-communication/create-room).

### Import

```cpp
#include <videosdk/videosdk.hpp>
```

### Initialize Meeting

```cpp
videosdk::Meeting meeting("MEETING_ID", "TOKEN");
```

To set a display name, use `MeetingConfig`:

```cpp
videosdk::MeetingConfig config;
config.meetingId = "MEETING_ID";
config.token = "TOKEN";
config.name = "Raspberry Pi";

videosdk::Meeting meeting(config);
```

### Add Listeners

```cpp
meeting.onParticipantJoined([](const std::string& id, const std::string& name) {
});

meeting.onParticipantLeft([](const std::string& id) {
});

meeting.onError([](videosdk::ErrorCode code, const std::string& message) {
});

meeting.onData([](const uint8_t* data, size_t len, bool is_binary) {
});
```

### Join

```cpp
meeting.join();

meeting.enableSpeaker();
meeting.enableMic();
meeting.enableCamera();
meeting.enableVideoDisplay();
```

### Send data

```cpp
std::string msg = "hello";
meeting.sendData(msg.data(), msg.size(), /*binary=*/false);   // text
meeting.sendData(blob, sizeof(blob), /*binary=*/true);        // binary
```

### Simulcast

Configure up to three VP8 layers before `join()` (smallest → largest):

```cpp
meeting.setSimulcastLayers({
    {.width = 320,  .height = 180, .maxBitrateKbps = 300,  .maxFps = 15},
    {.width = 640,  .height = 360, .maxBitrateKbps = 1500, .maxFps = 20},
    {.width = 1280, .height = 720, .maxBitrateKbps = 3000, .maxFps = 30},
});
```

## Listeners

### Meeting events

1. `onError` — an unrecoverable error occurred.
2. `onParticipantJoined` — a remote participant joined.
3. `onParticipantLeft` — a remote participant left.
4. `onData` — a data-channel message was received.

## Build

```cmake
find_package(videosdk REQUIRED)
target_link_libraries(my_app PRIVATE videosdk::cpp)
```

## Examples

Interactive consoles that toggle each media path at runtime: `m` mic, `c` camera, `s` speaker, `d` display, `i` stats, `t` send text, `b` send binary, `q` quit.

- [examples/raspberry_pi.cpp](examples/raspberry_pi.cpp) — Raspberry Pi / generic Linux
- [examples/arduino.cpp](examples/arduino.cpp) — Arduino UNO Q (Debian on the QRB2210)
- [examples/jetson.cpp](examples/jetson.cpp) — NVIDIA Jetson Orin Nano

```bash
cd examples && mkdir build && cd build
cmake .. && make
export VIDEOSDK_TOKEN="..." VIDEOSDK_MEETING_ID="..."
./raspberry_pi      # or ./arduino, ./jetson
```

## Documentation

[docs.videosdk.live](https://docs.videosdk.live/)

## License

[MIT](LICENSE)
