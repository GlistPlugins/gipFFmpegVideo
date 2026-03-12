/*
 * gipFFmpegUtils.cpp
 *
 *  Created on: 10 Jul 2021
 *      Author: kayra
 *      Edited By: Umutcan Turkmen 24 Feb 2023
 */

#include "gipFFmpegUtils.h"

#include <cmath>
#include <cstddef>
#include <vector>

extern "C" {
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

#include "gAudioSampleRingBuffer.h"
#include "gVideoFrameRingBuffer.h"

static constexpr int STREAM_VIDEO = 0, STREAM_AUDIO = 1, STREAM_SUBTITLE = 2;

// Seconds of frames to keep in ring buffer (capacity) and to pre-buffer before playback starts.
static constexpr int SECONDS_FOR_BUFFER{4};
static constexpr int SECONDS_BEFORE_PLAY{1};

std::shared_ptr<VideoState> VideoState::loadFromStorage(std::string const& t_filename) {
	auto state = std::make_shared<VideoState>();

	state->streamindices[STREAM_VIDEO] = -1;
	state->streamindices[STREAM_AUDIO] = -1;
	state->streamindices[STREAM_SUBTITLE] = -1;

	state->formatcontext = avformat_alloc_context();
	if(!state->formatcontext) {
		gLoge("VideoState") << "Couldn't create format context.";
		avformat_free_context(state->formatcontext);

		state->iscreated = false;
		return state;
	}

	int averror = avformat_open_input(&state->formatcontext, t_filename.c_str(), nullptr, nullptr);
	if(averror < 0) {
		gLoge("VideoState") << "Error executing `avformat_open_input`: " << av_err2str(averror);

		avformat_close_input(&state->formatcontext);

		state->iscreated = false;
		return state;
	}

	// Limit probe duration for large files to avoid long startup times
	state->formatcontext->max_analyze_duration = 5 * AV_TIME_BASE; // 5 seconds
	state->formatcontext->probesize = 5 * 1024 * 1024; // 5 MB

	averror = avformat_find_stream_info(state->formatcontext, nullptr);
	if(averror < 0) {
		gLoge("VideoState") << "Error executing `avformat_find_stream_info`: " << av_err2str(averror);

		avformat_close_input(&state->formatcontext);

		state->iscreated = false;
		return state;
	}

	//	Find the needed streams
	AVCodecParameters* video_codec_params;
	AVCodecParameters* audio_codec_params;
	const AVCodec* video_codec;
	const AVCodec* audio_codec;

	for(int i = 0; i < state->formatcontext->nb_streams; i++) {
		auto* stream = state->formatcontext->streams[i];

		auto* codec_params = stream->codecpar;

		if(nullptr == codec_params) {
			gLoge("VideoState") << "Stream " << i << " has null codec parameters.";
			continue;
		}

		if(state->streamindices[STREAM_VIDEO] == -1 && codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
			state->streamindices[STREAM_VIDEO] = i;

			video_codec_params = codec_params;
			video_codec = avcodec_find_decoder(codec_params->codec_id);

			gLogd("VideoState") << "Stream " << i << ": codec_id = "
												<< avcodec_get_name(codec_params->codec_id);

			state->height = video_codec_params->height;
			state->width = video_codec_params->width;


			state->framecount = stream->nb_frames;
			state->duration = stream->duration;
			state->timebase = stream->time_base;

			int64_t duration = state->formatcontext->duration;
			AVRational frame_rate = stream->avg_frame_rate;
			if(frame_rate.den > 0 && frame_rate.num > 0) {
				state->avgfps = static_cast<double>(frame_rate.num) / frame_rate.den;
			} else {
				state->avgfps = 25.0; // fallback for containers without frame rate info
			}

			// Allocate a bit more than needed.
			state->framesbuffer = new gVideoFrameRingBuffer(static_cast<size_t>((SECONDS_FOR_BUFFER + 2) * state->avgfps));

			continue;
		}

		if(state->streamindices[STREAM_AUDIO] == -1 && codec_params->codec_type == AVMEDIA_TYPE_AUDIO) {
			state->streamindices[STREAM_AUDIO] = i;
			gLogd("VideoState") << "Stream " << i << ": codec_id = "
												<< avcodec_get_name(codec_params->codec_id);
			audio_codec_params = codec_params;
			audio_codec = avcodec_find_decoder(codec_params->codec_id);
			state->audiochannelsnum = audio_codec_params->ch_layout.nb_channels;
			state->audiosamplerate = audio_codec_params->sample_rate;
			state->audiotimebase = stream->time_base;
			continue;
		}
	}

	if(state->streamindices[STREAM_VIDEO] == -1) {
		gLoge("VideoState") << "Valid VIDEO stream could not be created from the file.";

		state->iscreated = false;
		return state;
	}

	if(state->streamindices[STREAM_AUDIO] == -1) {
		gLogw("VideoState") << "No audio stream found. Video will play without audio.";
	} else {
		state->hasaudio = true;
	}

	// Initialize codec context for the video
	state->videocodeccontext = avcodec_alloc_context3(video_codec);
	if(!state->videocodeccontext) {
		gLoge("VideoState") << "Could not create AVCodecContext for the found VIDEO codec :"
				<< avcodec_get_name(video_codec->id);
		state->iscreated = false;
		return state;
	}

	// Initialize codec context for the audio
	if(state->hasaudio) {
		state->audiocodeccontext = avcodec_alloc_context3(audio_codec);
		if(!state->audiocodeccontext) {
			gLoge("VideoState") << "Could not create AVCodecContext for the audio.";
			state->hasaudio = false;
		}
	}

	// Turn video codec parameters to codec context
	averror = avcodec_parameters_to_context(state->videocodeccontext, video_codec_params);
	if(averror < 0) {
		gLoge("VideoState") << "Error when executing: avcodec_parameters_to_context"
				<< av_err2str(averror);
		state->iscreated = false;
		return state;
	}
	// Turn audio codec parameters to codec context
	if(state->hasaudio) {
		averror = avcodec_parameters_to_context(state->audiocodeccontext, audio_codec_params);
		if(averror < 0) {
			gLoge("VideoState") << "Error executing `avcodec_parameters_to_context` for audio: " << av_err2str(averror);
			state->hasaudio = false;
		}
	}

	// Enable multi-threaded decoding for video
	state->videocodeccontext->thread_count = 0; // 0 = auto-detect optimal thread count
	state->videocodeccontext->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

	// Initialize the video codec context to use the codec for the video
	averror = avcodec_open2(state->videocodeccontext, video_codec, nullptr);
	if(averror < 0) {
		gLoge("VideoState") << "Error executing `avcodec_open2`: "
				<< av_err2str(averror);
		state->iscreated = false;
		return state;
	}

	// Detect interlaced content and correct avgfps from field rate to frame rate
	// (e.g. MPEG-2 interlaced: avg_frame_rate reports 50 fields/s, actual is 25 frames/s)
	{
		auto fo = state->videocodeccontext->field_order;
		bool interlaced = (fo == AV_FIELD_TT || fo == AV_FIELD_BB ||
						   fo == AV_FIELD_TB || fo == AV_FIELD_BT);
		if(!interlaced && state->videocodeccontext->ticks_per_frame == 2) {
			interlaced = true;
		}
		if(interlaced && state->avgfps > 1.0) {
			gLogd("VideoState") << "Interlaced content detected, correcting avgfps from " << state->avgfps << " to " << (state->avgfps / 2.0);
			state->avgfps /= 2.0;
		}
	}

	// Initialize the audio codec context to use the codec for the audio
	if(state->hasaudio) {
		averror = avcodec_open2(state->audiocodeccontext, audio_codec, nullptr);
		if(averror < 0) {
			gLoge("VideoState") << "Error executing `avcodec_open2` for audio: " << av_err2str(averror);
			state->hasaudio = false;
		}
	}

	state->videoframe = av_frame_alloc();
	if(!state->videoframe) {
		gLoge("VideoState") << "Could not allocate the video frame!";
		state->iscreated = false;
		return state;
	}

	if(state->hasaudio) {
		state->audioframe = av_frame_alloc();
		if(!state->audioframe) {
			gLoge("VideoState") << "Could not allocate the audio frame!";
			state->hasaudio = false;
		}
	}

	state->currentpacket = av_packet_alloc();
	if(!state->currentpacket) {
		gLoge("VideoState") << "Could not allocate AVPacket!";
		state->iscreated = false;
		return state;
	}

	auto correctedpixfmt = gGetCorrectedPixelFormat(state->videocodeccontext->pix_fmt);
	state->numpixelcomponents = av_pix_fmt_count_planes(correctedpixfmt);
	state->swscontext = sws_getContext(
			state->width, state->height, correctedpixfmt,
			state->width, state->height, AV_PIX_FMT_RGBA,
			SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
	state->decodewidth = state->width;
	state->decodeheight = state->height;

	if(!state->swscontext) {
		gLoge("VideoState") << "Couldn't initialize SwsContext";
		state->iscreated = false;
		return state;
	}

	// Initialize audio resampler (convert to interleaved float32 stereo)
	if(state->hasaudio) {
		state->swrcontext = swr_alloc();
		if(state->swrcontext) {
			AVChannelLayout outlayout = AV_CHANNEL_LAYOUT_STEREO;
			int swrerr = swr_alloc_set_opts2(
					&state->swrcontext,
					&outlayout, AV_SAMPLE_FMT_FLT, state->audiosamplerate,
					&state->audiocodeccontext->ch_layout, state->audiocodeccontext->sample_fmt, state->audiosamplerate,
					0, nullptr);
			if(swrerr < 0 || swr_init(state->swrcontext) < 0) {
				gLoge("VideoState") << "Could not initialize SwrContext for audio resampling.";
				swr_free(&state->swrcontext);
				state->hasaudio = false;
			} else {
				// Buffer for ~SECONDS_FOR_BUFFER seconds of stereo float audio
				size_t audiobufsize = SECONDS_FOR_BUFFER * state->audiosamplerate * 2 * 2;
				state->audiobuffer = new gAudioSampleRingBuffer(audiobufsize);
				gLogd("VideoState") << "Audio initialized: " << state->audiosamplerate << " Hz, " << state->audiochannelsnum << " ch -> stereo float32";
			}
		} else {
			gLoge("VideoState") << "Could not allocate SwrContext.";
			state->hasaudio = false;
		}
	}

	state->iscreated = true;

	return state;
}

bool VideoState::advanceFramesUntilBufferFull() {
	if(needallocate) {
		if(preloaded) {
			// Preloaded mode: ready when all frames decoded or buffer is full
			if(isfinished || framesbuffer->isFull()) {
				needallocate = false;
				readytoplay = true;
			}
		} else {
			// Ready after bufferthreshold frames are buffered (or EOF)
			if(static_cast<int>(framesbuffer->size()) >= bufferthreshold || isfinished) {
				needallocate = false;
				readytoplay = true;
			}
		}
	}

	if(framesbuffer->isFull() || (framecount > 0 && framesprocessed == framecount)) {
		return true;
	}

	AVCodecContext* currentcodeccontext{};
	AVFrame* currentframe{};
	FrameType successfulframetype{};

	int avresult = av_read_frame(formatcontext, currentpacket);
	if(avresult < 0) {
		if(avresult == AVERROR_EOF) {
			isfinished = true;
			if(needallocate) {
				needallocate = false;
				readytoplay = true;
			}
		} else {
			gLoge("VideoState") << "advanceFramesUntilBufferFull: Could not read frame: " << av_err2str(avresult);
		}
		return true;
	}

	if(currentpacket->stream_index == streamindices[STREAM_VIDEO]) {
		currentcodeccontext = videocodeccontext;
		currentframe = videoframe;
		successfulframetype = FrameType::FRAMETYPE_VIDEO;
	} else if(hasaudio && currentpacket->stream_index == streamindices[STREAM_AUDIO]) {
		currentcodeccontext = audiocodeccontext;
		currentframe = audioframe;
		successfulframetype = FrameType::FRAMETYPE_AUDIO;
	} else {
		av_packet_unref(currentpacket);
		return false;// discarded packet, keep reading
	}


	// Sometimes the packet may be empty. In that case, we get another packet until
	// we find one that has some information.
	do {
		avresult = avcodec_send_packet(currentcodeccontext, currentpacket);
		if(avresult < 0) {
			gLoge("VideoState") << "advanceFramesUntilBufferFull: Couldn't receive packet from `avcodec_send_packet`: "
					<< av_err2str(avresult);

			lastreceivedframetype = FrameType::FRAMETYPE_NONE;

			av_packet_unref(currentpacket);
			return false;
		}

		//	Receive the decoded frame from the codec
		avresult = avcodec_receive_frame(currentcodeccontext, currentframe);
		if(avresult == AVERROR(EAGAIN)) {
			av_frame_unref(currentframe);
			return false;
		} else if(avresult == AVERROR_EOF) {
			lastreceivedframetype = FrameType::FRAMETYPE_NONE;
			isfinished = true;

			av_packet_unref(currentpacket);
			av_frame_unref(currentframe);
			return true;		  // end of file, stop reading
		} else if(avresult < 0) {// if other error
			gLoge("VideoState") << "advanceFramesUntilBufferFull: Error executing `avcodec_receive_frame`:"
					<< av_err2str(avresult);

			lastreceivedframetype = FrameType::FRAMETYPE_NONE;

			av_packet_unref(currentpacket);
			av_frame_unref(currentframe);
			return false;
		}
	} while (avresult == AVERROR(EAGAIN));


	//	Wipe data back to default values
	av_packet_unref(currentpacket);
	lastreceivedframetype = successfulframetype;

	if(successfulframetype == FrameType::FRAMETYPE_VIDEO) {
		addFrameToBuffer();
		framesprocessed++;
	} else if(successfulframetype == FrameType::FRAMETYPE_AUDIO) {
		addAudioToBuffer();
	}

	// Return true only when buffer is genuinely full
	return framesbuffer->isFull() || (framecount > 0 && framesprocessed == framecount);
}

void VideoState::addFrameToBuffer() {
	// Detect dimension change mid-stream and recreate SwsContext for this frame
	int32_t framewidth = videoframe->width;
	int32_t frameheight = videoframe->height;
	if(framewidth != decodewidth || frameheight != decodeheight) {
		gLogd("VideoState") << "Dimension change: " << decodewidth << "x" << decodeheight
								   << " -> " << framewidth << "x" << frameheight;
		decodewidth = framewidth;
		decodeheight = frameheight;

		sws_freeContext(swscontext);
		auto correctedpixfmt = gGetCorrectedPixelFormat(videocodeccontext->pix_fmt);
		swscontext = sws_getContext(
				framewidth, frameheight, correctedpixfmt,
				framewidth, frameheight, AV_PIX_FMT_RGBA,
				SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
	}

	uint8_t* destination[4];
	int32_t destinationlinesize[4];

	auto data = new uint8_t[framewidth * frameheight * 4];

	// Stuff for sws context
	destination[0] = data;
	destination[1] = nullptr;
	destination[2] = nullptr;
	destination[3] = nullptr;

	// The layout of our pixel data. [RGBA, RGBA, RGBA... ]
	destinationlinesize[0] = framewidth * 4;
	destinationlinesize[1] = 0;
	destinationlinesize[2] = 0;
	destinationlinesize[3] = 0;

	auto height = sws_scale(
			swscontext,
			videoframe->data, videoframe->linesize,
			0, videoframe->height,
			destination, destinationlinesize);

	if(height != videoframe->height) {
		gLogd("VideoState") << "Sws scaling unsucessful.";
	}

	framesbuffer->push(data);
	framedimensions.push_back({framewidth, frameheight});

	// Record PTS for A/V sync
	double pts = 0.0;
	if(videoframe->pts != AV_NOPTS_VALUE) {
		pts = videoframe->pts * av_q2d(timebase);
	} else {
		pts = framesprocessed / static_cast<double>(avgfps);
	}
	framepts.push_back(pts);

	destination[0] = nullptr;

	av_frame_unref(videoframe);
}

void VideoState::addAudioToBuffer() {
	if(!swrcontext || !audiobuffer) {
		av_frame_unref(audioframe);
		return;
	}

	// Set audio clock sync point on first audio frame
	if(!audioclockvalid.load(std::memory_order_relaxed)) {
		double pts = 0.0;
		if(audioframe->pts != AV_NOPTS_VALUE) {
			pts = audioframe->pts * av_q2d(audiotimebase);
		}
		audioclocksyncpts.store(pts, std::memory_order_release);
		audioclockvalid.store(true, std::memory_order_release);
	}

	int outsamples = swr_get_out_samples(swrcontext, audioframe->nb_samples);
	if(outsamples <= 0) {
		av_frame_unref(audioframe);
		return;
	}

	int channels = 2;// stereo output
	std::vector<float> outbuf(outsamples * channels);
	uint8_t* outptr = reinterpret_cast<uint8_t*>(outbuf.data());

	int convertedsamples = swr_convert(
			swrcontext,
			&outptr, outsamples,
			(const uint8_t**) audioframe->data, audioframe->nb_samples);

	if(convertedsamples > 0) {
		audiobuffer->write(outbuf.data(), convertedsamples * channels);
	}

	av_frame_unref(audioframe);
}

void VideoState::fetchVideoFrame() {
	framesbuffer->pop(videoframepixeldata);
	if(!framepts.empty()) {
		currentvideopts = framepts.front();
		framepts.pop_front();
	}
	if(!framedimensions.empty()) {
		auto [w, h] = framedimensions.front();
		framedimensions.pop_front();
		if(w != width || h != height) {
			width = w;
			height = h;
			dimensionchanged = true;
		}
	}
}

bool VideoState::seekToFrame(float l_timeStampInSec) {

	auto fmtcontext = formatcontext;
	auto videostream = formatcontext->streams[streamindices[STREAM_VIDEO]];

	int64_t timestamptimeshundred = static_cast<int64_t>(l_timeStampInSec * 100);

	int64_t seekTarget = av_rescale(timestamptimeshundred, videostream->time_base.den, videostream->time_base.num) / 100;

	int avresult = av_seek_frame(fmtcontext, streamindices[STREAM_VIDEO], seekTarget, 0);
	if(avresult < 0) {
		gLoge("VideoState") << "Error when seeking to time: " << l_timeStampInSec << " " << av_err2str(avresult);
	}

	avcodec_flush_buffers(videocodeccontext);
	if(avresult < 0) {
		gLoge("VideoState") << "Error when flushing codec context buffers " << av_err2str(avresult);
	}
	if(hasaudio) {
		avcodec_flush_buffers(audiocodeccontext);
		if(audiobuffer) {
			audiobuffer->clear();
		}
		audioclockvalid.store(false, std::memory_order_release);
		audioclocksyncpts.store(0.0, std::memory_order_release);
	}
	framesbuffer->popAll();
	framepts.clear();
	framedimensions.clear();

	av_frame_unref(videoframe);
	av_frame_unref(audioframe);
	av_packet_unref(currentpacket);

	framesprocessed = 0;
	readytoplay = false;
	needallocate = true;

	return true;
}

void VideoState::clear() {
	avformat_close_input(&formatcontext);
	avformat_free_context(formatcontext);
	av_frame_free(&videoframe);
	av_frame_free(&audioframe);
	av_packet_free(&currentpacket);
	avcodec_free_context(&videocodeccontext);
	avcodec_free_context(&audiocodeccontext);
	sws_freeContext(swscontext);
	if(swrcontext) {
		swr_free(&swrcontext);
	}
	delete audiobuffer;
	audiobuffer = nullptr;
	delete framesbuffer;
	framesbuffer = nullptr;
	framepts.clear();
	framedimensions.clear();
}

void VideoState::clearLastFrame() {
	videoframepixeldata.reset();
}

double VideoState::getAudioClock() {
	if(!hasaudio || !audiobuffer || !audioclockvalid.load(std::memory_order_acquire)) {
		return -1.0;
	}
	double syncpts = audioclocksyncpts.load(std::memory_order_acquire);
	size_t samplesread = audiobuffer->getTotalRead();
	// 2 channels (stereo), so divide by 2 to get frames
	double secondsread = static_cast<double>(samplesread) / (2.0 * audiosamplerate);
	return syncpts + secondsread;
}

double VideoState::peekNextVideoFramePts() {
	if(framepts.empty()) return -1.0;
	return framepts.front();
}

void VideoState::setPreloaded(size_t maxmemorybytes) {
	preloaded = true;

	// Estimate total frames for buffer capacity
	int64_t estimatedframes;
	if(framecount > 0) {
		estimatedframes = framecount;
	} else if(formatcontext->duration > 0 && avgfps > 0) {
		double durationsec = static_cast<double>(formatcontext->duration) / AV_TIME_BASE;
		estimatedframes = static_cast<int64_t>(durationsec * avgfps);
	} else {
		// Fallback: assume 60 seconds at 30fps
		estimatedframes = 1800;
	}
	estimatedframes = static_cast<int64_t>(estimatedframes * 1.1) + 10;
	if(estimatedframes < 10) estimatedframes = 10;

	// Cap buffer based on memory limit to prevent OOM
	// Each decoded frame = width * height * 4 bytes (RGBA)
	size_t framebytes = static_cast<size_t>(width) * height * 4;
	size_t totalmemory = static_cast<size_t>(estimatedframes) * framebytes;

	if(framebytes > 0 && totalmemory > maxmemorybytes) {
		int64_t maxframes = static_cast<int64_t>(maxmemorybytes / framebytes);
		if(maxframes < 10) maxframes = 10;
		gLogw("VideoState") << "Full preload would require "
									<< (totalmemory / (1024 * 1024))
									<< " MB. Capping buffer to " << maxframes
									<< " frames (~" << (maxmemorybytes / (1024 * 1024)) << " MB limit).";
		estimatedframes = maxframes;
	}

	// Reallocate video frame buffer for entire video
	delete framesbuffer;
	framesbuffer = new gVideoFrameRingBuffer(estimatedframes);
	framepts.clear();
	framedimensions.clear();
	needallocate = true;

	// Reallocate audio buffer for entire video (audio is much smaller, ~10 bytes/sample vs ~8MB/frame)
	if(hasaudio && audiobuffer) {
		double durationsec = static_cast<double>(formatcontext->duration) / AV_TIME_BASE;
		size_t audiosamples = static_cast<size_t>(durationsec * audiosamplerate * 2 * 1.1) + 48000;
		// Cap audio buffer proportionally (use 25% of video memory limit)
		size_t maxaudiomemory = maxmemorybytes / 4;
		size_t audiomemory = audiosamples * sizeof(float);
		if(audiomemory > maxaudiomemory) {
			audiosamples = maxaudiomemory / sizeof(float);
		}
		delete audiobuffer;
		audiobuffer = new gAudioSampleRingBuffer(audiosamples);
	}
}

void VideoState::setBufferDuration(float seconds) {
	int frames = std::max(1, static_cast<int>(seconds * avgfps));
	bufferthreshold = frames;

	// Ensure buffer capacity can hold the requested duration
	if(static_cast<int>(framesbuffer->capacity()) < frames) {
		int newcapacity = static_cast<int>(frames * 1.1) + 10;
		delete framesbuffer;
		framesbuffer = new gVideoFrameRingBuffer(newcapacity);
		framepts.clear();
		framedimensions.clear();
		needallocate = true;
	}
}

AVPixelFormat gGetCorrectedPixelFormat(AVPixelFormat l_pixelFormat) {
	// Fix swscaler deprecated pixel format warning
	// (YUVJ has been deprecated, change pixel format to regular YUV)
	switch (l_pixelFormat) {
	case AV_PIX_FMT_YUVJ420P:
		return AV_PIX_FMT_YUV420P;
	case AV_PIX_FMT_YUVJ422P:
		return AV_PIX_FMT_YUV422P;
	case AV_PIX_FMT_YUVJ444P:
		return AV_PIX_FMT_YUV444P;
	case AV_PIX_FMT_YUVJ440P:
		return AV_PIX_FMT_YUV440P;
	default:
		return l_pixelFormat;
	}
}
