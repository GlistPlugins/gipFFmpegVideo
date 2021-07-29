/*
 * gipFFmpegUtils.cpp
 *
 *  Created on: 10 Jul 2021
 *      Author: kayra
 */

#include "gipFFmpegUtils.h"

gipFFmpegUtils::gipFFmpegUtils() {
	streamslist[STREAM_VIDEO] = -1;
	streamslist[STREAM_AUDIO] = -1;
	streamslist[STREAM_SUBTITLE] = -1;
}

gipFFmpegUtils::~gipFFmpegUtils() {

}

bool gipFFmpegUtils::openVideo(const char *filename, int* width, int* height, int64_t* frame_count, int64_t* duration, AVRational* time_base) {
	state.formatcontext = avformat_alloc_context();

    if (checkNull(state.formatcontext, "Couldn't create create format context")) return false;

    if(checkError(avformat_open_input(&state.formatcontext, filename, NULL, NULL))) return false;

	// Try to find the video stream
	AVCodecParameters *codecparams;
	AVCodec *v_codec = NULL, *a_codec = NULL;

	for (int i = 0; i < state.formatcontext->nb_streams; i++) {
		auto stream = state.formatcontext->streams[i];

		codecparams = stream->codecpar;
		v_codec = avcodec_find_decoder(codecparams->codec_id);

		if (!v_codec ) {
			continue;
		}
		if (codecparams->codec_type == AVMEDIA_TYPE_VIDEO) {
			streamslist[STREAM_VIDEO] = i;
			state.width = codecparams->width;
			state.height = codecparams->height;
			*frame_count = stream->nb_frames;
			*duration = stream->duration;
			*time_base = stream->time_base;
			break;
		}
	}

	if (streamslist[STREAM_VIDEO] == -1) {
		loge("Valid video stream could not be created from the file");
		return false;
	}

    state.video_cdc_ctx = avcodec_alloc_context3(v_codec);
    if (checkNull(state.video_cdc_ctx, "Could not create AVCodecContext"))
    	return false;

    if (checkError(avcodec_parameters_to_context(state.video_cdc_ctx, codecparams)))
    	return false;

    if(checkError(avcodec_open2(state.video_cdc_ctx, v_codec, nullptr)))
    	return false;

    state.video_frame = av_frame_alloc();
    state.video_packet = av_packet_alloc();

    if (checkNull(state.video_frame, "Could not allocate the video frame.") ||
        checkNull(state.video_packet, "Could not allocate the packet.")) return false;

    *width = state.width;
    *height = state.height;

    return true;
}

bool gipFFmpegUtils::loadFrame(unsigned char **data_out, int64_t* pts) {

    // Wait until we got a video packet ignoring audio, etc.
    int response;
    while (av_read_frame(state.formatcontext, state.video_packet) >= 0) { // Until the EOF
        if(state.video_packet->stream_index != streamslist[STREAM_VIDEO]) {
        	av_packet_unref(state.video_packet);
            continue;
        }

        //logi("1");

        if (checkError(avcodec_send_packet(state.video_cdc_ctx, state.video_packet))) {
            return false;
        }

        //logi("2");

        response = avcodec_receive_frame(state.video_cdc_ctx, state.video_frame);
        if(response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
        	av_packet_unref(state.video_packet);
        	continue;
        } else if (checkError(response)) {
            return false;
        }

        //logi("2");

        av_packet_unref(state.video_packet);
        break;
    }
    *pts = state.video_frame->pts;

	auto corrected_pix_frmt = correct_for_deprecated_pixel_format(state.video_cdc_ctx->pix_fmt);
	state.sws_ctx = sws_getContext(	state.width, state.height, corrected_pix_frmt,
									state.width, state.height, AV_PIX_FMT_RGB0,
									SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

	if (checkNull(state.sws_ctx, "Couldn't initialize SwsContext"))
		return false;

    uint8_t *data = new uint8_t[state.width * state.height * 4];

    uint8_t* dest[4] = {data, NULL, NULL, NULL};
    int dest_linesize[4] = {state.width * 4, 0, 0, 0};

    sws_scale(state.sws_ctx, state.video_frame->data, state.video_frame->linesize, 0, state.video_frame->height, dest, dest_linesize);
    sws_freeContext(state.sws_ctx);

    *data_out = data;



    return true;
}

void gipFFmpegUtils::close() {
    avformat_close_input(&state.formatcontext);
    avformat_free_context(state.formatcontext);
    av_frame_free(&state.video_frame);
    av_packet_free(&state.video_packet);
    avcodec_free_context(&state.video_cdc_ctx);
}

AVPixelFormat gipFFmpegUtils::correct_for_deprecated_pixel_format(AVPixelFormat pix_fmt) {
    // Fix swscaler deprecated pixel format warning
    // (YUVJ has been deprecated, change pixel format to regular YUV)
    switch (pix_fmt) {
        case AV_PIX_FMT_YUVJ420P: return AV_PIX_FMT_YUV420P;
        case AV_PIX_FMT_YUVJ422P: return AV_PIX_FMT_YUV422P;
        case AV_PIX_FMT_YUVJ444P: return AV_PIX_FMT_YUV444P;
        case AV_PIX_FMT_YUVJ440P: return AV_PIX_FMT_YUV440P;
        default:                  return pix_fmt;
    }
}

bool gipFFmpegUtils::checkError(int errcode) {
    if (errcode < 0) {
        char buf[256];

        av_strerror(errcode, buf, 256);

        gLoge(std::string(buf));

        return true;
    }

    return false;
}

bool gipFFmpegUtils::checkNull(void *ptr, std::string errtext) {
    if(!ptr) {
        gLoge("checkNull") << errtext;
        return true;
    }
    return false;
}
