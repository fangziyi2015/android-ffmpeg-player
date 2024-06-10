#ifndef FMPLAYER_BASE_CHANNEL_H
#define FMPLAYER_BASE_CHANNEL_H

#define PLAYING 1
#define STOP 0

#include "safe_queue.h"
#include "log4c.h"
#include "JNICallbackHelper.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/time.h"
}

class BaseChannel {
public:
    SafeQueue<AVPacket *> packets;
    SafeQueue<AVFrame *> frames;
    int stream_index;
    AVCodecContext *codecContext = nullptr;

    bool isPlaying = STOP;

    // 需要同步，同步需要时间基 time_base
    AVRational time_base;

    JNICallbackHelper *helper;

public:
    BaseChannel(int streamIndex,
                AVCodecContext *codecContext,
                AVRational time_base) : stream_index(streamIndex),
                                        codecContext(codecContext),
                                        time_base(time_base) {

        packets.setReleaseCallback(releaseAVPacket);
        frames.setReleaseCallback(releaseAVFrame);
    }

    ~BaseChannel() {
        packets.release();
        frames.release();
        if (codecContext) {
            avcodec_free_context(&codecContext);
        }
    }

    void setJNICallbackHelper(JNICallbackHelper *_helper) {
        this->helper = _helper;
    }

    static void releaseAVPacket(AVPacket **packet) {
        if (packet) {
            av_packet_free(packet);
            *packet = nullptr;
        }
    }

    static void releaseAVFrame(AVFrame **frame) {
        if (frame) {
            av_frame_free(frame);
            *frame = nullptr;
        }
    }
};

#endif //FMPLAYER_BASE_CHANNEL_H
