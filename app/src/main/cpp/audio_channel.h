#ifndef ANDROID_FFMPEG_PLAYER_AUDIO_CHANNEL_H
#define ANDROID_FFMPEG_PLAYER_AUDIO_CHANNEL_H

#include "base_channel.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

using namespace std;

extern "C" {
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
};


class AudioChannel : public BaseChannel {
public:
    int out_audio_channel_num = 0;
    int out_sample_size = 0;
    int out_sample_rate = 0;

    int out_buffer_size = 0;
    ::uint8_t *out_buffer = nullptr;

    pthread_t pid_start = 0L;
    pthread_t pid_play = 0L;

    SwrContext *swr_ctx = nullptr;

    SLObjectItf audio_engine = nullptr;
    SLEngineItf audio_engine_ift= nullptr;

    SLObjectItf outputMixInterface= nullptr;
    SLObjectItf audioPlayer= nullptr;
    SLPlayItf playerInterface= nullptr;
    SLAndroidSimpleBufferQueueItf playerQueueInterface= nullptr;

public:
    AudioChannel(int streamIndex, AVCodecContext *codecContext);

    ~AudioChannel();

    void start();


    void _start();

    void _play();


    SLint32 get_pcm_data_size();
};


#endif //ANDROID_FFMPEG_PLAYER_AUDIO_CHANNEL_H
