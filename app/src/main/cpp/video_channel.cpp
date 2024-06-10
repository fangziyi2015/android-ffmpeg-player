#include "video_channel.h"

void removeAVFrame(queue<AVFrame *> &q) {
    if (!q.empty()) {
        AVFrame *frame = q.front();
        BaseChannel::releaseAVFrame(&frame);
        q.pop();
    }
}

void removeAVPacket(queue<AVPacket *> &q) {
    while (!q.empty()) {
        AVPacket *packet = q.front();
        if (packet->flags != AV_PKT_FLAG_KEY) {
            BaseChannel::releaseAVPacket(&packet);
            q.pop();
        } else {
            LOGE("当前是关键帧，不能丢掉，直接结束\n")
            break;
        }
    }
}

VideoChannel::VideoChannel(int streamIndex, AVCodecContext *codecContext, AVRational time_base,
                           int fps)
        : BaseChannel(streamIndex,
                      codecContext, time_base), fps(fps) {

    frames.setSyncCallback(removeAVFrame);
    packets.setSyncCallback(removeAVPacket);
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
            break;
        }
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            LOGI("可能读了B帧，重新获取\n")
            continue;
        } else if (ret) {
            LOGI("视频帧数据获取失败，退出循环\n")
            if (frame) {
                releaseAVFrame(&frame);
            }
            break;
        }

        frames.insertData(frame);
        av_packet_unref(packet);
        releaseAVPacket(&packet);
    }
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
            SWS_BILINEAR,
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

        // extra_delay = repeat_pict / (2*fps)
        double extra_delay = frame->repeat_pict / (2 * fps);
        // 每帧间隔时间 1.0 / fps ，假若fps25 则 1.0 / 25 = 0.04
        double real_delay = extra_delay + 1.0 / fps;

        // 在这里做同步的工作
        double video_time = frame->best_effort_timestamp * av_q2d(time_base);
        double time_diff = video_time - audio_channel->audio_time;
        if (time_diff > 0) {
            // 视频时间 > 音频时间 ，这种情况就要睡眠等待音频
            if (time_diff > 1) {
                LOGI("时间差很大\n")
                av_usleep(real_delay * 2.0 * 1000000);
            } else {
                av_usleep((real_delay + time_diff) * 1000000);
            }
        } else if (time_diff < 0) {
            // 视频时间 < 音频时间 ，这种情况就需要丢帧
            if (fabs(time_diff) <= 0.05) {
                frames.sync();
                continue;
            }
        } else {
            LOGI("百分百同步")
            av_usleep(real_delay * 1000000);
        }

        if (renderCallback) {
            renderCallback(dst_data[0], dst_linesize[0], codecContext->width, codecContext->height);
        }

        av_frame_unref(frame);
        releaseAVFrame(&frame);
    }

    isPlaying = STOP;
    av_frame_unref(frame);
    releaseAVFrame(&frame);
    if (dst_data[0]) {
        av_free(&dst_data[0]);
    }
    sws_freeContext(swsContext);
}

void VideoChannel::setRenderCallback(RenderCallback _renderCallback) {
    this->renderCallback = _renderCallback;
}

void VideoChannel::setAudioChannel(AudioChannel *_audio_channel) {
    this->audio_channel = _audio_channel;
}
