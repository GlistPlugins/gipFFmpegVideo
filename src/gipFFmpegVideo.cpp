/*
 * gipFFmpegVideo.cpp
 *
 *  Created on: 10 Jul 2021
 *      Author: kayra
 *      Edited By: Umutcan Turkmen 24 Feb 2023
 */

#include "gipFFmpegVideo.h"

gipFFmpegVideo::gipFFmpegVideo() {
}

gipFFmpegVideo::~gipFFmpegVideo() {
}

void gipFFmpegVideo::load(std::string fullPath) {
	filepath = fullPath;
	videostate = gLoadVideoStateFromStorage(fullPath);

	if(!videostate->iscreated) {
		gLoge("Could not open video file at: " + fullPath);
	}

	fpsquotient = static_cast<float>(videostate->avgfps) / appmanager->getTargetFramerate();

    framebuffer = new gTexture(videostate->width, videostate->height, GL_RGBA);
}

void gipFFmpegVideo::loadVideo(std::string videoPath) {
	load(gGetVideosDir() + videoPath);
}

void gipFFmpegVideo::update() {
	// When quotientnum == 1 then try to display every frame
	if(videostate->isfinished || ispaused || !videostate->iscreated || !isplaying) {
		return;
	}
	if(currentframe >= videostate->framecount) {
		close();
		return;
	}

	// Advancing 2 times ensures we have enough frames in buffer at all times.
	// Therefore we dont lose our video fps.
	gAdvanceFramesUntilBufferFull(videostate);
	gAdvanceFramesUntilBufferFull(videostate);

	// When fpsquotientcumulative is atleast 1, that means its ready to show the frame on screen
	// For example if app fps is 45, and our video is 30fps, fpsquotient is 0.66, Meaning that we
	// will show 2 video frames per 3 engine tick on average.
	if (videostate->readytoplay)
	{
		if(fpsquotientcumulative >= 1.0f) {
			fpsquotientcumulative -= 1.0f;
			gFetchVideoFrameToState(videostate);
			framebuffer->setData(videostate->videoframepixeldata.get(), false, false);
			// gLogd("gipFFmpegVideo::update")
			// 	<< "Rendering frame: " << currentframe << "/" << videostate->framecount;

			// gLogd("gipFFmpegVideo::update")
			// 	<< "App FPS: " << appmanager->getFramerate()
			// 	<< " Video FPS: " << videostate->avgfps
			// 	<< " App Target FPS: " << appmanager->getTargetFramerate();
			currentframe++;
			gClearLastFrame(videostate);
		}
		fpsquotientcumulative += fpsquotient;
	}
}

void gipFFmpegVideo::draw() {
	draw(0, 0);
}

void gipFFmpegVideo::draw(int x, int y) {
	draw(x, y, videostate->width, videostate->height);
}

void gipFFmpegVideo::draw(int x, int y, int w, int h) {
	if(videostate->isfinished || !videostate->iscreated || currentframe <= 0) {
		return;
	}
	framebuffer->draw(x, y, w, h);
}

void gipFFmpegVideo::play() {
	if(!videostate->iscreated) return;

	//appmanager->setTargetFramerate(videostate->avgfps + 1);
    isplaying = true;
}

void gipFFmpegVideo::stop() {
	close();

	videostate = gLoadVideoStateFromStorage(filepath);

	isplaying = false;
}

void gipFFmpegVideo::setPaused(bool t_isPaused) {
	ispaused = t_isPaused;
}

void gipFFmpegVideo::close() {
	gClearVideoState(videostate);
	gClearLastFrame(videostate);
	videostate->isfinished = true;
}

double gipFFmpegVideo::getPosition() {
	return 0.0; // TODO
}

double gipFFmpegVideo::getDuration() {
	return durationInSec / static_cast<double>(time_base.den);
}

int gipFFmpegVideo::getWidth() {
	return videostate->width;
}

int gipFFmpegVideo::getHeight() {
	return videostate->height;
}

void gipFFmpegVideo::setSpeed(float t_speed) {
	speed = t_speed;
}

void gipFFmpegVideo::setVolume(float t_volume) {
	volume = t_volume;
}

float gipFFmpegVideo::getSpeed() {
	return speed;
}

float gipFFmpegVideo::getVolume() {
	return volume;
}

bool gipFFmpegVideo::isPlaying() {
	return isplaying;
}

bool gipFFmpegVideo::isPaused() {
	return ispaused;
}
