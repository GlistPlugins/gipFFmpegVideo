/*
 * gAudioSampleRingBuffer.h
 *
 *  Created on: 11 Mar 2026
 *      Author: Metehan Gezer
 */

#ifndef G_AUDIOSAMPLERINGBUFFER_H
#define G_AUDIOSAMPLERINGBUFFER_H

#include <atomic>
#include <vector>
#include <cstddef>

/**
 * Lock-free single-producer single-consumer ring buffer for float audio samples.
 * The producer (main thread) writes resampled audio; the consumer (audio thread) reads.
 */
class gAudioSampleRingBuffer {
public:
	explicit gAudioSampleRingBuffer(size_t capacity);

	size_t write(const float* data, size_t samplecount);
	size_t read(float* data, size_t samplecount);

	size_t available() const;
	size_t space() const;
	size_t getTotalRead() const;
	void clear();

private:
	std::vector<float> buffer;
	std::atomic<size_t> readpos{0};
	std::atomic<size_t> writepos{0};
	std::atomic<size_t> totalread{0};
	size_t capacity;
};

#endif // G_AUDIOSAMPLERINGBUFFER_H