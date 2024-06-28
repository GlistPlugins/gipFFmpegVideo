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
#include <inttypes.h>
}
class gipFFmpegUtils : public gObject {
public:
    static const int RECEIVED_NONE = 0, RECEIVED_VIDEO = -1;

    struct VideoReaderState {
    	int width;
    	int height;

    	AVFormatContext *formatcontext;
    	AVSampleFormat sample_format;
    	AVPixelFormat pixel_format;
    	AVPacket *av_packet;

    	//	Video
    	AVCodecContext *video_cdc_ctx;
    	AVFrame *video_frame;
    	SwsContext *sws_ctx; // for video scaling and pixel correcting only
    };

    gipFFmpegUtils();
    ~gipFFmpegUtils();

    bool openVideo(const char *filename, int *width, int *height, int64_t* frame_count, int64_t* duration, AVRational* time_base);
    int read_frame();
    void fetch_video_frame(uint8_t **data_out, int64_t* pts);
    bool seekFrame(int64_t time_stamp_in_secs);

	gipFFmpegUtils::VideoReaderState* getState();

    void close();

private:
    static const int STREAM_VIDEO = 0, STREAM_AUDIO = 1, STREAM_SUBTITLE = 2;

    VideoReaderState state;

    int streamslist[3]; // video, audio, subtitle


    static bool isError(int errcode);
    static bool checkNull(void *ptr, std::string errtext);

    static AVPixelFormat correct_for_deprecated_pixel_format(AVPixelFormat pix_fmt);
    std::vector<char*> splitString(const char* str, const char delimiter);
    uint8_t *data;
    uint8_t* dest[4];
    int dest_linesize[4];
};

#endif /* GIP_FFMPEG_UTILS_H */
