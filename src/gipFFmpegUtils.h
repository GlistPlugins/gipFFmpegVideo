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
#include <inttypes.h>
}
#include "gipAVSound.h"

#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000

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

    	// 	Audio
    	AVCodecContext *audio_cdc_ctx;
    	AVFrame *audio_frame;
    	int num_channels, sample_rate;

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
    void fetch_audio_frame(int size_1, float *buffer_1, int size_2, float *buffer_2);
    void fetch_audio_frame_sub(int size, float* buffer, int offset);
    bool seekFrame(int64_t time_stamp_in_secs);

	gipAVSound* getAudio();

	gipFFmpegUtils::VideoReaderState* getState();

    void close();

private:
    static const int STREAM_VIDEO = 0, STREAM_AUDIO = 1, STREAM_SUBTITLE = 2;

    VideoReaderState state;
    gipAVSound audio;

    int streamslist[3]; // video, audio, subtitle


    static bool isError(int errcode);
    static bool checkNull(void *ptr, std::string errtext);

    static void audio_callback(int num_channels, int buffer_size, float* buffer, void* user_ptr);

    static AVPixelFormat correct_for_deprecated_pixel_format(AVPixelFormat pix_fmt);
    std::vector<char*> splitString(const char* str, const char delimiter);
    uint8_t *data;
    uint8_t* dest[4];
    int dest_linesize[4];
};

#endif /* GIP_FFMPEG_UTILS_H */
