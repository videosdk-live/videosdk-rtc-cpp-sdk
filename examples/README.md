# Using the SDK

[`main.cpp`](main.cpp) is the example in this folder: it joins a meeting, turns on the
microphone, camera, speaker and video window, and toggles each from the keyboard. See the
[top-level README](../README.md) for how to install the SDK and build it.

This document is the API reference for everything the SDK offers, including the features
the example does not show — the data channel, cloud recording, HLS, RTMP livestreaming,
raw frames and tracing.

One include pulls in the whole API:

```cpp
#include <videosdk/videosdk.hpp>
```

## Contents

- [A complete program](#a-complete-program)
- [The main thread](#the-main-thread)
- [Meeting and events](#meeting-and-events)
- [Devices](#devices)
- [Video quality](#video-quality)
- [Data channel](#data-channel)
- [End-to-end encryption](#end-to-end-encryption)
- [Cloud recording, HLS and RTMP livestream](#cloud-recording-hls-and-rtmp-livestream)
- [Raw frames](#raw-frames)
- [App-owned devices](#app-owned-devices)
- [Statistics](#statistics)
- [Performance tracing](#performance-tracing)
- [Callback rules](#callback-rules)

## A complete program

```cpp
#include <videosdk/videosdk.hpp>

#include <cstdio>
#include <string>
#include <thread>

int main() {
    videosdk::MeetingConfig config;
    config.meetingId = "<meeting-id>";
    config.token = "<token>";
    config.name = "My C++ app";           // display name in the meeting
    videosdk::Meeting meeting(config);

    meeting.onParticipantJoined([](const std::string&, const std::string& name) {
        std::printf("%s joined\n", name.c_str());
    });
    meeting.onError([](videosdk::ErrorCode, const std::string& message) {
        std::fprintf(stderr, "error: %s\n", message.c_str());
    });

    if (!meeting.join()) return 1;

    meeting.enableMic();                                  // default microphone
    meeting.enableSpeaker();                              // play remote audio
    meeting.enableCamera("/dev/video0", 1280, 720, 30);
    meeting.enableVideoDisplay(1280, 720);                // window with remote video

    // Quit on Enter. The main thread must stay in runEventLoop().
    std::thread input([] {
        std::getchar();
        videosdk::stopEventLoop();
    });
    videosdk::runEventLoop();
    input.join();

    meeting.disableVideoDisplay();
    meeting.disableCamera();
    meeting.disableMic();
    meeting.disableSpeaker();
    meeting.leave();
    return 0;
}
```

Windows reads that first argument as camera 0; macOS ignores it and uses the default
camera.

## The main thread

Call `videosdk::runEventLoop()` from `main()`, and only from `main()`. It returns once any
thread calls `videosdk::stopEventLoop()`.

This is required whenever you use the video window, and harmless otherwise. Do not call
`stopEventLoop()` from a signal handler —
set a flag there and stop the loop from normal code.

## Meeting and events

**Call order:** create the `Meeting`, register callbacks and set E2EE, simulcast or bitrate,
then `join()`, then turn on devices. When done, turn devices off, then `leave()`.

```cpp
videosdk::Meeting meeting("<meeting-id>", "<token>");   // short form

meeting.onParticipantJoined([](const std::string& id, const std::string& name) { });
meeting.onParticipantLeft([](const std::string& id) { });
meeting.onError([](videosdk::ErrorCode code, const std::string& message) { });
```

`MeetingConfig` takes `meetingId` and `token` (both required), an optional display `name`
and an optional `participantId`; leave the last empty and one is generated.

`ErrorCode` is one of `Success`, `InvalidConfig`, `InvalidArgument`, `ConnectionFailed`,
`SignalingFailed`, `NotJoined`, `AlreadyJoined`, `SendFailed`, `InternalError`, `Panic`.

A `Meeting` cannot be copied. Keep it alive until after `leave()` returns.

## Devices

The SDK opens and owns the platform's microphone, speaker, camera and video window. Each
call returns `true` on success, or `false` with the reason printed, and each can be called
again at any time.

```cpp
meeting.enableMic("default");                         // disableMic()
meeting.enableSpeaker();                              // disableSpeaker()
meeting.enableCamera("/dev/video0", 1280, 720, 30);   // disableCamera()
meeting.enableVideoDisplay(1280, 720);                // disableVideoDisplay()
meeting.isVideoDisplayEnabled();                      // false once the user closes the window
```

Query `isVideoDisplayEnabled()` rather than tracking a flag of your own: the user can close
the window from its title bar at any time.

`disableMic()` and `disableCamera()` unpublish, so other participants see the device turn
off. They also end media sent with `publishAudioFrame` / `publishVideoFrame` — stop pushing
frames first.

## Video quality

Call these **before** `join()`.

```cpp
// Simulcast: up to three layers, smallest first. { width, height, max kbps, max fps }
meeting.setSimulcastLayers({
    {320, 180, 300, 15},
    {640, 360, 1500, 20},
    {1280, 720, 3000, 30},
});

// Or, for single-layer video, cap the bitrate.
meeting.setVideoMaxBitrate(1500);   // kbps
```

With simulcast the server sends each viewer the layer their connection can carry. The top
layer should match the resolution you actually publish. Sizes are absolute, and `0` means
no cap.

## Data channel

Messages are text or binary, up to **15 KiB** each. `sendData` returns `false` if the
message is too big or the channel isn't ready yet.

```cpp
meeting.onData([](const uint8_t* data, size_t len, bool is_binary) { });

std::string text = "hello";
meeting.sendData(text.data(), text.size(), /*binary=*/false);
```

Send text (`binary = false`) when the other end is a browser or mobile SDK that expects a
string; binary frames arrive there as an `ArrayBuffer`. `data` in the callback is valid only
until it returns — copy anything you keep.

## End-to-end encryption

Encrypts every audio and video frame on the device, so VideoSDK's servers only ever see
ciphertext. Everyone in the meeting must use the same secret. It works with VideoSDK's
iOS, Android and Flutter SDKs using the same secret.

Set the key **before** `join()`; otherwise the first frames go out unencrypted:

```cpp
meeting.setE2EEAlgorithm(videosdk::E2EEAlgorithm::VsdkAesGcm128);
if (!meeting.setE2EEKey(std::string("shared-secret"))) {
    return 1;   // don't join unencrypted
}
meeting.join();

meeting.ratchetE2EEKey();   // optional, during the call: advance the key one step
```

- Encryption is AES-GCM with a key derived from your secret. `setE2EEKey` also accepts
  raw bytes: `setE2EEKey(const uint8_t* key, size_t len)`.
- Call `setE2EEAlgorithm` before `setE2EEKey`.
- After `ratchetE2EEKey()`, other peers keep up automatically for up to 16 steps.
- To turn encryption off, create a new `Meeting`.
- E2EE **can't be combined** with cloud recording, HLS or transcription. Those composite
  the meeting on the server, which cannot decrypt your frames.

## Cloud recording, HLS and RTMP livestream

These run on VideoSDK's servers; your device only starts and stops them. For all three:

- call them **after** `join()`;
- the token needs **recording permission** (the three share it);
- someone must be sending audio or video, or the output is empty;
- empty strings mean server defaults;
- callbacks receive status names and the server's JSON reply as text.

**Cloud recording:**

```cpp
meeting.onRecordingStateChanged([](const std::string& status) {
    // RECORDING_STARTING → RECORDING_STARTED → RECORDING_STOPPING → RECORDING_STOPPED
});
meeting.onRecordingStarted([](const std::string& json) { /* recording id, … */ });
meeting.onRecordingStopped([](const std::string& json) { /* storage path, … */ });

meeting.startRecording(/*webhook_url=*/"", /*aws_dir_path=*/"",
                       /*config_json=*/"", /*transcription_json=*/"");
meeting.stopRecording();
```

**HLS livestream** — the server turns the meeting into an HLS stream. The playback URL
(`playbackHlsUrl`) arrives in `onHlsPlayableStateChanged`, about 15–30 seconds after
`HLS_STARTED`:

```cpp
meeting.onHlsStateChanged([](const std::string& status) { /* HLS_STARTING … HLS_STOPPED */ });
meeting.onHlsPlayableStateChanged([](const std::string& json) { /* has playbackHlsUrl */ });
meeting.onHlsStarted([](const std::string& json) { /* … */ });
meeting.onHlsStopped([](const std::string& json) { /* … */ });

meeting.startHls(/*config_json=*/"", /*transcription_json=*/"");
meeting.stopHls();
```

**RTMP livestream** — the server streams the meeting to any RTMP address. `outputs` is a
JSON list of `{url, streamKey}` (`streamKey` exactly as written; `stream_key` is rejected):

```cpp
meeting.onLivestreamStateChanged([](const std::string& status) {
    // LIVESTREAM_STARTING → LIVESTREAM_STARTED → LIVESTREAM_STOPPING → LIVESTREAM_STOPPED
});

const std::string outputs =
    R"([{"url":"rtmp://a.rtmp.youtube.com/live2","streamKey":"xxxx-yyyy"}])";
const std::string config =
    R"({"layout":{"type":"GRID","priority":"SPEAKER","gridSize":9},"theme":"DEFAULT"})";
meeting.startLivestream(outputs, config);
meeting.stopLivestream();
```

`onLivestreamStarted` and `onLivestreamStopped` receive the server's JSON reply.

## Raw frames

To produce or display media yourself, skip the built-in devices and exchange frames
directly:

```cpp
meeting.publishVideoFrame(y, u, v, y_stride, u_stride, v_stride,
                          width, height, timestamp_us);          // I420
meeting.publishAudioFrame(pcm, 480, 48000, 1);                   // 10 ms, 48 kHz mono, 16-bit

meeting.onRemoteVideoFrame([](const videosdk::VideoFrame& f) { });
meeting.onRemoteAudioFrame([](const videosdk::AudioFrame& f) { });
```

Use `publishVideoFrame` and `publishAudioFrame` instead of `enableCamera` and `enableMic`,
not alongside them. To stop sending, stop pushing frames, then call `disableCamera()` or
`disableMic()`. Pushing a frame again starts sending again.

The SDK copies each frame, so your buffer only has to outlive the call. Strides are in
bytes per row. Calling these before `join()` finishes is harmless. There is one sink per
stream type: registering again replaces the previous callback, and an empty function
unsubscribes.

## App-owned devices

The adapters in [`videosdk/adapters/`](../include/videosdk/adapters) sit between the two
approaches: the SDK still drives the hardware, but the media flows through the raw-frame
API rather than the Meeting's built-in devices. Each has the same shape.

```cpp
#include <videosdk/adapters/PulseMic.hpp>
#include <videosdk/adapters/SDL2Display.hpp>

videosdk::adapters::PulseMic mic(meeting, "default");
videosdk::adapters::PulseSpeaker spk(meeting);
videosdk::adapters::V4L2Camera cam(meeting, "/dev/video0", 1280, 720, 30);
videosdk::adapters::SDL2Display disp(meeting, 1280, 720);

mic.start();  /* … */  mic.stop();
```

| Adapter | Does | Linux / Windows / macOS |
|---|---|---|
| `PulseMic` | Captures 16-bit PCM, publishes each 10 ms frame | PulseAudio / WASAPI / AVFoundation |
| `PulseSpeaker` | Plays every decoded remote audio frame | PulseAudio / WASAPI / AVFoundation |
| `V4L2Camera` | Publishes I420 camera frames | V4L2 / Media Foundation / AVFoundation |
| `SDL2Display` | Blits decoded remote video to a window | SDL2 |

On Windows the audio source and camera device are name substrings or a camera index; on
macOS both are ignored and the system default is used. With `SDL2Display`, macOS still
needs `runEventLoop()` on the main thread.

## Statistics

```cpp
videosdk::MeetingStats s = meeting.stats();
```

| Field | Meaning |
|---|---|
| `audioFramesSent` | Audio frames published from this device |
| `audioFramesPlayed` | Remote audio frames played through the speaker |
| `videoFramesSent` | Video frames published from this device |
| `videoFramesReceived` | Remote video frames decoded |
| `videoFramesRendered` | Remote video frames drawn in the window |
| `videoFramesDropped` | Frames decoded but never drawn |

All six are cumulative counters since the meeting was joined. `videoFramesDropped` rising
while `videoFramesReceived` keeps up means rendering is behind, not the network.

## Performance tracing

Writes a Chrome trace-format JSON file you can open in
[ui.perfetto.dev](https://ui.perfetto.dev) or `chrome://tracing`.

```cpp
meeting.startTracing("videosdk-trace.json");
meeting.stopTracing();
```

You can also set `VIDEOSDK_TRACE=/path/trace.json` before creating the `Meeting`. Add your
own spans with the macros in [`videosdk/Trace.hpp`](../include/videosdk/Trace.hpp):

```cpp
#include <videosdk/Trace.hpp>

void render_frame() {
    VSDK_TRACE_SCOPE("app", "render_frame");   // measures until the scope ends
    VSDK_TRACE_INSTANT("app", "frame_ready");  // a point in time
}
```

Tracing can be started only **once per run** — after `stopTracing()`, a later
`startTracing()` cannot reopen it. `startTracing` returns `false` if the path is empty.

## Callback rules

- Callbacks arrive on background threads, not yours. Return quickly, well under 10 ms — a
  slow callback delays media.
- Strings and pointers passed to a callback are valid only until it returns.
- Keep the `Meeting` alive until after `leave()`.
- Print from callbacks through a mutex, or lines from different threads interleave.

Every call is declared in [`include/videosdk/Meeting.hpp`](../include/videosdk/Meeting.hpp).
