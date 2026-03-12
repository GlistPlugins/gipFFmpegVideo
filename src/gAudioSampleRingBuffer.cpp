/*
 * gAudioSampleRingBuffer.cpp
 *
 *  Created on: 11 Mar 2026
 *      Author: Metehan Gezer
 */

#include "gAudioSampleRingBuffer.h"

#include <algorithm>
#include <cstring>

gAudioSampleRingBuffer::gAudioSampleRingBuffer(size_t capacity)
	: capacity(capacity + 1) // +1 to distinguish full from empty
	, buffer(capacity + 1, 0.0f) {
}

size_t gAudioSampleRingBuffer::available() const {
	size_t w = writepos.load(std::memory_order_acquire);
	size_t r = readpos.load(std::memory_order_acquire);
	if(w >= r) return w - r;
	return capacity - r + w;
}

size_t gAudioSampleRingBuffer::space() const {
	return capacity - 1 - available();
}

size_t gAudioSampleRingBuffer::write(const float* data, size_t samplecount) {
	size_t avail = space();
	size_t towrite = std::min(samplecount, avail);
	if(towrite == 0) return 0;

	size_t wp = writepos.load(std::memory_order_relaxed);

	size_t firstchunk = std::min(towrite, capacity - wp);
	std::memcpy(&buffer[wp], data, firstchunk * sizeof(float));

	size_t secondchunk = towrite - firstchunk;
	if(secondchunk > 0) {
		std::memcpy(&buffer[0], data + firstchunk, secondchunk * sizeof(float));
	}

	writepos.store((wp + towrite) % capacity, std::memory_order_release);
	return towrite;
}

size_t gAudioSampleRingBuffer::read(float* data, size_t samplecount) {
	size_t avail = available();
	size_t toread = std::min(samplecount, avail);
	if(toread == 0) return 0;

	size_t rp = readpos.load(std::memory_order_relaxed);

	size_t firstchunk = std::min(toread, capacity - rp);
	std::memcpy(data, &buffer[rp], firstchunk * sizeof(float));

	size_t secondchunk = toread - firstchunk;
	if(secondchunk > 0) {
		std::memcpy(data + firstchunk, &buffer[0], secondchunk * sizeof(float));
	}

	readpos.store((rp + toread) % capacity, std::memory_order_release);
	totalread.fetch_add(toread, std::memory_order_relaxed);
	return toread;
}

size_t gAudioSampleRingBuffer::getTotalRead() const {
	return totalread.load(std::memory_order_relaxed);
}

void gAudioSampleRingBuffer::clear() {
	readpos.store(0, std::memory_order_release);
	writepos.store(0, std::memory_order_release);
	totalread.store(0, std::memory_order_release);
}