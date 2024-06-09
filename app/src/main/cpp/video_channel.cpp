#include "video_channel.h"


VideoChannel::VideoChannel(int streamIndex, AVCodecContext *codecContext)
        : BaseChannel(streamIndex,
                      codecContext) {

}

void *video_start_task(void *ptr) {
    auto *channel = static_cast<VideoChannel *>(ptr);
    channel->_video_start();
    return nullptr;
}

void *video_play_task(void *ptr) {
    auto *channel = static_cast<VideoChannel *>(ptr);
    channel->_video_play();
    return nullptr;
}

void VideoChannel::start() {
    isPlaying = PLAYING;
    packets.setWork(1);
    frames.setWork(1);
    pthread_create(&pid_video_start, nullptr, video_start_task, this);
    pthread_create(&pid_video_play, nullptr, video_play_task, this);
}

void VideoChannel::_video_start() {
    AVPacket *packet = nullptr;
    while (isPlaying) {
        if (isPlaying && frames.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }
        int ret = packets.getData(packet);
        if (!ret) {
            LOGE("没有获取到视频数据，重新获取\n")
            continue;
        }

        ret = avcodec_send_packet(codecContext, packet);
        if (ret) {
            LOGE("数据发送失败，msg:%s\n", av_err2str(ret))
            continue;
        }
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            LOGI("可能读了B帧，重新获取\n")
            continue;
        } else if (ret) {
            LOGI("视频帧数据获取失败，退出循环\n")
            break;
        }

        frames.insertData(frame);
        av_packet_unref(packet);
        releaseAVPacket(&packet);
    }
    isPlaying = STOP;
    av_packet_unref(packet);
    releaseAVPacket(&packet);
}

void VideoChannel::_video_play() {
    SwsContext *swsContext = sws_getContext(
            codecContext->width,
            codecContext->height,
            codecContext->pix_fmt,
            codecContext->width,
            codecContext->height,
            AV_PIX_FMT_RGBA,
            0,
            nullptr,
            nullptr,
            nullptr
    );

    uint8_t *dst_data[4];
    int dst_linesize[4];
    av_image_alloc(
            dst_data,
            dst_linesize,
            codecContext->width,
            codecContext->height,
            AV_PIX_FMT_RGBA,
            1
    );

    AVFrame *frame = nullptr;
    while (isPlaying) {
        int ret = frames.getData(frame);
        if (!ret) {
            LOGE("原始数据获取失败,重新获取\n")
            continue;
        }

        sws_scale(
                swsContext,
                frame->data,
                frame->linesize,
                0,
                codecContext->height,
                dst_data,
                dst_linesize
        );

        if (renderCallback) {
            renderCallback(dst_data[0], dst_linesize[0], codecContext->width, codecContext->height);
        }

        av_frame_unref(frame);
        releaseAVFrame(&frame);
    }

    isPlaying = STOP;
    sws_freeContext(swsContext);
    av_frame_unref(frame);
    av_free(&dst_data[0]);
    releaseAVFrame(&frame);
}

void VideoChannel::setRenderCallback(RenderCallback _renderCallback) {
    this->renderCallback = _renderCallback;
}
