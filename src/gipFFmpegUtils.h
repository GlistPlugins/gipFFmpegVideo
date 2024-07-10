/*
 * gFFmpegUtils.h
 *
 *  Created on: 10 Jul 2021
 *      Author: kayra
 *      Edited By: Umutcan T�rkmen 24 Feb 2023
 */

#ifndef GIP_FFMPEG_UTILS_H
#define GIP_FFMPEG_UTILS_H

#include "gObject.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libswscale/swscale.h>
}

#include <array>
#include <cstdint>
#include <memory>
#include <utility>

enum class FrameType {
    FRAMETYPE_VIDEO
  , FRAMETYPE_AUDIO
  , FRAMETYPE_NONE
};

struct VideoState {
    bool iscreated{};

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
    int64_t         duration{};

    //	Audio
    AVCodecContext* audiocodeccontext{};
    AVFrame*        audioframe{};
    int32_t         audiochannelsnum{};
    int32_t         audiosamplerate{};

    uint8_t*                videoframepixeldata{};
    int64_t                 totalframecount{}; // this may not be present
    std::array<uint8_t*, 4> destination;
    std::array<int32_t,  4> destinationlinesize;


    FrameType lastreceivedframetype;
};

std::shared_ptr<VideoState> gLoadVideoStateFromStorage(std::string const& t_filename);
void gClearVideoState(std::shared_ptr<VideoState> l_state);

void gAdvanceFrameInPacket(std::shared_ptr<VideoState> l_state);
void gFetchVideoFrameToState(std::shared_ptr<VideoState> l_state);
bool gSeekToFrame(std::shared_ptr<VideoState> l_state, int64_t t_timeStampInSec);

void          gAllocateStorageForVideoFrame(std::shared_ptr<VideoState> l_state);
AVPixelFormat gGetCorrectedPixelFormat(AVPixelFormat l_pixelFormat);

#endif /* GIP_FFMPEG_UTILS_H */
