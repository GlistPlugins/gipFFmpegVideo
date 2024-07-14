/*
 * gipFFmpegUtils.cpp
 *
 *  Created on: 10 Jul 2021
 *      Author: kayra
 *      Edited By: Umutcan T�rkmen 24 Feb 2023
 */

#include "gipFFmpegUtils.h"

#include <cmath>
#include <cstddef>

extern "C" {
	#include <libavutil/pixdesc.h>
}

#include "gVideoFrameRingBuffer.h"

static constexpr int STREAM_VIDEO = 0, STREAM_AUDIO = 1, STREAM_SUBTITLE = 2;

// Atleast SECONDS_FOR_BUFFER seconds of frames are present in buffer.
static constexpr int SECONDS_FOR_BUFFER{4};
static constexpr int MINIMUM_NUM_OF_SECS_TO_PLAY{1};

// Has to be dynamic buffer to accomodate lower/higher fps videos.
static gVideoFrameRingBuffer* g_framesBuffer;
static bool g_needAllocate{true};

bool hasEnoughFramesToDraw()
{
	return g_framesBuffer->size() >= 0;
}

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

			// if(framecount > 0)
			// {
			// 	state->totalframecount = framecount;
			// }
			// else
			// {
			// 	gLogd("gLoadVideoStateFromStorage") << "Meta-data \"nb_frames\" was not found in file. Estimating...";
			// 	int64_t duration = state->formatcontext->duration;
            // 	AVRational frame_rate = state->formatcontext->streams[i]->avg_frame_rate;
            // 	state->totalframecount = (duration * frame_rate.num) / (AV_TIME_BASE * frame_rate.den);
			// 	gLogd("gLoadVideoStateFromStorage") << "Estimated frame rate is: " << state->totalframecount;
			// }

			video_codec_params = codec_params;
			video_codec = avcodec_find_decoder(codec_params->codec_id);

			gLogd("gLoadVideoStateFromStorage") << "Stream " << i << ": codec_id = "
								<< avcodec_get_name(codec_params->codec_id);

			state->height = video_codec_params->height;
			state->width = video_codec_params->width;


			state->framecount = stream->nb_frames;
			state->duration   = stream->duration;
			state->timebase   = stream->time_base;

			int64_t duration       = state->formatcontext->duration;
			AVRational frame_rate  = stream->avg_frame_rate;
			state->avgfps          = frame_rate.num / frame_rate.den;

			// Allocate a bit more than needed.
			g_framesBuffer = new gVideoFrameRingBuffer((SECONDS_FOR_BUFFER + 2) * state->avgfps);

			continue;
		}

		if(state->streamindices[STREAM_AUDIO] == -1 && codec_params->codec_type == AVMEDIA_TYPE_AUDIO) {
			state->streamindices[STREAM_AUDIO] = i;
			gLogd("gLoadVideoStateFromStorage") << "Stream " << i << ": codec_id = "
					<< avcodec_get_name(codec_params->codec_id);
			audio_codec_params = codec_params;
			audio_codec = avcodec_find_decoder(codec_params->codec_id);;
			state->audiochannelsnum = audio_codec_params->ch_layout.nb_channels;
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

	auto correctedpixfmt = gGetCorrectedPixelFormat(state->videocodeccontext->pix_fmt);
	state->numpixelcomponents = av_pix_fmt_count_planes(correctedpixfmt);
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

bool gAdvanceFramesUntilBufferFull(std::shared_ptr<VideoState> l_state) {

	if(g_needAllocate)
	{
		// Should always buffer a bit more than needed.
		if(g_framesBuffer->size() >= SECONDS_FOR_BUFFER * (l_state->avgfps + 2))
		{
			g_needAllocate = false;
			l_state->readytoplay = true;
		}
	}

	if (g_framesBuffer->isFull() || l_state->framesprocessed == l_state->framecount)
	{
		return true;
	}

	AVCodecContext* currentcodeccontext{};
	AVFrame*        currentframe{};
	FrameType       successfulframetype{};

	int avresult = av_read_frame(l_state->formatcontext, l_state->currentpacket);
	if (avresult < 0)
	{
		gLoge("gAdvanceFramesUntilBufferFull") << "Could not read frame: " << av_err2str(avresult);
		return hasEnoughFramesToDraw();
	}

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
		return hasEnoughFramesToDraw();
	}


	// Sometimes the packet may be empty. In that case, we get another packet until
	// we find one that has some information.
	do
	{
		avresult = avcodec_send_packet(currentcodeccontext, l_state->currentpacket);
		if (avresult < 0) {
			gLoge("gAdvanceFramesUntilBufferFull")
				<< "Couldn't receive packet from `avcodec_send_packet`: "
				<< av_err2str(avresult);

			l_state->lastreceivedframetype = FrameType::FRAMETYPE_NONE;

			av_packet_unref(l_state->currentpacket);
			return hasEnoughFramesToDraw();
		}

		//	Receive the decoded frame from the codec
		avresult = avcodec_receive_frame(currentcodeccontext, currentframe);
		if(avresult == AVERROR(EAGAIN)) {
			av_frame_unref(currentframe);
			return false;
		}
		else if(avresult == AVERROR_EOF)
		{
			gLoge("gAdvanceFramesUntilBufferFull")
				<< "End of file reached for current video.";

			l_state->lastreceivedframetype = FrameType::FRAMETYPE_NONE;

			l_state->isfinished = true;

			av_packet_unref(l_state->currentpacket);
			av_frame_unref(currentframe);
			return false;
		}
		else if (avresult < 0) { // if other error
			gLoge("gAdvanceFramesUntilBufferFull")
				<< "Error executing `avcodec_receive_frame`:"
				<< av_err2str(avresult);

			l_state->lastreceivedframetype = FrameType::FRAMETYPE_NONE;

			av_packet_unref(l_state->currentpacket);
			av_frame_unref(currentframe);
			return false;
		}
	} while (avresult == AVERROR(EAGAIN));
	

	//	Wipe data back to default values
	av_packet_unref(l_state->currentpacket);
	l_state->lastreceivedframetype = successfulframetype;

	if(successfulframetype == FrameType::FRAMETYPE_VIDEO)
	{
		gAddFrameToBuffer(l_state);
		l_state->framesprocessed++;
	}

	if (avresult < 0)
	{
		gLoge("gAdvanceFramesUntilBufferFull") << "Error when reading frame: " << av_err2str(avresult);
	}

	return !g_needAllocate;
}

