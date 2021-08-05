/*
 * gipFFmpegVideo.cpp
 *
 *  Created on: 10 Jul 2021
 *      Author: kayra
 */

#include <thread>
#include "gipFFmpegVideo.h"

gipFFmpegVideo::gipFFmpegVideo() {
	framecount = 0;
	duration = 0;
	filepath = "";
	width = 0;
	height = 0;
	currentframe = 0;
    isPaused = false;
    couldopen = false;
    closed = false;
    speed = 1.0f;
    position = 0;
}

gipFFmpegVideo::~gipFFmpegVideo() {
	delete framedata;
}

void gipFFmpegVideo::load(std::string fullPath) {
	filepath = fullPath;
	couldopen = utils.openVideo(fullPath.c_str(), &width, &height, &framecount, &duration, &time_base);

	if(!couldopen) {
		gLoge("Could not open video file at: " + fullPath);
	}

	firstframe = true;
}

void gipFFmpegVideo::loadVideo(std::string videoPath) {
	load(gGetVideosDir() + videoPath);
}

void gipFFmpegVideo::update() {
	if(closed || isPaused || !couldopen) return;
	if(currentframe >= framecount) {
		close();
		return;
	}
	utils.loadFrame(&framedata, &pts);
	framebuffer.loadData(framedata, width, height, 4);
	currentframe++;

	if(firstframe) {
		glfwSetTime(0.0f);
		firstframe = false;
	}

	ptsec = pts * double(time_base.num) / double(time_base.den / speed) ;
	while(ptsec > glfwGetTime()) {
		glfwWaitEventsTimeout(ptsec - glfwGetTime());
	}
}

void gipFFmpegVideo::draw() {
	draw(0, 0);
}

void gipFFmpegVideo::draw(int x, int y) {
	draw(x, y, width, height);
}

void gipFFmpegVideo::draw(int x, int y, int w, int h) {
	if(closed || isPaused || !couldopen) return;
	framebuffer.draw(x, y, w, h);
}

void gipFFmpegVideo::play() {
	if(!couldopen) return;

    framebuffer = gTexture(width, height, GL_RGBA, false);

    isplaying = true;
}

void gipFFmpegVideo::stop() {
	close();

	couldopen = utils.openVideo(filepath.c_str(), &width, &height, &framecount, &duration, &time_base);

	isplaying = false;
}

void gipFFmpegVideo::setPaused(bool isPaused) {
	this->isPaused = isPaused;
}

void gipFFmpegVideo::close() {
	utils.close();
	closed = true;
}

int64_t gipFFmpegVideo::getPosition() {
	return ptsec;
}

int64_t gipFFmpegVideo::getDuration() {
	return duration / time_base.den;
}

int gipFFmpegVideo::getWidth() {
	return width;
}

int gipFFmpegVideo::getHeight() {
	return height;
}

void gipFFmpegVideo::setSpeed(float speed) {
	this->speed = speed;
}

void gipFFmpegVideo::setVolume(float volume) {
	this->volume = volume;
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
