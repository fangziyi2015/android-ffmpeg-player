#ifndef FMPLAYER_VIDEO_CHANNEL_H
#define FMPLAYER_VIDEO_CHANNEL_H

#include "base_channel.h"

extern "C" {
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
};

typedef void (*RenderCallback)(uint8_t *src_data, int src_linesize, int w, int h);

class VideoChannel : public BaseChannel {
private:
    pthread_t pid_video_start;
    pthread_t pid_video_play;
    RenderCallback renderCallback;
public:
    VideoChannel(int streamIndex, AVCodecContext *codecContext);

    void start();

    void setRenderCallback(RenderCallback _renderCallback);

    void _video_start();

    void _video_play();

};


#endif //FMPLAYER_VIDEO_CHANNEL_H
