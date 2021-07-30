# gipFFmpegVideo
A GlistEngine plugin that plays a video and draws it on the screen using FFmpeg library.

## Installation
Fork & clone this project into ~/dev/glist/glistplugins

## Usage
1. Add gipFFmpegVideo into plugins of your GlistApp/CMakeLists.txt
> set(PLUGINS gipFFmpegVideo)

2. Include gipFFmpegVideo.h in GameCanvas.h
> #include "gipFFmpegVideo.h"

3. Define gipFFmpegVideo object
> gipFFmpegVideo video;

4. Load video
> void GameCanvas::setup() {
>     video.loadVideo("videofilename");
> }

5. Update
> void GameCanvas::update() {
>     video.update();
> }

6. Draw
> void GameCanvas::draw() {
>     video.draw(x, y);
> }

## Plugin Licence
Apache 2
