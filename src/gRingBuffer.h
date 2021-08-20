/*
 * gRingBuffer.h
 *
 *  Created on: 20 Aðu 2021
 *      Author: kayra
 */

#ifndef SRC_GRINGBUFFER_H_
#define SRC_GRINGBUFFER_H_

#include <atomic>

class gRingBuffer {
public:
	gRingBuffer();
	virtual ~gRingBuffer();

	void init(int size, float elements_per_sec = 1.0f);
	void destroy();

	bool canRead(int elements);
	void read(int elements, int *size_1, float **buffer_1, int *size_2, float **buffer_2);
	void readWait(int elements, int *size_1, float **buffer_1, int *size_2, float **buffer_2);
	void readAdvance(int elements);

	bool canWrite(int elements);
	void write(int elements, int *size_1, float **buffer_1, int *size_2, float **buffer_2);
	void writeWait(int elements, int *size_1, float **buffer_1, int *size_2, float **buffer_2);
	void writeAdvance(int elements);

private:
	std::atomic_int read_point;
	std::atomic_int write_point;
	int size;
	float elements_per_sec;
	float *buffer;
};

#endif /* SRC_GRINGBUFFER_H_ */
