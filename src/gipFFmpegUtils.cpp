/*
 * gipFFmpegUtils.cpp
 *
 *  Created on: 10 Jul 2021
 *      Author: kayra
 *      Edited By: Umutcan T�rkmen 24 Feb 2023
 */

#include "gipFFmpegUtils.h"

#include <cmath>

static const int STREAM_VIDEO = 0, STREAM_AUDIO = 1, STREAM_SUBTITLE = 2;

std::shared_ptr<VideoState> gLoadVideoStateFromStorage(std::string const& t_filename) {
	auto state = std::make_shared<VideoState>();

	state->streamindices[STREAM_VIDEO] = -1;
	state->streamindices[STREAM_AUDIO] = -1;
	state->streamindices[STREAM_SUBTITLE] = -1;

	state->formatcontext = avformat_alloc_context();
    if (nullptr == state->formatcontext)
	{
		gLoge("gLoadVideoStateFromStorage") << "Couldn't create create format context.";
		avformat_free_context(state->formatcontext);

		state->iscreated = false;
		return state;
	}

	int averror = avformat_open_input(&state->formatcontext, t_filename.c_str(), nullptr, nullptr);
    if(averror < 0)
	{
		gLoge("gLoadVideoStateFromStorage") << "Error executing `avformat_open_input`: " << av_err2str(averror);

		avformat_close_input(&state->formatcontext);

		state->iscreated = false;
		return state;
	}

	averror = avformat_find_stream_info(state->formatcontext, nullptr);
	if (averror < 0) {
        gLoge("gLoadVideoStateFromStorage") << "Error executing `avformat_find_stream_info`: " << av_err2str(averror);

		avformat_close_input(&state->formatcontext);

		state->iscreated = false;
		return state;
    }

	//	Find the needed streams
	AVCodecParameters *video_codec_params;
	AVCodecParameters *audio_codec_params;
	const AVCodec *video_codec;
	const AVCodec *audio_codec;

	for (int i = 0; i < state->formatcontext->nb_streams; i++) {
		auto* stream = state->formatcontext->streams[i];

		auto* codec_params = stream->codecpar;

		if (nullptr == codec_params) {
            gLoge("gLoadVideoStateFromStorage") << "Stream " << i << " has null codec parameters.";
            continue;
        }


		if (state->streamindices[STREAM_VIDEO] == -1 && codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
			state->streamindices[STREAM_VIDEO] = i;

			int64_t framecount = state->formatcontext->streams[i]->nb_frames;
			if(framecount > 0)
			{
				state->totalframecount = framecount;
			}
			else
			{
				gLogd("gLoadVideoStateFromStorage") << "Meta-data \"nb_frames\" was not found in file. Estimating...";
				int64_t duration = state->formatcontext->duration;
            	AVRational frame_rate = state->formatcontext->streams[i]->avg_frame_rate;
            	state->totalframecount = (duration * frame_rate.num) / (AV_TIME_BASE * frame_rate.den);
				gLogd("gLoadVideoStateFromStorage") << "Estimated frame rate is: " << state->totalframecount;
			}

			video_codec_params = codec_params;
			video_codec = avcodec_find_decoder(codec_params->codec_id);

			gLogd("gLoadVideoStateFromStorage") << "Stream " << i << ": codec_id = "
								<< avcodec_get_name(codec_params->codec_id);

			state->height = video_codec_params->height;
			state->width = video_codec_params->width;

			state->framecount = stream->nb_frames;
			state->duration   = stream->duration;
			state->timebase   = stream->time_base;
			continue;
		}
		if(state->streamindices[STREAM_AUDIO] == -1 && codec_params->codec_type == AVMEDIA_TYPE_AUDIO) {
			state->streamindices[STREAM_AUDIO] = i;
			gLogd("gLoadVideoStateFromStorage") << "Stream " << i << ": codec_id = "
					<< avcodec_get_name(codec_params->codec_id);
			audio_codec_params = codec_params;
			audio_codec = avcodec_find_decoder(codec_params->codec_id);;
			state->audiochannelsnum = audio_codec_params->channels;
			state->audiosamplerate = audio_codec_params->sample_rate;
			continue;
		}

	}

	if (state->streamindices[STREAM_VIDEO] == -1) {
		gLoge("gLoadVideoStateFromStorage") << "Valid VIDEO stream could not be created from the file.";

		state->iscreated = false;
		return state;
	}

	if (state->streamindices[STREAM_AUDIO] == -1) {
		// Audio is optional, so log should be a warning
		gLogw("gLoadVideoStateFromStorage") << "Valid AUDIO stream could not be created from the file.";

		state->iscreated = false;
		return state;
	}

	if(state->audiochannelsnum != 2) {
 		gLogd("gLoadVideoStateFromStorage") << "Only stereo audio found in stream.";
	}

	// Initialize codec context for the video
    state->videocodeccontext = avcodec_alloc_context3(video_codec);
    if (nullptr == state->videocodeccontext)
	{
		gLoge("gLoadVideoStateFromStorage")
			<< "Could not create AVCodecContext for the found VIDEO codec :"
			<< avcodec_get_name(video_codec->id) ;
		state->iscreated = false;
		return state;
	}

    // Initialize codec context for the audio
    state->audiocodeccontext = avcodec_alloc_context3(audio_codec);
    if (nullptr == state->audiocodeccontext)
	{
		gLoge("gLoadVideoStateFromStorage")
			<< "Could not create AVCodecContext for the audio.";
		state->iscreated = false;
		return state;
	}

    // Turn video codec parameters to codec context
	averror = avcodec_parameters_to_context(state->videocodeccontext, video_codec_params);
    if (averror < 0)
	{
		gLoge("gLoadVideoStateFromStorage")
			<< "Error when executing: avcodec_parameters_to_context"
			<< av_err2str(averror);
		state->iscreated = false;
		return state;
	}
    // Turn audio codec parameters to codec context
	averror = avcodec_parameters_to_context(state->audiocodeccontext, audio_codec_params);
   	if (averror < 0)
	{
		gLoge("gLoadVideoStateFromStorage")
			<< "Error executing `avcodec_parameters_to_context`: "
			<< av_err2str(averror);
		state->iscreated = false;
		return state;
	}


    // Initialize the video codec context to use the codec for the video
	averror = avcodec_open2(state->videocodeccontext, video_codec, nullptr);
	if (averror < 0)
	{
		gLoge("gLoadVideoStateFromStorage")
			<< "Error executing `avcodec_open2`: "
			<< av_err2str(averror);
		state->iscreated = false;
		return state;
	}

    // Initialize the audio codec context to use the codec for the audio
	averror = avcodec_open2(state->audiocodeccontext, audio_codec, nullptr);
	if (averror < 0)
	{
		gLoge("gLoadVideoStateFromStorage")
			<< "Error executing `avcodec_open2`: "
			<< av_err2str(averror);
		state->iscreated = false;
		return state;
	}

    state->videoframe = av_frame_alloc();
	if (nullptr == state->videoframe)
	{
		gLoge("gLoadVideoStateFromStorage") << "Could not allocate the AUDIO frame!";
		state->iscreated = false;
		return state;
	}

    state->audioframe = av_frame_alloc();
	if (nullptr == state->audioframe)
	{
		gLoge("gLoadVideoStateFromStorage") << "Could not allocate the AUDIO frame!";
		state->iscreated = false;
		return state;
	}

	state->currentpacket = av_packet_alloc();
	if (nullptr == state->currentpacket)
	{
		gLoge("gLoadVideoStateFromStorage") << "Could not allocate AVPacket!";
		state->iscreated = false;
		return state;
	}

	gAllocateStorageForVideoFrame(state);

	auto correctedpixfmt = gGetCorrectedPixelFormat(state->videocodeccontext->pix_fmt);
	state->swscontext = sws_getContext(
			state->width, state->height, correctedpixfmt,
			state->width, state->height, AV_PIX_FMT_RGBA,
			SWS_FAST_BILINEAR, nullptr, nullptr, nullptr
	);

	if (nullptr == state->swscontext)
	{
		gLoge("gLoadVideoStateFromStorage") << "Couldn't initialize SwsContext";
		state->iscreated = false;
		return state;
	}

	state->iscreated = true;

    return state;
}

