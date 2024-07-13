/*
 * gVideoFrameRingBuffer.h
 *
 *  Created on: 12 Jul 2024
 *      Author: sorbatdev
 */

#ifndef G_VIDEOFRAMERINGBUFFER_H
#define G_VIDEOFRAMERINGBUFFER_H

#include <vector>
#include <cstddef>

#include "gObject.h"

class gVideoFrameRingBuffer : public gObject{
public:
    explicit gVideoFrameRingBuffer(size_t size);

    bool push(uint8_t* t_ptr);
    bool pop(std::unique_ptr<uint8_t[]>& value);

    bool isFull() const;
    bool isEmpty() const;
    size_t size() const;
    size_t capacity() const;
private:
    std::vector<std::unique_ptr<uint8_t[]>> m_buffer;
    size_t m_head{0};
    size_t m_tail{0};
    size_t m_count{0};
    const size_t m_capacity;
};

#endif // G_VIDEOFRAMERINGBUFFER_H