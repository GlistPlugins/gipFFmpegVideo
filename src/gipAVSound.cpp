/*
 * gipAVSound.cpp
 *
 *  Created on: 19 Aðu 2021
 *      Author: kayra
 */

#include "gipAVSound.h"

gipAVSound::AudioCallback gipAVSound::usr_callback;

gipAVSound::gipAVSound() {

	err = Pa_Initialize();

	isError(err);

	is_initialized = true;
}

gipAVSound::~gipAVSound() {
	// TODO Auto-generated destructor stub
}

void gipAVSound::start(int num_channels, int buffer_size, float sample_rate , AudioCallback callback) {
	if(!is_initialized) return;

	usr_callback = callback;
	audio_info.num_channels = num_channels;
	audio_info.ring_buffer = &ring_buffer;

	err = Pa_OpenDefaultStream(&audio_info.stream, 0, num_channels, paFloat32, sample_rate, buffer_size, pa_callback, (void*)&audio_info);
	isError(err);

	err = Pa_StartStream(audio_info.stream);
	isError(err);
}

void gipAVSound::stop() {
	err = Pa_StopStream(audio_info.stream);
	isError(err);
	Pa_Terminate();
}

void gipAVSound::loadSound(std::string soundPath) {

}

void gipAVSound::update() {

}

void gipAVSound::isError(PaError error_code) {
	if(error_code != paNoError) {
		gLoge("PortAudio error:") << Pa_GetErrorText(error_code);
		exit(DEBUG_ERROR_RETURN);
	}
}

int gipAVSound::pa_callback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *audio_info) {
	auto buffer = (float*)output;
	AudioInfo *info = (AudioInfo*)audio_info;

//	buffer => 64 (buffer_size) * 2 (num_channels)
//	buffer[0] => first sample, LEFT speaker
//	buffer[1] => first sample, RIGHT speaker
//	buffer[2] => second sample, LEFT speaker
//	...

	usr_callback(info->num_channels, frameCount, buffer, (void*)info->ring_buffer);

	return paContinue;
}


gRingBuffer* gipAVSound::getRingBuffer() {
	return &ring_buffer;
}
