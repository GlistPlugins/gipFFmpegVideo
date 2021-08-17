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
	AVCodecParameters *v_codecparams;
	AVCodec *v_codec = NULL;

	for (int i = 0; i < state.formatcontext->nb_streams; i++) {
		auto stream = state.formatcontext->streams[i];

		v_codecparams = stream->codecpar;
		v_codec = avcodec_find_decoder(v_codecparams->codec_id);

		if (!v_codec ) {
			continue;
		}
		if (v_codecparams->codec_type == AVMEDIA_TYPE_VIDEO) {
			streamslist[STREAM_VIDEO] = i;
			state.width = v_codecparams->width;
			state.height = v_codecparams->height;
			*frame_count = stream->nb_frames;
			*duration = stream->duration;
			*time_base = stream->time_base;
			break;
		}
	}

	// Find audio stream
	AVCodecParameters *a_codecparams;
	AVCodec *a_codec = nullptr;

	for (int i = 0; i < state.formatcontext->nb_streams; i++) {
		auto stream = state.formatcontext->streams[i];

		a_codecparams = stream->codecpar;
		a_codec = avcodec_find_decoder(a_codecparams->codec_id);

		if (!a_codec ) {
			continue;
		}
		if (a_codecparams->codec_type == AVMEDIA_TYPE_AUDIO) {
			streamslist[STREAM_AUDIO] = i;
			break;
		}
	}

	if (streamslist[STREAM_VIDEO] == -1) {
		loge("Valid video stream could not be created from the file");
		return false;
	}
	if (streamslist[STREAM_AUDIO] == -1) {
		loge("Valid audio stream could not be created from the file");
		return false;
	}

	// Initialize codec context for the video
    state.video_cdc_ctx = avcodec_alloc_context3(v_codec);
    if (checkNull(state.video_cdc_ctx, "Could not create AVCodecContext for the video."))
    	return false;

    // Initialize codec context for the audio
    state.audio_cdc_ctx = avcodec_alloc_context3(a_codec);
    if (checkNull(state.audio_cdc_ctx, "Could not create AVCodecContext for the audio."))
        return false;

    // Turn video codec parameters to codec context
    if (checkError(avcodec_parameters_to_context(state.video_cdc_ctx, v_codecparams)))
    	return false;
    // Turn audio codec parameters to codec context
    if (checkError(avcodec_parameters_to_context(state.audio_cdc_ctx, a_codecparams)))
    	return false;

    // Initialize the video codec context to use the codec for the video
    if(checkError(avcodec_open2(state.video_cdc_ctx, v_codec, nullptr)))
    	return false;
    // Initialize the audio codec context to use the codec for the audio
    if(checkError(avcodec_open2(state.audio_cdc_ctx, a_codec, nullptr)))
    	return false;

    state.video_frame = av_frame_alloc();
    state.video_packet = av_packet_alloc();

    state.audio_frame = av_frame_alloc();
    state.audio_packet = av_packet_alloc();

    if (checkNull(state.video_frame, "Could not allocate the video frame!") ||
        checkNull(state.video_packet, "Could not allocate the video packet!") ||
		checkNull(state.audio_frame, "Could not allocate the audio frame!") ||
		checkNull(state.audio_packet, "Could not allocate the audio packet!")
		)
    	return false;

    // libao Initialization
	ao_initialize();

	driver = ao_default_driver_id();
	av_sampleformat = state.audio_cdc_ctx->sample_fmt;
	gLogi("sample format") << "bits:" << static_cast<int>(av_sampleformat);
    if(av_sampleformat == AV_SAMPLE_FMT_U8){
        printf("U8\n");

        sampleformat.bits = 8;
    }else if(av_sampleformat == AV_SAMPLE_FMT_S16){
        printf("S16\n");
        sampleformat.bits = 16;
    }else if(av_sampleformat == AV_SAMPLE_FMT_S32){
        printf("S32\n");
        sampleformat.bits = 32;
    }else if(av_sampleformat == AV_SAMPLE_FMT_FLTP) {
    	sampleformat.bits = 32;
    }

    sampleformat.channels = state.audio_cdc_ctx->channels;
    sampleformat.rate = state.audio_cdc_ctx->sample_rate;
    sampleformat.byte_format = AO_FMT_NATIVE;
    sampleformat.matrix = 0;
    device = ao_open_live(driver, &sampleformat, nullptr);
    buffer_size = AVCODEC_MAX_AUDIO_FRAME_SIZE + AV_INPUT_BUFFER_PADDING_SIZE;
    buffer = new uint8_t[buffer_size];
    state.audio_packet->data = buffer;
    state.audio_packet->size = buffer_size;


    *width = state.width;
    *height = state.height;

    return true;
}

bool gipFFmpegUtils::loadVideoFrame(unsigned char **data_out, int64_t* pts) {

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


bool gipFFmpegUtils::loadAudioFrame() {

    int frameFinished;

	while(av_read_frame(state.formatcontext, state.audio_packet) >= 0) {
        if(state.audio_packet->stream_index == streamslist[STREAM_AUDIO]){
        	av_packet_unref(state.video_packet);
            continue;
        }
        logi("1");

        if (checkError(avcodec_send_packet(state.audio_cdc_ctx, state.audio_packet))) {
        	return false;
        }

        logi("2");

        frameFinished = avcodec_receive_frame(state.audio_cdc_ctx, state.audio_frame);

        logi("3");

        if(frameFinished == AVERROR(EAGAIN) || frameFinished == AVERROR_EOF) {
        	av_packet_unref(state.audio_packet);
        	logi("some error, but still continue");
        	continue;
        } else if (checkError(frameFinished)) {
        	logi("error");
        	return false;
        }

        logi("4");

        if(frameFinished){
        	gLogi("Frame finished") << "playing";
        	ao_play(device, (char*)state.audio_frame->extended_data[0], state.audio_frame->linesize[0] );
        }else{
        	//printf("Not Finished\n");
        }

        logi("5");

	}

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