void gAdvanceFrameInPacket(std::shared_ptr<VideoState> l_state) {

	// Wait until we got a video packet ignoring audio, etc.
	int avresult{};
	AVCodecContext* currentcodeccontext{};
	AVFrame*        currentframe{};
	FrameType       successfulframetype{};
	do {
		if (l_state->currentpacket->stream_index == l_state->streamindices[STREAM_VIDEO]) {
			currentcodeccontext = l_state->videocodeccontext;
			currentframe 		= l_state->videoframe;
			successfulframetype = FrameType::FRAMETYPE_VIDEO;
		}
		else if (l_state->currentpacket->stream_index == l_state->streamindices[STREAM_AUDIO]) {
			currentcodeccontext = l_state->audiocodeccontext;
			currentframe	 	= l_state->audioframe;
			successfulframetype = FrameType::FRAMETYPE_AUDIO;
		}
		else {
			av_packet_unref(l_state->currentpacket);
			continue;
		}

		avresult = avcodec_send_packet(currentcodeccontext, l_state->currentpacket);
		if (avresult < 0) {
			gLoge("gAdvanceFrameInPacket")
				<< "Couldn't receive packet from `avcodec_send_packet`: "
				<< av_err2str(avresult);

			l_state->lastreceivedframetype = FrameType::FRAMETYPE_NONE;

			av_packet_unref(l_state->currentpacket);
			return;
		}

		//	Receive the decoded frame from the codec
		avresult = avcodec_receive_frame(currentcodeccontext, currentframe);
		if(avresult == AVERROR(EAGAIN) || avresult == AVERROR_EOF) {
			gLoge("gAdvanceFrameInPacket")
				<< "End of file reached for current video.";

			l_state->lastreceivedframetype = FrameType::FRAMETYPE_NONE;

			av_packet_unref(l_state->currentpacket);
			av_frame_unref(currentframe);
			continue;
		}
		else if (avresult < 0) { // if other error
			gLoge("gAdvanceFrameInPacket")
				<< "Error executing `avcodec_receive_frame`:"
				<< av_err2str(avresult);

			l_state->lastreceivedframetype = FrameType::FRAMETYPE_NONE;

			av_packet_unref(l_state->currentpacket);
			av_frame_unref(currentframe);
			return;
		}

		//	Wipe data back to default values
		av_packet_unref(l_state->currentpacket);
		l_state->lastreceivedframetype = successfulframetype;
		return;

		avresult = av_read_frame(l_state->formatcontext, l_state->currentpacket);
	}
	while (avresult >= 0);

	if (avresult < 0)
	{
		gLoge("gAdvanceFrameInPacket") << "Error when reading frame: " << av_err2str(avresult);
	}
}

