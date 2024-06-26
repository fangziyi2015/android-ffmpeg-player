#ifndef FMPLAYER_FMPLAYER_H
#define FMPLAYER_FMPLAYER_H

#define PLAYING 1
#define STOP 0

#include "log4c.h"
#include "JNICallbackHelper.h"
#include "video_channel.h"
#include "audio_channel.h"
#include <pthread.h>
#include "util.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/time.h"
};

class FmPlayer {
private:
    const char *data_source = nullptr;
    JNICallbackHelper *helper = nullptr;
    AVFormatContext *formatContext = nullptr;
    VideoChannel *video_channel = nullptr;
    AudioChannel *audio_channel = nullptr;

    pthread_t pid_prepare;
    pthread_t pid_start;
    pthread_t pid_stop;

    bool isPlaying = STOP;

    RenderCallback renderCallback;
    long duration;

    pthread_mutex_t seek_mutex_t;

public:
    FmPlayer(const char *dataSource, JNICallbackHelper *helper,RenderCallback _renderCallback);

    ~FmPlayer();

    /**
     * 准备
     */
    void prepare();

    /**
     * 播放
     */
    void start();

    /**
     * 停止
     */
    void stop();

    /**
     * 释放资源
     */
    void release();

    /**
     * 在子线程做准备工作
     */
    void _prepare();

    /**
     * 在子线程做解包工作
     */
    void _start();


    jlong getDuration();

    void seek(int progress);

    void _stop(FmPlayer *player);


};


#endif //FMPLAYER_FMPLAYER_H
