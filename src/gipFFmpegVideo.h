/*
 *  gFFmpegVideo.h
 *
 *  Created on: 10 Jul 2021
 *      Author: kayra
 *      Edited By: Umutcan Türkmen 24 Feb 2023
 */

#ifndef GIP_FFMPEGVIDEO_H
#define GIP_FFMPEGVIDEO_H

#include "gBasePlugin.h"
#include "gImage.h"
#include "gTexture.h"
#include "gipFFmpegUtils.h"

#include <GLFW/glfw3.h>
#include <libavutil/rational.h>
#include <memory>

struct gVideoAudioContext;

class gipFFmpegVideo : public gBasePlugin {
public:
	gipFFmpegVideo();
	virtual ~gipFFmpegVideo();

	/**
     * Loads a video from an absolute path. Audio is auto-detected.
     */
	void load(std::string fullPath);

	/**
     * Loads a video from the assets/videos/ directory.
     */
	void loadVideo(std::string videoPath);

	void update();
	void draw();
	void draw(int x, int y);
	void draw(int x, int y, int w, int h);

	void play();

	/**
     * Stops playback and reloads the video from scratch, rewinding to the start.
     */
	void stop();

	/**
     * Releases all resources. The video cannot be played again without reloading.
     */
	void close();

	void setPaused(bool paused);

	void setPosition(float timeInSeconds);

	/** Returns the current playback position in seconds. */
	double getPosition();

	/** Returns the total duration of the video in seconds. */
	double getDuration();
	int getWidth();
	int getHeight();

	void setSpeed(float speed);

	/**
     * @param vol 0.0 (mute) to 1.0 (full). Values above 1.0 amplify.
     */
	void setVolume(float vol);
	float getSpeed();
	float getVolume();

	/**
     * Decodes all frames into memory before playback starts.
     *
     * Gives the smoothest playback but uses a lot of RAM (~width*height*4 bytes
     * per frame). Memory is capped by setMaxPreloadMemory() (default 2 GB);
     * if the video exceeds that, it gracefully falls back to a large streaming
     * buffer.
     *
     * Must be called after loadVideo() and before play(). Check isLoading() to
     * know when preloading is still in progress.
     *
     * @code
     * video.loadVideo("intro.mp4");
     * video.setMaxPreloadMemory(512 * 1024 * 1024); // optional, before setPreloaded
     * video.setPreloaded(true);
     * video.setPaused(false);
     * video.play();
     * // in draw(): if(!video.isLoading()) video.draw(0, 0);
     * @endcode
     */
	void setPreloaded(bool preload);

	/**
     * Sets the maximum memory (in bytes) that preloaded mode may use for decoded
     * frames. Must be called before setPreloaded(). Default is 2 GB.
     */
	void setMaxPreloadMemory(size_t bytes);

	/**
     * Sets how many seconds of frames to buffer before playback starts in
     * streaming (non-preloaded) mode. Default is ~4 frames worth. Higher values
     * add startup delay but reduce mid-playback stalls on slow I/O.
     * Must be called after loadVideo().
     */
	void setBufferDuration(float seconds);

	/**
     * Returns true while the video is buffering: either during initial load
     * (before enough frames are ready to play) or during playback if the
     * buffer runs dry (e.g. slow I/O, high bitrate). Useful for showing a
     * loading/buffering indicator.
     *
     * @code
     * if(video.isLoading()) {
     *     drawBufferingSpinner();
     * } else {
     *     video.draw(0, 0);
     * }
     * @endcode
     */
	bool isLoading();

	bool isPlaying();
	bool isPaused();

private:
	gTexture* framebuffer{};
	int currentframe{};

	std::shared_ptr<VideoState> videostate;
	bool ispaused{false};
	bool isplaying{false};
	int64_t durationInSec{};
	int64_t positionInSec{};// seconds
	float fpsquotient{0.0f};
	float fpsquotientcumulative{0.0f};

	AVRational time_base{};
	float speed{};

	//Audio
	float volume{1.0f};
	gVideoAudioContext* audiocontext{};
	bool audiostarted{false};

	size_t maxpreloadmemory{2ULL * 1024 * 1024 * 1024};// 2 GB default

	void initAudio();
	void cleanupAudio();

	std::string filepath;
};

#endif /* GIP_FFMPEGVIDEO_H */