void gAddFrameToBuffer(std::shared_ptr<VideoState> l_state)
{
    uint8_t* destination[4];
    int32_t  destinationlinesize[4];

	auto data = new uint8_t[l_state->width * l_state->height * 4];

	// Stuff for sws context
    destination[0] = data;
    destination[1] = nullptr;
    destination[2] = nullptr;
    destination[3] = nullptr;

	// The layout of our pixel data. [RGBA, RGBA, RGBA... ]
    destinationlinesize[0] = l_state->width * 4;
    destinationlinesize[1] = 0;
    destinationlinesize[2] = 0;
    destinationlinesize[3] = 0;

	auto height = sws_scale(
		l_state->swscontext,
		l_state->videoframe->data, l_state->videoframe->linesize,
		0, l_state->videoframe->height,
		destination, destinationlinesize
	);

	if(height != l_state->videoframe->height)
	{
		gLogd("gAddFrameToBuffer") << "Sws scaling unsucessful.";
	}

	g_framesBuffer->push(data);

	destination[0] = nullptr;

    av_frame_unref(l_state->videoframe);
    av_frame_unref(l_state->audioframe);
}

void gFetchVideoFrameToState(std::shared_ptr<VideoState> l_state) {
	g_framesBuffer->pop(l_state->videoframepixeldata);
}

bool gSeekToFrame(std::shared_ptr<VideoState> l_state, float l_timeStampInSec) {
	
	auto fmtcontext = l_state->formatcontext;
	auto videostream = l_state->formatcontext->streams[l_state->streamindices[STREAM_VIDEO]];
	auto audiostream = l_state->formatcontext->streams[l_state->streamindices[STREAM_AUDIO]];
	
	int64_t timestamptimeshundred = static_cast<int64_t>(l_timeStampInSec * 100);

	int64_t seekTarget = av_rescale(timestamptimeshundred, videostream->time_base.den, videostream->time_base.num) / 100;
	
	//int avresult = avformat_seek_file(fmtcontext, l_state->streamindices[STREAM_VIDEO], seekTarget - videostream->time_base.den, seekTarget, seekTarget, 0);
	
	int avresult = av_seek_frame(fmtcontext, l_state->streamindices[STREAM_VIDEO], seekTarget, 0);
	if(avresult < 0)
	{
		gLoge("gSeekToFrame") << "Error when seeking to time: " << l_timeStampInSec << " " << av_err2str(avresult);
	}

	avcodec_flush_buffers(l_state->videocodeccontext);
	if(avresult < 0)
	{
		gLoge("gSeekToFrame") << "Error when flushing codec context buffers " << av_err2str(avresult);
	}
	g_framesBuffer->popAll();

	av_frame_unref(l_state->videoframe);
	av_frame_unref(l_state->audioframe);
	av_packet_unref(l_state->currentpacket);

	l_state->framesprocessed = 0;
	l_state->readytoplay = false;
	g_needAllocate = true;

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

void gClearLastFrame(std::shared_ptr<VideoState> l_state)
{
	l_state->videoframepixeldata.reset();
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
