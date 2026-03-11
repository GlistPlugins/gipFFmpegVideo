/*
 * gipFFmpegVideo.cpp
 *
 *  Created on: 10 Jul 2021
 *      Author: kayra
 *      Edited By: Umutcan Turkmen 24 Feb 2023
 */

#include "gipFFmpegVideo.h"
#include "gAudioSampleRingBuffer.h"
#include "gSound.h"

#include <cstring>

struct gVideoAudioContext {
	ma_data_source_base datasource;
	gAudioSampleRingBuffer* buffer;
	ma_uint32 samplerate;
	ma_uint32 channels;
	ma_sound sound;
	bool initialized{false};
};

static ma_result gVideoAudio_onRead(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead) {
	auto* ctx = reinterpret_cast<gVideoAudioContext*>(pDataSource);
	size_t samplesneeded = static_cast<size_t>(frameCount) * ctx->channels;
	size_t samplesread = ctx->buffer->read(static_cast<float*>(pFramesOut), samplesneeded);

	// Fill remaining with silence
	size_t remaining = samplesneeded - samplesread;
	if(remaining > 0) {
		std::memset(static_cast<float*>(pFramesOut) + samplesread, 0, remaining * sizeof(float));
	}

	if(pFramesRead) *pFramesRead = frameCount;
	return MA_SUCCESS;
}

static ma_result gVideoAudio_onSeek(ma_data_source* pDataSource, ma_uint64 frameIndex) {
	return MA_SUCCESS;
}

static ma_result gVideoAudio_onGetDataFormat(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap) {
	auto* ctx = reinterpret_cast<gVideoAudioContext*>(pDataSource);
	if(pFormat) *pFormat = ma_format_f32;
	if(pChannels) *pChannels = ctx->channels;
	if(pSampleRate) *pSampleRate = ctx->samplerate;
	if(pChannelMap) ma_channel_map_init_standard(ma_standard_channel_map_default, pChannelMap, channelMapCap, ctx->channels);
	return MA_SUCCESS;
}

static ma_result gVideoAudio_onGetCursor(ma_data_source* pDataSource, ma_uint64* pCursor) {
	if(pCursor) *pCursor = 0;
	return MA_SUCCESS;
}

static ma_result gVideoAudio_onGetLength(ma_data_source* pDataSource, ma_uint64* pLength) {
	if(pLength) *pLength = 0;
	return MA_NOT_IMPLEMENTED;
}

static ma_data_source_vtable g_videoaudiovtable = {
		gVideoAudio_onRead,
		gVideoAudio_onSeek,
		gVideoAudio_onGetDataFormat,
		gVideoAudio_onGetCursor,
		gVideoAudio_onGetLength};

gipFFmpegVideo::gipFFmpegVideo() {
}

gipFFmpegVideo::~gipFFmpegVideo() {
	cleanupAudio();
}

void gipFFmpegVideo::load(std::string fullPath) {
	filepath = fullPath;
	videostate = gLoadVideoStateFromStorage(fullPath);

	if(!videostate->iscreated) {
		gLoge("Could not open video file at: " + fullPath);
	}

	fpsquotient = static_cast<float>(videostate->avgfps) / appmanager->getTargetFramerate();

	framebuffer = new gTexture(videostate->width, videostate->height, GL_RGBA);
	initAudio();
}

void gipFFmpegVideo::loadVideo(std::string videoPath) {
	load(gGetVideosDir() + videoPath);
}

