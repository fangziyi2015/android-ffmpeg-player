#include "FmPlayer.h"


FmPlayer::FmPlayer(const char *dataSource, JNICallbackHelper *helper,
                   RenderCallback _renderCallback)
        : data_source(dataSource),
          helper(helper),
          renderCallback(_renderCallback) {

    pthread_mutex_init(&seek_mutex_t, nullptr);
}

FmPlayer::~FmPlayer() {

    LOGI("FmPlayer 释放资源\n")
    if (formatContext) {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        formatContext = nullptr;
    }

    DELETE(helper)
    DELETE(video_channel)
    DELETE(audio_channel)

    DELETE(data_source)

    pthread_mutex_destroy(&seek_mutex_t);
}

void *prepare_task(void *ptr) {
    auto *player = static_cast<FmPlayer *>(ptr);
    player->_prepare();
    return nullptr;
}

void *start_task(void *ptr) {
    auto *player = static_cast<FmPlayer *>(ptr);
    player->_start();
    return nullptr;
}

void FmPlayer::prepare() {
    pthread_create(&pid_prepare, nullptr, prepare_task, this);
}

void FmPlayer::start() {
    isPlaying = PLAYING;

    if (video_channel) {
        if (audio_channel) {
            video_channel->setAudioChannel(audio_channel);
        }
        video_channel->start();
    }

    if (audio_channel) {
        audio_channel->start();
    }

    pthread_create(&pid_start, nullptr, start_task, this);
}

jlong FmPlayer::getDuration() {
    return duration;
}

void FmPlayer::seek(int progress) {
    if (progress < 0 || progress > duration) {
        return;
    }

    if (!video_channel && !audio_channel) {
        return;
    }

    if (!formatContext) {
        return;
    }

    pthread_mutex_lock(&seek_mutex_t);
    /**
     * 1.formatContext 安全问题
     * 2.-1 代表默认情况，FFmpeg自动选择 音频 还是 视频 做 seek，  模糊：0视频  1音频
     * 3. AVSEEK_FLAG_ANY（老实） 直接精准到 拖动的位置，问题：如果不是关键帧，B帧 可能会造成 花屏情况
     *    AVSEEK_FLAG_BACKWARD（则优  8的位置 B帧 ， 找附件的关键帧 6，如果找不到他也会花屏）
     *    AVSEEK_FLAG_FRAME 找关键帧（非常不准确，可能会跳的太多），一般不会直接用，但是会配合用
     */
    int ret = av_seek_frame(formatContext, -1, progress * AV_TIME_BASE, AVSEEK_FLAG_FRAME);
    if (ret < 0) {
        LOGE("进度设置失败，error:%s\n", av_err2str(ret))
        pthread_mutex_unlock(&seek_mutex_t);
        return;
    }

    if (audio_channel) {
        audio_channel->packets.setWork(0);
        audio_channel->frames.setWork(0);
        audio_channel->packets.release();
        audio_channel->frames.release();
        audio_channel->packets.setWork(1);
        audio_channel->frames.setWork(1);
    }

    if (video_channel) {
        video_channel->packets.setWork(0);
        video_channel->frames.setWork(0);
        video_channel->packets.release();
        video_channel->frames.release();
        video_channel->packets.setWork(1);
        video_channel->frames.setWork(1);
    }

    pthread_mutex_unlock(&seek_mutex_t);
}

void *stop_task(void *ptr) {
    auto *player = static_cast<FmPlayer *>(ptr);
    player->_stop(player);
    return nullptr;
}

void FmPlayer::stop() {
    pthread_create(&pid_stop, nullptr, stop_task, this);
}

void FmPlayer::release() {

}

