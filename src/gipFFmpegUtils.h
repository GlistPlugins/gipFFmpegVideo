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
}


class gipFFmpegUtils : public gObject {
public:
    gipFFmpegUtils();
    ~gipFFmpegUtils();


    bool openVideo(const char *filename, int *width, int *height, int64_t* frame_count, int64_t* duration, AVRational* time_base);
    bool loadFrame(unsigned char **data_out, int64_t* pts);
    bool seekFrame();

	struct VideoReaderState {
		int width;
		int height;

		AVFormatContext *formatcontext;
		AVCodecContext *video_cdc_ctx, *audio_cdc_ctx;
		AVFrame *video_frame, *audio_frame;
		AVPacket *video_packet, *audio_packet;
		SwsContext *sws_ctx;
	};

    void close();

private:
    static const int STREAM_VIDEO = 0, STREAM_AUDIO = 1, STREAM_SUBTITLE = 2;

    VideoReaderState state;

    int streamslist[3]; // video, audio, subtitle

    static bool checkError(int errcode);
    static bool checkNull(void *ptr, std::string errtext);

    static AVPixelFormat correct_for_deprecated_pixel_format(AVPixelFormat pix_fmt);
};
