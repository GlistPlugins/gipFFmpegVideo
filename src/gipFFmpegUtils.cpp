/*
 * gipFFmpegUtils.cpp
 *
 *  Created on: 10 Jul 2021
 *      Author: kayra
 *      Edited By: Umutcan T�rkmen 24 Feb 2023
 */

#include "gipFFmpegUtils.h"
#include <cmath>
gipFFmpegUtils::gipFFmpegUtils() {
	streamslist[STREAM_VIDEO] = -1;
	streamslist[STREAM_AUDIO] = -1;
	streamslist[STREAM_SUBTITLE] = -1;
}

gipFFmpegUtils::~gipFFmpegUtils() {
	delete data;
}

bool gipFFmpegUtils::openVideo(const char *filename, int* width, int* height, int64_t* frame_count, int64_t* duration, AVRational* time_base) {
	state.formatcontext = avformat_alloc_context();

    if (nullptr == state.formatcontext) 
	{
		gLoge("gipFFmpegUtils::openVideo") << "Couldn't create create format context.";
		return false;
	}

	int averror = avformat_open_input(&state.formatcontext, filename, nullptr, nullptr);
    if(averror < 0)
	{
		gLoge("gipFFmpegUtils::openVideo") << av_err2str(averror);
		return false;
	}

	averror = avformat_find_stream_info(state.formatcontext, nullptr);
	if (averror < 0) {
        gLoge("gipFFmpegUtils::openVideo") << av_err2str(averror);
        return false;
    }

	//	Find the needed streams
	AVCodecParameters *video_codec_params;
//	AVCodecParameters *audio_codec_params;
	const AVCodec *video_codec;
//	const AVCodec *audio_codec;
	for (int i = 0; i < state.formatcontext->nb_streams; i++) {
		auto* stream = state.formatcontext->streams[i];

		auto* codec_params = stream->codecpar;

		if (nullptr == codec_params) {
            gLoge("gipFFmpegUtils::openVideo") << "Stream " << i << " has null codec parameters.";
            continue;
        }


		if (streamslist[STREAM_VIDEO] == -1 && codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
			streamslist[STREAM_VIDEO] = i;
			video_codec_params = codec_params;
			video_codec = avcodec_find_decoder(codec_params->codec_id);

			gLogd("gipFFmpegUtils::openVideo") << "Stream " << i << ": codec_id = "
								<< avcodec_get_name(codec_params->codec_id);

			state.height = video_codec_params->height;
			state.width = video_codec_params->width;

			*frame_count = stream->nb_frames;
			*duration = stream->duration;
			*time_base = stream->time_base;
			continue;
		}
		if(streamslist[STREAM_AUDIO] == -1 && codec_params->codec_type == AVMEDIA_TYPE_AUDIO) {
			streamslist[STREAM_AUDIO] = i;
			gLogd("gipFFmpegUtils::openVideo") << "Stream " << i << ": codec_id = "
					<< avcodec_get_name(codec_params->codec_id);
//			audio_codec_params = codec_params;
//			audio_codec = avcodec_find_decoder(codec_params->codec_id);;
//			state.num_channels = audio_codec_params->channels;
//			state.sample_rate = audio_codec_params->sample_rate;
//			continue;
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

//	if(state.num_channels != 2) {
//		gLoge("Only stereo!");
//	}

//	audio.getRingBuffer()->init(8192, state.sample_rate * NUM_CHANNELS);
//	audio.start(NUM_CHANNELS, 512, state.sample_rate, audio_callback);

	// Initialize codec context for the video
    state.video_cdc_ctx = avcodec_alloc_context3(video_codec);
    if (checkNull(state.video_cdc_ctx, "Could not create AVCodecContext for the video."))
    	return false;

    // Initialize codec context for the audio
//    state.audio_cdc_ctx = avcodec_alloc_context3(audio_codec);
//    if (checkNull(state.audio_cdc_ctx, "Could not create AVCodecContext for the audio."))
//        return false;

    // Turn video codec parameters to codec context
    if (isError(avcodec_parameters_to_context(state.video_cdc_ctx, video_codec_params)))
    	return false;
    // Turn audio codec parameters to codec context
//    if (isError(avcodec_parameters_to_context(state.audio_cdc_ctx, audio_codec_params)))
//    	return false;

    // Initialize the video codec context to use the codec for the video
    if(isError(avcodec_open2(state.video_cdc_ctx, video_codec, nullptr)))
    	return false;
    // Initialize the audio codec context to use the codec for the audio
//    if(isError(avcodec_open2(state.audio_cdc_ctx, audio_codec, nullptr)))
//    	return false;

    state.video_frame = av_frame_alloc();
    state.av_packet = av_packet_alloc();
//    state.audio_frame = av_frame_alloc();

    if (checkNull(state.video_frame, "Could not allocate the video frame!")
    		|| checkNull(state.av_packet, "Could not allocate the packet!")
//			|| checkNull(state.audio_frame, "Could not allocate the audio frame!")
		)
    	return false;


    *width = state.width;
    *height = state.height;


    data = new uint8_t[state.width * state.height * 4];
    dest[0] = data;
    dest[1] = NULL;
    dest[2] = NULL;
    dest[3] = NULL;
    dest_linesize[0] = state.width * 4;
    dest_linesize[1] = 0;
    dest_linesize[2] = 0;
    dest_linesize[3] = 0;


    return true;
}

int gipFFmpegUtils::read_frame() {

	// Wait until we got a video packet ignoring audio, etc.
	    int response;
	    while (av_read_frame(state.formatcontext, state.av_packet) >= 0) { // Until the EOF
	        if (state.av_packet->stream_index == streamslist[STREAM_VIDEO]) {

	        	//	Send packet to decoder (codec)
				if (isError(avcodec_send_packet(state.video_cdc_ctx, state.av_packet))) {
				    av_packet_unref(state.av_packet);
					return RECEIVED_NONE;
				}

				//	Receive the decoded frame from the codec
				response = avcodec_receive_frame(state.video_cdc_ctx, state.video_frame);
				if(response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
					av_packet_unref(state.av_packet);
					av_frame_unref(state.video_frame);
					continue;
				}
				else if (isError(response)) { // if other error
					av_packet_unref(state.av_packet);
					av_frame_unref(state.video_frame);
					return RECEIVED_NONE;
				}

				state.pixel_format = correct_for_deprecated_pixel_format((AVPixelFormat)state.video_frame->format);

				//	Wipe data back to default values
				av_packet_unref(state.av_packet);
				return RECEIVED_VIDEO;
	        }
//	        else if (state.av_packet->stream_index == streamslist[STREAM_AUDIO]) {
//	        	//	Send packet to decoder (codec)
//				if (isError(avcodec_send_packet(state.audio_cdc_ctx, state.av_packet))) {
//					av_packet_unref(state.av_packet);
//					return RECEIVED_NONE;
//				}
//
//				//	Receive the decoded frame from the codec
//				response = avcodec_receive_frame(state.audio_cdc_ctx, state.audio_frame);
//				if(response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
//					av_packet_unref(state.av_packet);
//					av_frame_unref(state.audio_frame);
//					continue;
//				}
//				else if (isError(response)) { // if other error
//					av_packet_unref(state.av_packet);
//					av_frame_unref(state.audio_frame);
//					return RECEIVED_NONE;
//				}
//
//				state.sample_format = (AVSampleFormat)state.audio_frame->format;
//
//				//	Wipe data back to default values
//				av_packet_unref(state.av_packet);
//				return state.audio_frame->nb_samples;
//	        }
	        else {
	        	//	Packet is from some other stream, igrone
	        	av_packet_unref(state.av_packet);
	        	continue;
	        }
	    }

	    return RECEIVED_NONE;
}

void gipFFmpegUtils::fetch_video_frame(uint8_t **data_out, int64_t* pts) {

	//Commented this out since it is not used
	//*pts = state.video_frame->pts;

	auto corrected_pix_frmt = correct_for_deprecated_pixel_format(state.video_cdc_ctx->pix_fmt);
	state.sws_ctx = sws_getContext(	state.width, state.height, corrected_pix_frmt,
									state.width, state.height, AV_PIX_FMT_RGBA,
									SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

	if (checkNull(state.sws_ctx, "Couldn't initialize SwsContext"))
		return;

    sws_scale(state.sws_ctx, state.video_frame->data, state.video_frame->linesize, 0, state.video_frame->height, dest, dest_linesize);
    sws_freeContext(state.sws_ctx);

    av_frame_unref(state.video_frame);
//    av_frame_unref(state.audio_frame);

    *data_out = data;
}

bool gipFFmpegUtils::seekFrame(int64_t time_stamp_in_secs) {

	av_seek_frame(state.formatcontext, streamslist[STREAM_VIDEO], time_stamp_in_secs, AVSEEK_FLAG_BACKWARD);
	avcodec_flush_buffers(state.video_cdc_ctx);

	return true;
}

void gipFFmpegUtils::close() {
    avformat_close_input(&state.formatcontext);
    avformat_free_context(state.formatcontext);
    av_frame_free(&state.video_frame);
//    av_frame_free(&state.audio_frame);
    av_packet_free(&state.av_packet);
    avcodec_free_context(&state.video_cdc_ctx);
//    avcodec_free_context(&state.audio_cdc_ctx);
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

bool gipFFmpegUtils::isError(int errcode) {
    if (errcode < 0) {
        char buf[256];

        av_strerror(errcode, buf, 256);

        gLoge(std::string(buf));

        return true;
    }

    return false;
}

bool gipFFmpegUtils::checkNull(void *ptr, std::string errtext) {
    if(nullptr == ptr) {
        gLoge("checkNull") << errtext;
        return true;
    }
    return false;
}

gipFFmpegUtils::VideoReaderState* gipFFmpegUtils::getState() {
	return &state;
}
