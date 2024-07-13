/*
 * gVideoFrameRingBuffer.cpp
 *
 *  Created on: 12 Jul 2024
 *      Author: sorbatdev
 */

#include "gVideoFrameRingBuffer.h"

#include <cstddef>
#include <utility>

gVideoFrameRingBuffer::gVideoFrameRingBuffer(size_t size)
    : m_capacity(size), m_buffer(size)
{
    m_buffer.resize(m_capacity);
}

bool gVideoFrameRingBuffer::push(uint8_t* t_ptr)
{
    if (m_count == m_capacity) {
        gLoge("gVideoFrameRingBuffer::push") << "Reached buffer's end!\n";
        return false;  // Buffer is full
    }

    std::unique_ptr<uint8_t[]> framedata(t_ptr);

    m_buffer[m_head] = std::move(framedata);
    m_head = (m_head + 1) % m_capacity;
    ++m_count;
    return true;
}

bool gVideoFrameRingBuffer::pop(std::unique_ptr<uint8_t[]>& value)
{
    if (m_count == 0) {
        gLoge("gVideoFrameRingBuffer::pop") << "Cannot pop. No objects inside the buffer\n";
        return false;  // Buffer is empty
    }

    value = std::move(m_buffer[m_tail]);
    m_tail = (m_tail + 1) % m_capacity;
    --m_count;
    return true;
}

bool gVideoFrameRingBuffer::isFull() const
{
    return m_count == m_capacity;
}

bool gVideoFrameRingBuffer::isEmpty() const
{
    return m_count == 0;
}

size_t gVideoFrameRingBuffer::size() const
{
    return m_count;
}

size_t gVideoFrameRingBuffer::capacity() const
{
    return m_capacity;
}