void FmPlayer::_prepare() {
    LOGI("prepare阶段\n")
    formatContext = avformat_alloc_context();
    AVDictionary *dictionary = nullptr;
    // 超时时间 5 秒
    av_dict_set(&dictionary, "timeout", "5000000", 0);
    // 1
    int ret = avformat_open_input(&formatContext, data_source, nullptr, &dictionary);
    av_dict_free(&dictionary);
    if (ret) {
        if (helper) {
            helper->error(av_err2str(ret));
        }
        LOGE("文件打开失败\n")
        avformat_close_input(&formatContext);
        return;
    }

    // 2
    ret = avformat_find_stream_info(formatContext, nullptr);
    if (ret < 0) {
        if (helper) {
            helper->error(av_err2str(ret));
        }
        LOGE("读取流信息失败\n")
        avformat_close_input(&formatContext);
        return;
    }

    if (formatContext) {
        // 需要除以时间机
        duration = formatContext->duration / AV_TIME_BASE;
        LOGI("视频总时长：%ld\n", duration)
    }

    AVCodecContext *codecContext = nullptr;

    for (int stream_index = 0; stream_index < formatContext->nb_streams; ++stream_index) {
        AVStream *stream = formatContext->streams[stream_index];
        if (!stream) {
            if (helper) {
                helper->error("流获取失败");
            }
            LOGE("流获取失败\n")
        }

        AVCodecParameters *parameters = stream->codecpar;
        if (!parameters) {
            if (helper) {
                helper->error("解码器参数获取失败");
            }
            LOGE("解码器参数获取失败\n")
        }

        AVCodec *codec = avcodec_find_decoder(parameters->codec_id);
        if (!codec) {
            if (helper) {
                helper->error("解码器获取失败");
            }
            LOGE("解码器获取失败\n")
            avformat_close_input(&formatContext);
            return;
        }

        codecContext = avcodec_alloc_context3(codec);
        if (!codecContext) {
            if (helper) {
                helper->error("解码器上下文创建失败");
            }
            LOGE("解码器上下文创建失败\n")
            avcodec_free_context(&codecContext);
            avformat_close_input(&formatContext);
            return;
        }

        ret = avcodec_parameters_to_context(codecContext, parameters);
        if (ret < 0) {
            if (helper) {
                helper->error(av_err2str(ret));
            }
            LOGE("解码器上下文参数设置失败\n")
            avcodec_free_context(&codecContext);
            avformat_close_input(&formatContext);
            return;
        }

        ret = avcodec_open2(codecContext, codec, nullptr);
        if (ret) {
            if (helper) {
                helper->error(av_err2str(ret));
            }
            LOGE("解码器打开失败\n")
            avcodec_free_context(&codecContext);
            avformat_close_input(&formatContext);
            return;
        }

        // 获取time_base
        AVRational time_base = stream->time_base;

        if (codec->type == AVMediaType::AVMEDIA_TYPE_VIDEO) {// 视频
            LOGD("当前类型是 视频流\n")
            // 需要过滤掉图片流，有些比较特殊的视频的第一个画面是一个图片流，所以需要过滤掉
            if (stream->disposition == AV_DISPOSITION_ATTACHED_PIC) {
                LOGE("当前流类型是图片流，需要过滤掉\n")
                continue;
            }
            // 视频流需要获取到fps,fps是视频独有的特性
            AVRational fps_rational = stream->avg_frame_rate;
            // 需要转成int类型
            int fps = static_cast<int>(av_q2d(fps_rational));
            LOGI("视频的fps:%d\n", fps)
            video_channel = new VideoChannel(stream_index, codecContext, time_base, fps);
            video_channel->setRenderCallback(renderCallback);
            if (duration != 0) {
                video_channel->setJNICallbackHelper(helper);
            }
        } else if (codec->type == AVMediaType::AVMEDIA_TYPE_AUDIO) {// 音频
            LOGD("当前类型是 音频流\n")
            audio_channel = new AudioChannel(stream_index, codecContext, time_base);
            if (duration != 0) {
                audio_channel->setJNICallbackHelper(helper);
            }

        }
    }// for end

    if (!video_channel && !audio_channel) {
        if (helper) {
            helper->error("没有获取到视频流");
        }
        LOGE("没有获取到视频流\n")
        if (codecContext) {
            avcodec_free_context(&codecContext);
        }
        avformat_close_input(&formatContext);
        return;
    }

    if (helper) {
        LOGD("流获取成功\n")
        helper->prepare();
    }
}

void FmPlayer::_start() {
    LOGD("进入解包阶段\n")
    while (isPlaying) {
        if (video_channel && video_channel->packets.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }

        if (audio_channel && audio_channel->packets.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }

        AVPacket *packet = av_packet_alloc();
        int ret = av_read_frame(formatContext, packet);
        if (ret == AVERROR_EOF) {
            LOGI("已经读取到文件的末尾了")
            if (video_channel && video_channel->packets.empty() &&
                audio_channel && audio_channel->packets.empty()) {
                LOGE("已经没有包了，退出循环")
                break;
            }
        } else if (ret < 0) {
            LOGE("获取原始数据包失败 msg :%s\n", av_err2str(ret))
            break;
        }

        if (video_channel && video_channel->stream_index == packet->stream_index) {
            video_channel->packets.insertData(packet);
        } else if (audio_channel && audio_channel->stream_index == packet->stream_index) {
            audio_channel->packets.insertData(packet);
        }
    }

    isPlaying = STOP;
    if (video_channel){
        video_channel->stop();
    }
    if (audio_channel){
        audio_channel->stop();
    }
}

void FmPlayer::_stop(FmPlayer *player) {
    isPlaying = STOP;
    pthread_join(pid_prepare, nullptr);
    pthread_join(pid_start, nullptr);
    DELETE(player)
}