void gFetchVideoFrameToState(std::shared_ptr<VideoState> l_state) {

    sws_scale(
		l_state->swscontext,
		l_state->videoframe->data, l_state->videoframe->linesize,
		0, l_state->videoframe->height,
		l_state->destination.data(), l_state->destinationlinesize.data()
	);

    av_frame_unref(l_state->videoframe);
    av_frame_unref(l_state->audioframe);
}

bool gSeekToFrame(std::shared_ptr<VideoState> l_state, int64_t l_timeStampInSec) {

	av_seek_frame(l_state->formatcontext, l_state->streamindices[STREAM_VIDEO], l_timeStampInSec, AVSEEK_FLAG_BACKWARD);
	avcodec_flush_buffers(l_state->videocodeccontext);

	return true;
}

void gClearVideoState(std::shared_ptr<VideoState> l_state) {
    avformat_close_input(&l_state->formatcontext);
    avformat_free_context(l_state->formatcontext);
    av_frame_free(&l_state->videoframe);
    av_frame_free(&l_state->audioframe);
    av_packet_free(&l_state->currentpacket);
    avcodec_free_context(&l_state->videocodeccontext);
    avcodec_free_context(&l_state->audiocodeccontext);
	sws_freeContext(l_state->swscontext);
}

AVPixelFormat gGetCorrectedPixelFormat(AVPixelFormat l_pixelFormat) {
    // Fix swscaler deprecated pixel format warning
    // (YUVJ has been deprecated, change pixel format to regular YUV)
    switch (l_pixelFormat) {
        case AV_PIX_FMT_YUVJ420P: return AV_PIX_FMT_YUV420P;
        case AV_PIX_FMT_YUVJ422P: return AV_PIX_FMT_YUV422P;
        case AV_PIX_FMT_YUVJ444P: return AV_PIX_FMT_YUV444P;
        case AV_PIX_FMT_YUVJ440P: return AV_PIX_FMT_YUV440P;
        default:                  return l_pixelFormat;
    }
}

void gAllocateStorageForVideoFrame(std::shared_ptr<VideoState> l_state)
{
	// Allocate enough memory for the video's size * rgba
    l_state->videoframepixeldata = new uint8_t[l_state->width * l_state->height * 4];

	// Stuff for sws context
    l_state->destination[0] = l_state->videoframepixeldata;
    l_state->destination[1] = nullptr;
    l_state->destination[2] = nullptr;
    l_state->destination[3] = nullptr;

	// The layout of our pixel data. [RGBA, RGBA, RGBA... ]
    l_state->destinationlinesize[0] = l_state->width * 4;
    l_state->destinationlinesize[1] = 0;
    l_state->destinationlinesize[2] = 0;
    l_state->destinationlinesize[3] = 0;
}
