/*
 * gipFFmpegVideo.cpp
 *
 *  Created on: 10 Jul 2021
 *      Author: kayra
 *      Edited By: Umutcan Turkmen 24 Feb 2023
 */

#include "gipFFmpegVideo.h"

gipFFmpegVideo::gipFFmpegVideo() {
	framecount = 0;
	duration = 0;
	filepath = "";
	width = 0;
	height = 0;
	currentframe = 0;
	isplaying = false;
	ispaused = false;
    couldopen = false;
    closed = false;
    speed = 1.0f;
	timesincelastframe = 0.0;
	timeperframe = 0.0;
}

gipFFmpegVideo::~gipFFmpegVideo() {

}

void gipFFmpegVideo::load(std::string fullPath) {
	filepath = fullPath;
	couldopen = utils.openVideo(fullPath.c_str(), &width, &height, &framecount, &duration, &time_base);

	if(!couldopen) {
		gLoge("Could not open video file at: " + fullPath);
	}

    framebuffer = new gTexture(width, height, GL_RGBA);

	//This part calculates the number of frames with the loaded video data
	//We don't use the metadata of video since it can be corrupted or false
	//Also videofps is a close approximation and might not be the exact value

	double duration = (double)utils.getState()->formatcontext->duration / AV_TIME_BASE;
	double numframes = (double)utils.getState()->formatcontext->streams[0]->nb_frames;
	videofps = numframes / duration;
	timeperframe = videofps / 1000.0f;
}

void gipFFmpegVideo::loadVideo(std::string videoPath) {
	load(gGetVideosDir() + videoPath);
}

void gipFFmpegVideo::update() {
	timesincelastframe += appmanager->getElapsedTime();
	if(timesincelastframe >= timeperframe) {
		if(closed || ispaused || !couldopen || !isplaying) {
			return;
		}
		if(currentframe >= framecount) {
			close();
			return;
		}

		int result;
		int size_1, size_2;
		float *buffer_1, *buffer_2;
		while(true) {
			result = utils.read_frame();

			if(result == gipFFmpegUtils::RECEIVED_VIDEO) {
				utils.fetch_video_frame(&framedata, nullptr);
				framebuffer->setData(framedata, false, false);
				currentframe++;
				timesincelastframe = 0;
				break;
			} else if (result == gipFFmpegUtils::RECEIVED_NONE) {
				gLoge("Video:") << "Could not load the video frame";
				return;
			}
			// Audio received, (result = number of samples)
			utils.getAudio()->getRingBuffer()->writeWait(result * utils.getState()->num_channels, &size_1, &buffer_1, &size_2, &buffer_2);
			utils.fetch_audio_frame(size_1, buffer_1, size_2, buffer_2);
			utils.getAudio()->getRingBuffer()->writeAdvance(result * utils.getState()->num_channels);
		}
	}
}

void gipFFmpegVideo::draw() {
	draw(0, 0);
}

void gipFFmpegVideo::draw(int x, int y) {
	draw(x, y, width, height);
}

void gipFFmpegVideo::draw(int x, int y, int w, int h) {
	if(closed || !couldopen || currentframe <= 0) {
		return;
	}
	framebuffer->draw(x, y, w, h);
}

void gipFFmpegVideo::play() {
	if(!couldopen) return;

    isplaying = true;
}

void gipFFmpegVideo::stop() {
	close();

	couldopen = utils.openVideo(filepath.c_str(), &width, &height, &framecount, &duration, &time_base);

	isplaying = false;
}

void gipFFmpegVideo::setPaused(bool isPaused) {
	ispaused = isPaused;
}

void gipFFmpegVideo::close() {
	utils.close();
	closed = true;
}

double_t gipFFmpegVideo::getPosition() {
	return currentframe / videofps;
}

double_t gipFFmpegVideo::getDuration() {
	return (double_t) duration / time_base.den;
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

bool gipFFmpegVideo::isPaused() {
	return ispaused;
}
