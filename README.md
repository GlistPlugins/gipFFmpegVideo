# gipFFmpegVideo
A GlistEngine plugin that plays a video and draws it on the screen using FFmpeg library. For instant, the plugin is compatible with Windows and Linux.

## Installation
Fork & clone this project into 
> C:\dev\glist\glistplugins (on Windows)
> ~/dev/glist/glistplugins (on Linux)

*Notice For Linux Developers* :
Please install necessary libraries before running the plugin:
> sudo apt-get install libavcodec58 libavcodec-dev libavformat58 libavformat-dev libavfilter7 libavfilter-dev libavdevice58 libavdevice-dev libavutil56 libavutil-dev libswscale5 libswscale-dev libao-dev

##### *Notice for all*

Please add the PATH variable to eclipse paths by:

Right Click the GlistApp project once on the left, inside the Project Explorer

Then:

> Project > Properties > C/C++ Build > Environment

then click PATH followed by clicking Edit on the right. There, at the end of the paths, add:

> ${workspace_loc}\\..\\..\\..\\..\glistplugins\gipFFmpegVideo\prebuilts\bin

## Usage
1. Add gipFFmpegVideo into plugins of your GlistApp/CMakeLists.txt
```cmake
set(PLUGINS gipFFmpegVideo)
```

2. Include gipFFmpegVideo.h in GameCanvas.h
```cpp
#include "gipFFmpegVideo.h"
```

3. Define gipFFmpegVideo object

```cpp
gipFFmpegVideo video;
```

### Basic playback (streaming)
Frames are decoded on-the-fly during playback. Low memory usage, works for any
file size. Audio is played automatically if the file contains an audio stream.
```cpp
void GameCanvas::setup() {
    video.loadVideo("sintel_trailer-480p.mp4");
    video.setVolume(0.8f); // optionally set the volume
    video.setPaused(false);
    video.play();
}

void GameCanvas::draw() {
    video.draw(0, 0);
}
```

### Preloaded playback
All frames are decoded into memory before playback starts. Gives the smoothest
playback but uses significant RAM (each frame = width * height * 4 bytes).
A 1080p 24fps 1-minute clip uses ~12 GB. Memory is capped to prevent OOM
(default 2 GB); videos exceeding the limit fall back to a large streaming buffer.

`setMaxPreloadMemory()` must be called before `setPreloaded()`. Both must be
called after `loadVideo()` and before `play()`.
```cpp
void GameCanvas::setup() {
    video.loadVideo("intro.mp4");
    video.setMaxPreloadMemory(512 * 1024 * 1024); // optional, 512 MB limit
    video.setPreloaded(true);
    video.setPaused(false);
    video.play();
}

void GameCanvas::draw() {
    // isLoading() returns true during initial buffering or mid-playback stalls
    if(video.isLoading()) {
        // draw a loading indicator
    } else {
        video.draw(0, 0);
    }
}
```

### Custom buffer duration (streaming)
Controls how many seconds of frames are pre-buffered before playback begins.
Default is ~4 frames. Higher values add startup delay but reduce stalls on
slow I/O. Must be called after `loadVideo()`.
```cpp
video.loadVideo("big_video.mkv");
video.setBufferDuration(2.0f); // buffer 2 seconds before starting
video.setPaused(false);
video.play();
```

### Aspect-ratio-preserving draw
```cpp
void GameCanvas::draw() {
    float scale = std::min(
        static_cast<float>(getWidth()) / video.getWidth(),
        static_cast<float>(getHeight()) / video.getHeight()
    );
    int w = static_cast<int>(video.getWidth() * scale);
    int h = static_cast<int>(video.getHeight() * scale);
    video.draw((getWidth() - w) / 2, (getHeight() - h) / 2, w, h);
}
```

## Plugin Licence
Apache 2
