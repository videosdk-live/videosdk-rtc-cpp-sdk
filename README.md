# VideoSDK RTC C++ SDK

Official C++ SDK of [videosdk.live](https://www.videosdk.live/).

Add real-time audio and video to native C++17 apps on **Linux, macOS and Windows**,
including boards such as Raspberry Pi, NVIDIA Jetson and Arduino UNO Q. The SDK joins a
meeting, captures your camera and microphone, plays remote audio and shows remote video
in a window. It also offers a data channel, end-to-end encryption, cloud recording, HLS
and RTMP livestreaming.

| Platform | Camera | Microphone and speaker | Video window |
|---|---|---|---|
| Linux x86_64 and arm64 | V4L2 | PulseAudio or PipeWire | SDL2 |
| macOS, Apple Silicon and Intel | AVFoundation | CoreAudio | SDL2 |
| Windows x64 | Media Foundation | WASAPI | SDL2 |

The SDK ships prebuilt, so a C++ compiler and CMake are all you need.

## Requirements

| Platform | Runs on | To build your app |
|---|---|---|
| Linux | glibc 2.35 or newer: Ubuntu 22.04+, Debian 12+, Raspberry Pi OS Bookworm, NVIDIA JetPack 6, Arduino UNO Q | CMake 3.16+, a C++17 compiler |
| macOS | macOS 11 or newer | CMake 3.16+, Xcode Command Line Tools |
| Windows | Windows 10 or 11, x64 | CMake 3.16+, Visual Studio 2022 (or its Build Tools) |

Check a Linux system's glibc with `ldd --version`.

## Installation

### Linux and macOS

```bash
curl -fsSL https://raw.githubusercontent.com/videosdk-live/videosdk-rtc-cpp-sdk/main/install.sh | sudo sh
```

This installs the headers and the library into `/usr/local`.

- **Linux:** on Debian, Ubuntu and their derivatives, the script also installs the system
  libraries the SDK needs. On other distributions, install these yourself: PulseAudio
  client (`libpulse`), SDL2, GLib and libXtst.
- **macOS:** nothing else is needed. SDL2 is included in the package.

Options, set as environment variables:

| Variable | Meaning |
|---|---|
| `VIDEOSDK_VERSION` | Install this release, such as `v0.0.1-beta.6`. Default: the latest |
| `PREFIX` | Install here instead of `/usr/local` |
| `SKIP_DEPS=1` | Don't install system libraries |

On an **Intel Mac with Homebrew SDL2**, the script stops rather than replace Homebrew's
SDL2 in `/usr/local/lib`. Install somewhere else instead:

```bash
curl -fsSL https://raw.githubusercontent.com/videosdk-live/videosdk-rtc-cpp-sdk/main/install.sh | sudo PREFIX=/opt/videosdk sh
```

Then pass `-DCMAKE_PREFIX_PATH=/opt/videosdk` when you configure your app.

### Windows

1. Download `videosdk-cpp-<version>-windows-x64.zip` from
   [Releases](https://github.com/videosdk-live/videosdk-rtc-cpp-sdk/releases).
2. Extract it, for example to `C:\videosdk`.

The zip holds a static library, so it is a few hundred MB. Your own program stays small,
because the linker only takes what it uses. Nothing else needs installing on the machines
that run your program.

## Build the example

[`examples/main.cpp`](examples/main.cpp) joins a meeting, turns on the mic, camera,
speaker and video window, and lets you toggle each from the keyboard. You need a meeting
ID and a token: generate a token from the [dashboard](https://app.videosdk.live/) and
create a meeting with the
[Create Room API](https://docs.videosdk.live/api-reference/realtime-communication/create-room).

**Linux and macOS:**

```bash
cd examples
mkdir build && cd build
cmake ..
cmake --build .
./main <meeting-id> <token>
```

**Windows**, in PowerShell:

```powershell
cd examples
mkdir build; cd build
cmake .. -DCMAKE_PREFIX_PATH=C:\videosdk
cmake --build . --config Release
.\Release\main.exe <meeting-id> <token>
```

On macOS, the first run asks whether Terminal may use the camera and microphone. Allow
both.

| Key | Action |
|---|---|
| `m` | Microphone on / off |
| `c` | Camera on / off |
| `s` | Speaker on / off |
| `d` | Video window open / closed |
| `i` | Print statistics |
| `e` | Advance the encryption key one step (only when E2EE is on) |
| `h` | List the keys |
| `q` | Leave the meeting and quit |

The example also reads `VIDEOSDK_MEETING_ID` and `VIDEOSDK_TOKEN` from the environment.
To choose devices, run `main <meeting-id> <token> <microphone> <camera>`:

| | Camera | Microphone |
|---|---|---|
| Linux | A device path such as `/dev/video2`. Default: the first working camera | A PulseAudio source name (`pactl list short sources`). Default: `default` |
| Windows | A camera number (`0`, `1`) or part of its name. Default: the first camera | Part of the microphone's name. Default: the system default |
| macOS | Always the system default | Always the system default |

## Add the SDK to your project

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# Windows: the SDK uses the static C runtime, so your app must too.
# Must come before find_package.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

find_package(videosdk REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE videosdk::cpp)
```

- **Windows:** build the **Release** configuration (`--config Release`). Debug builds don't
  link.
- **NVIDIA Jetson:** also copy the Tegra `link_directories` block from
  [`examples/CMakeLists.txt`](examples/CMakeLists.txt).

## Using the SDK

[`examples/README.md`](examples/README.md) is the API reference: a worked program, then a
section for each feature — events, devices, video quality, the data channel, end-to-end
encryption, cloud recording, HLS, RTMP livestreaming, raw frames, statistics and tracing.

## Known limitations

- **Built for one-to-one calls.** The built-in speaker and video window take every remote
  stream.
- **No echo cancellation.** Use headphones.
- **Windows:** closing the console window skips `leave()`, and a device unplugged during a
  call isn't picked up again.

## Documentation

[docs.videosdk.live](https://docs.videosdk.live/)

## License

[MIT](LICENSE)
