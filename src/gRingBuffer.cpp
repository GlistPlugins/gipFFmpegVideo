/*
 * gRingBuffer.cpp
 *
 *  Created on: 20 Aðu 2021
 *      Author: kayra
 */

#include <gRingBuffer.h>
#include <time.h>

gRingBuffer::gRingBuffer() {
}

gRingBuffer::~gRingBuffer() {
	destroy();
}

void gRingBuffer::init(int size, float elements_per_sec) {
	this->size = size;
	this->elements_per_sec = elements_per_sec;
	read_point = 0;
	write_point = 0;
	buffer = new float[size];
}

void gRingBuffer::destroy() {
	delete buffer;
}

bool gRingBuffer::canRead(int elements) {
	return read_point + elements <= write_point;
}

void gRingBuffer::read(int elements, int *size_1, float **buffer_1, int *size_2, float **buffer_2) {
	int read_point_in_buffer = read_point % size;

	int size_1_max = size - read_point_in_buffer;
	*size_1 = size_1_max < elements ? size_1_max : elements;
	*buffer_1 = &buffer[read_point_in_buffer];
	*size_2 = elements - *size_1;
	*buffer_2 = &buffer[0];
}

void gRingBuffer::readWait(int elements, int *size_1, float **buffer_1, int *size_2, float **buffer_2) {
	int available;
	int remaining;
	int sleep_time;
	while(true) {
		available = write_point - read_point;
		if(available >= elements) {
			break;
		}

		remaining = elements - available;
		sleep_time = 0.25f * remaining / elements_per_sec;
		struct timespec ts;
		ts.tv_sec = (int)sleep_time;
		ts.tv_nsec = (sleep_time - ts.tv_sec) * 1000000000;
		nanosleep(&ts, NULL);
	}
	read(elements, size_1, buffer_1, size_2, buffer_2);
}

void gRingBuffer::readAdvance(int elements) {
	read_point += elements;
}

bool gRingBuffer::canWrite(int elements) {
	return write_point + elements <= read_point + size;
}

void gRingBuffer::write(int elements, int *size_1, float **buffer_1, int *size_2, float **buffer_2) {
	int write_point_in_buffer = write_point % size;

	int size_1_max = size - write_point_in_buffer;
	*size_1 = size_1_max < elements ? size_1_max : elements;
	*buffer_1 = &buffer[write_point_in_buffer];
	*size_2 = elements - *size_1;
	*buffer_2 = &buffer[0];
}

void gRingBuffer::writeWait(int elements, int *size_1, float **buffer_1, int *size_2, float **buffer_2) {
	int available;
	int remaining;
	int sleep_time;
	while(true) {
		available = read_point + size - write_point;
		if(available >= elements) {
			break;
		}

		remaining = elements - available;
		sleep_time = 0.25f * remaining / elements_per_sec;
		struct timespec ts;
		ts.tv_sec = (int)sleep_time;
		ts.tv_nsec = (sleep_time - ts.tv_sec) * 1000000000;
		nanosleep(&ts, NULL);
	}
	write(elements, size_1, buffer_1, size_2, buffer_2);
}

void gRingBuffer::writeAdvance(int elements) {
	write_point += elements;
}