void gipFFmpegVideo::update() {
	if(videostate->isfinished || ispaused || !videostate->iscreated || !isplaying) {
		return;
	}
	if(currentframe >= videostate->framecount) {
		close();
		return;
	}

	// Decode enough packets to keep buffers fed even at low app FPS.
	// We need (video_fps + audio_frames_per_sec) / app_fps packets per update.
	// Use a generous multiplier to handle subtitle/other discarded packets.
	float appfps = appmanager->getFramerate();
	if(appfps < 1.0f) appfps = 1.0f;
	int maxpackets = std::max(8, static_cast<int>(videostate->avgfps * 5.0f / appfps));
	for (int i = 0; i < maxpackets; i++) {
		if(gAdvanceFramesUntilBufferFull(videostate)) break;
	}

	if(!videostate->readytoplay) return;

	// Start audio playback once buffers are ready (deferred from play())
	if(!audiostarted && audiocontext && audiocontext->initialized) {
		ma_sound_start(&audiocontext->sound);
		audiostarted = true;
	}

	if(videostate->hasaudio && audiocontext && audiocontext->initialized) {
		// PTS-based sync: video follows audio clock
		double audioclock = gGetAudioClock(videostate);
		if(audioclock < 0.0) return;

		double framethreshold = 0.5 / videostate->avgfps;

		// Skip frames that are behind the audio clock
		while(true) {
			double nextpts = gPeekNextVideoFramePts();
			if(nextpts < 0.0) {
				break;
			}

			double diff = nextpts - audioclock;
			if(diff < -framethreshold) {
				// Video is behind - skip this frame
				gFetchVideoFrameToState(videostate);
				gClearLastFrame(videostate);
				currentframe++;
				continue;
			}

			if(diff <= framethreshold) {
				// Close enough - display this frame
				gFetchVideoFrameToState(videostate);
				framebuffer->setData(videostate->videoframepixeldata.get(), videostate->width, videostate->height, false, false);
				currentframe++;
				gClearLastFrame(videostate);
			}
			// else: video is ahead of audio, wait
			break;
		}
	} else {
		// No audio - fall back to FPS quotient sync
		if(fpsquotientcumulative >= 1.0f) {
			fpsquotientcumulative -= 1.0f;
			gFetchVideoFrameToState(videostate);
			framebuffer->setData(videostate->videoframepixeldata.get(), videostate->width, videostate->height, false, false);
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

	isplaying = true;
}

void gipFFmpegVideo::stop() {
	close();

	videostate = gLoadVideoStateFromStorage(filepath);
	initAudio();

	isplaying = false;
	audiostarted = false;
}

void gipFFmpegVideo::setPaused(bool t_isPaused) {
	ispaused = t_isPaused;
	if(audiocontext && audiocontext->initialized && audiostarted) {
		if(t_isPaused) {
			ma_sound_stop(&audiocontext->sound);
		} else {
			ma_sound_start(&audiocontext->sound);
		}
	}
}

void gipFFmpegVideo::close() {
	cleanupAudio();
	gClearVideoState(videostate);
	gClearLastFrame(videostate);
	videostate->isfinished = true;
	audiostarted = false;
}

void gipFFmpegVideo::setPosition(float t_timeInSeconds) {
	// Stop audio during re-buffering after seek
	if(audiocontext && audiocontext->initialized && audiostarted) {
		ma_sound_stop(&audiocontext->sound);
	}
	audiostarted = false;
	gSeekToFrame(videostate, t_timeInSeconds);
	currentframe = 0;
}

double gipFFmpegVideo::getPosition() {
	if(!videostate->iscreated) return 0.0;
	return videostate->currentvideopts;
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
	if(audiocontext && audiocontext->initialized) {
		ma_sound_set_volume(&audiocontext->sound, volume);
	}
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

void gipFFmpegVideo::initAudio() {
	if(!videostate->hasaudio || !videostate->audiobuffer) return;

	audiocontext = new gVideoAudioContext();
	audiocontext->buffer = videostate->audiobuffer;
	audiocontext->samplerate = videostate->audiosamplerate;
	audiocontext->channels = 2;

	ma_data_source_config dsconfig = ma_data_source_config_init();
	dsconfig.vtable = &g_videoaudiovtable;

	if(ma_data_source_init(&dsconfig, &audiocontext->datasource) != MA_SUCCESS) {
		gLoge("gipFFmpegVideo") << "Failed to init audio data source";
		delete audiocontext;
		audiocontext = nullptr;
		return;
	}

	if(ma_sound_init_from_data_source(gGetSoundEngine(), &audiocontext->datasource,
									   MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &audiocontext->sound) != MA_SUCCESS) {
		gLoge("gipFFmpegVideo") << "Failed to init ma_sound from data source";
		ma_data_source_uninit(&audiocontext->datasource);
		delete audiocontext;
		audiocontext = nullptr;
		return;
	}

	ma_sound_set_volume(&audiocontext->sound, volume);
	audiocontext->initialized = true;
}

void gipFFmpegVideo::cleanupAudio() {
	if(audiocontext) {
		if(audiocontext->initialized) {
			ma_sound_stop(&audiocontext->sound);
			ma_sound_uninit(&audiocontext->sound);
			ma_data_source_uninit(&audiocontext->datasource);
		}
		delete audiocontext;
		audiocontext = nullptr;
	}
}
