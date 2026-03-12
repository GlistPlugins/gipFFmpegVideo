/*
 * gVideoFrameRingBuffer.cpp
 *
 *  Created on: 12 Jul 2024
 *      Author: sorbatdev
 */

#include "gVideoFrameRingBuffer.h"

#include <cstddef>
#include <utility>

gVideoFrameRingBuffer::gVideoFrameRingBuffer(size_t size) : m_capacity(size), buffer(size) {
	buffer.resize(m_capacity);
}

bool gVideoFrameRingBuffer::push(uint8_t* t_ptr) {
	if(count == m_capacity) {
		gLoge("gVideoFrameRingBuffer::push") << "Reached buffer's end!\n";
		return false;// Buffer is full
	}

	std::unique_ptr<uint8_t[]> framedata(t_ptr);

	buffer[head] = std::move(framedata);
	head = (head + 1) % m_capacity;
	++count;
	return true;
}

bool gVideoFrameRingBuffer::pop(std::unique_ptr<uint8_t[]>& value) {
	if(count == 0) {
		gLoge("gVideoFrameRingBuffer::pop") << "Cannot pop. No objects inside the buffer\n";
		return false;// Buffer is empty
	}

	value = std::move(buffer[tail]);
	tail = (tail + 1) % m_capacity;
	--count;
	return true;
}

bool gVideoFrameRingBuffer::popAll() {
	tail = 0;
	head = 0;
	count = 0;
	buffer = decltype(buffer)(m_capacity);
	return true;
}

bool gVideoFrameRingBuffer::isFull() const {
	return count == m_capacity;
}

bool gVideoFrameRingBuffer::isEmpty() const {
	return count == 0;
}

size_t gVideoFrameRingBuffer::size() const {
	return count;
}

size_t gVideoFrameRingBuffer::capacity() const {
	return m_capacity;
}
