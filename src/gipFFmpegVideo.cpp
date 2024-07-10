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

    framebuffer = new gTexture(videostate->width, videostate->height, GL_RGBA);

	// This part calculates the number of frames with the loaded video data
	// We don't use the metadata of video since it can be corrupted or false
	// Also avgfps is a close approximation and might not be the exact value

	double duration = static_cast<double>(videostate->formatcontext->duration / AV_TIME_BASE);
	double numframes = static_cast<double>(videostate->formatcontext->streams[0]->nb_frames);
	avgfps = numframes / duration;

	// This part calculates at which time intervals frames would be changed
	// To give an example if the app is running with 80fps and our video is 25fps
	// quotientnum = 80/25 = 3 fpsintervalnum = 25 / (80/3 - 25) = 25
	// the program will display every (quotientnum = 3) times resulting in 80/3 fps
	// also to ensure the same rate every fpsintervalnum = 25 times when it won't display
	// the frame.

	quotientnum = static_cast<int>(appmanager->getTargetFramerate() / avgfps);
	fpsintervalnum = avgfps / (appmanager->getTargetFramerate() / quotientnum - avgfps);
	quotientno = 0;

	//Approximate fpsintervalnum to the closest integer
	if(fpsintervalnum - std::floor(fpsintervalnum) >= 0.5) {
		fpsintervalnum = std::ceil(fpsintervalnum);
	} else {
		fpsintervalnum = std::floor(fpsintervalnum);
	}

	fpsintervalnum = std::floor(fpsintervalnum);
	fpsintervalno = 0;
}

void gipFFmpegVideo::loadVideo(std::string videoPath) {
	load(gGetVideosDir() + videoPath);
}

void gipFFmpegVideo::update() {
	// When quotientnum == 1 then try to display every frame
	if (quotientno < quotientnum - 1 || quotientnum == 1) {
		quotientno++;
	}
	else {
		quotientno = 0;
		// When fpsintervalnum == 0 then appfps is a multiple of avgfps
		if(fpsintervalno < fpsintervalnum || fpsintervalnum == 0) {
			fpsintervalno++;
			if(closed || ispaused || !videostate->iscreated || !isplaying) {
				return;
			}
			if(currentframe >= videostate->totalframecount) {
				close();
				return;
			}

			int result;
			int size_1, size_2;
			float *buffer_1, *buffer_2;
			gAdvanceFrameInPacket(videostate);

			if(videostate->lastreceivedframetype == FrameType::FRAMETYPE_VIDEO) {
				gFetchVideoFrameToState(videostate);
				framebuffer->setData(videostate->videoframepixeldata, false, false);
				currentframe++;
			}
			//gLogi("ffmpeg video drawn");
		} else {
			//gLogi("ffmpeg video not drawn");
			fpsintervalno = 0;
		}
	}
}

void gipFFmpegVideo::draw() {
	draw(0, 0);
}

void gipFFmpegVideo::draw(int x, int y) {
	draw(x, y, videostate->width, videostate->height);
}

void gipFFmpegVideo::draw(int x, int y, int w, int h) {
	if(closed || !videostate->iscreated || currentframe <= 0) {
		return;
	}
	framebuffer->draw(x, y, w, h);
}

void gipFFmpegVideo::play() {
	if(!videostate->iscreated) return;

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
	closed = true;
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
