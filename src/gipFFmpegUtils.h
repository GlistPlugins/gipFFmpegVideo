/*
 * gFFmpegUtils.h
 *
 *  Created on: 10 Jul 2021
 *      Author: kayra
 *      Edited By: Umutcan Türkmen 24 Feb 2023
 */

#ifndef GIP_FFMPEG_UTILS_H
#define GIP_FFMPEG_UTILS_H

#include "gObject.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

class gAudioSampleRingBuffer;
class gVideoFrameRingBuffer;

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <utility>

enum class FrameType {
    FRAMETYPE_VIDEO
  , FRAMETYPE_AUDIO
  , FRAMETYPE_NONE
};

struct VideoState {
    bool iscreated{false};
    bool readytoplay{false};
    bool isfinished{false};

    AVFormatContext* formatcontext{};
    AVPacket*        currentpacket{};
    AVRational       timebase{};

    std::array<int, 3> streamindices;

    //	Video
    AVCodecContext* videocodeccontext{};
    AVFrame*        videoframe{};
    SwsContext*     swscontext{};
    int32_t         width{};
    int32_t         height{};
    int64_t         framecount{};
    int64_t         framesprocessed{};
    int64_t         duration{};
    int32_t         numpixelcomponents;

    //	Audio
    AVCodecContext* audiocodeccontext{};
    AVFrame*        audioframe{};
    SwrContext*     swrcontext{};
    gAudioSampleRingBuffer* audiobuffer{};
    AVRational      audiotimebase{};
    int32_t         audiochannelsnum{};
    int32_t         audiosamplerate{};
    bool            hasaudio{false};

    // PTS sync
    std::atomic<double> audioclocksyncpts{0.0};
    std::atomic<bool>   audioclockvalid{false};
    double              currentvideopts{0.0};

    // Buffering
    bool preloaded{false};
    int bufferthreshold{4}; // frames to buffer before starting playback
    bool dimensionchanged{false};

    std::unique_ptr<uint8_t[]> videoframepixeldata{};
    double                     avgfps{};

    // Per-instance frame buffer
    gVideoFrameRingBuffer* framesbuffer{};
    std::deque<double> framepts;
    std::deque<std::pair<int32_t, int32_t>> framedimensions;
    bool needallocate{true};
    int32_t decodewidth{0};
    int32_t decodeheight{0};

    FrameType lastreceivedframetype;

    // Factory
    static std::shared_ptr<VideoState> loadFromStorage(std::string const& filename);

    // Lifecycle
    void clear();
    void clearLastFrame();

    // Decode / buffer
    bool advanceFramesUntilBufferFull();
    void addFrameToBuffer();
    void addAudioToBuffer();
    void fetchVideoFrame();
    bool seekToFrame(float timeStampInSec);

    // Queries
    double getAudioClock();
    double peekNextVideoFramePts();

    // Configuration
    void setPreloaded(size_t maxmemorybytes);
    void setBufferDuration(float seconds);
};

AVPixelFormat gGetCorrectedPixelFormat(AVPixelFormat l_pixelFormat);

#endif /* GIP_FFMPEG_UTILS_H */
