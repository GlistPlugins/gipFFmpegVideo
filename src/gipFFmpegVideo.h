/*
 *  gFFmpegVideo.h
 *
 *  Created on: 10 Jul 2021
 *      Author: kayra
 *      Edited By: Umutcan Türkmen 24 Feb 2023
 */

#ifndef GIP_FFMPEGVIDEO_H
#define GIP_FFMPEGVIDEO_H

#include "gTexture.h"
#include "gBasePlugin.h"
#include "gImage.h"
#include "gipFFmpegUtils.h"

#include <GLFW/glfw3.h>
#include <libavutil/rational.h>
#include <memory>

struct gVideoAudioContext;

class gipFFmpegVideo : public gBasePlugin {
public:
	gipFFmpegVideo();
    virtual ~gipFFmpegVideo();

    void load(std::string fullPath);
    void loadVideo(std::string videoPath);

    void update();
    void draw();
    void draw(int x, int y);
    void draw(int x, int y, int w, int h);

    void play();
    void stop();
    void close();
	void setPaused(bool t_isPaused);

    void setPosition(float t_timeInSeconds);
    double getPosition(); // seconds
	double getDuration(); // seconds
    int getWidth();
    int getHeight();

    void setSpeed(float t_speed);
    void setVolume(float t_volume);
    float getSpeed();
    float getVolume();

    bool isPlaying();
    bool isPaused();

private:
    gTexture* framebuffer{};
    int currentframe{};

    std::shared_ptr<VideoState> videostate;
    bool ispaused{false};
    bool isplaying{false};
	int64_t durationInSec{};
    int64_t positionInSec{}; // seconds
    float fpsquotient{0.0f};
    float fpsquotientcumulative{0.0f};

    AVRational time_base{};
    float speed{};

    //Audio
    float volume{1.0f};
    gVideoAudioContext* audiocontext{};
    bool audiostarted{false};

    void initAudio();
    void cleanupAudio();

    std::string filepath;
};

#endif /* GIP_FFMPEGVIDEO_H */
