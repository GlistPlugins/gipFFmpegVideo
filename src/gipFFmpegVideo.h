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
	void setPaused(bool isPaused);

    double_t getPosition(); // seconds
	double_t getDuration(); // seconds
    int getWidth();
    int getHeight();

    void setSpeed(float speed);
    void setVolume(float volume);
    float getSpeed();
    float getVolume();

    bool isPlaying();
    bool isPaused();

    void setAppFps(int appfps);

private:
    gTexture* framebuffer;
    uint8_t* framedata;
    int64_t framecount;
    int currentframe;

    int width;
    int height;
    bool ispaused;
	int64_t duration, position; // seconds
    bool couldopen;
    bool closed;
    bool isplaying;

    AVRational time_base;
    int fps;
    float speed;

    //Audio
    float volume;

    std::string filepath;

    gipFFmpegUtils utils;
    int appfps;
    double avgfps;
    double fpsintervalnum;
    int fpsintervalno;
    int quotientnum;
    int quotientno;
};

#endif /* GIP_FFMPEGVIDEO_H */
