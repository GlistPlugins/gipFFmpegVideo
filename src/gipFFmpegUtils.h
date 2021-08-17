/*
 * gFFmpegUtils.h
 *
 *  Created on: 10 Jul 2021
 *      Author: kayra
 */

#include "gObject.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libswscale/swscale.h>
#include <inttypes.h>
#include <ao/ao.h>
}

#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000

class gipFFmpegUtils : public gObject {
public:
    gipFFmpegUtils();
    ~gipFFmpegUtils();


    bool openVideo(const char *filename, int *width, int *height, int64_t* frame_count, int64_t* duration, AVRational* time_base);
    bool loadVideoFrame(unsigned char **data_out, int64_t* pts);
    bool loadAudioFrame();
    bool seekFrame();

	struct VideoReaderState {
		int width;
		int height;

		AVFormatContext *formatcontext;
		AVCodecContext *video_cdc_ctx, *audio_cdc_ctx;
		AVFrame *video_frame, *audio_frame;
		AVPacket *video_packet, *audio_packet;
		SwsContext *sws_ctx; // for video scaling and pixel correcting only
	};

    void close();

private:
    static const int STREAM_VIDEO = 0, STREAM_AUDIO = 1, STREAM_SUBTITLE = 2;

    VideoReaderState state;

    // libao
    int driver;
    ao_sample_format sampleformat;
    AVSampleFormat av_sampleformat;
    ao_device *device;
    int buffer_size;
    uint8_t *buffer;

    int streamslist[3]; // video, audio, subtitle

    static bool checkError(int errcode);
    static bool checkNull(void *ptr, std::string errtext);

    static AVPixelFormat correct_for_deprecated_pixel_format(AVPixelFormat pix_fmt);
};
