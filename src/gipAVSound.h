/*
 * gipAVSound.h
 *
 *  Created on: 19 Aðu 2021
 *      Author: kayra
 */

#ifndef SRC_GIPAVSOUND_H_
#define SRC_GIPAVSOUND_H_

extern "C" {
#include "portaudio.h"
#include "stdlib.h"
}
#include "gBasePlugin.h"
#include "gRingBuffer.h"

#define DEBUG

#define AV_SAMPLE_RATE 44100.0f
#define NUM_CHANNELS 2

class gipAVSound : public gBasePlugin{
private:
	struct AudioInfo {
		PaStream* stream;
		int num_channels;
		int buffer_size;
		float sample_rate;
		gRingBuffer* ring_buffer;
	} audio_info;

	typedef void (*AudioCallback)(int num_channels, int buffer_size, float* buffer, void* user_ptr);

public:
	gipAVSound();
	virtual ~gipAVSound();

	void start(int num_channels, int buffer_size, float sample_rate, AudioCallback callback);
	void stop();

	void loadSound(std::string soundPath);

	void update();

	gRingBuffer* getRingBuffer();

private:
	void isError(PaError error_code);

	bool is_initialized;
	static AudioCallback usr_callback;
	gRingBuffer	ring_buffer;

	static int pa_callback(	const void *input, void *output,
							unsigned long frameCount,
							const PaStreamCallbackTimeInfo* timeInfo,
							PaStreamCallbackFlags statusFlags, void *audio_info);
#ifdef DEBUG
	static const int DEBUG_ERROR_RETURN = -2;
	PaError err;
#endif
};

#endif /* SRC_GIPAVSOUND_H_ */
