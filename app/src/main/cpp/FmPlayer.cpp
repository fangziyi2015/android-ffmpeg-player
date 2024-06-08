#include "FmPlayer.h"

FmPlayer::FmPlayer(const char *dataSource, JNICallbackHelper *helper,
                   RenderCallback _renderCallback)
        : data_source(dataSource),
          helper(helper),
          renderCallback(_renderCallback) {

}

FmPlayer::~FmPlayer() {
    if (helper) {
        delete helper;
        helper = nullptr;
    }
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
        video_channel->start();
    }

    pthread_create(&pid_start, nullptr, start_task, this);
}

void FmPlayer::stop() {
    isPlaying = STOP;
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
        return;
    }

    // 2
    ret = avformat_find_stream_info(formatContext, nullptr);
    if (ret < 0) {
        if (helper) {
            helper->error(av_err2str(ret));
        }
        LOGE("读取流信息失败\n")
        return;
    }

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
        }
        AVCodecContext *codecContext = avcodec_alloc_context3(codec);
        if (!codecContext) {
            if (helper) {
                helper->error("解码器上下文创建失败");
            }
            LOGE("解码器上下文创建失败\n")
        }

        ret = avcodec_parameters_to_context(codecContext, parameters);
        if (ret < 0) {
            if (helper) {
                helper->error(av_err2str(ret));
            }
            LOGE("解码器上下文参数设置失败\n")
            return;
        }

        ret = avcodec_open2(codecContext, codec, nullptr);
        if (ret) {
            if (helper) {
                helper->error(av_err2str(ret));
            }
            LOGE("解码器打开失败\n")
            return;
        }

        if (codec->type == AVMediaType::AVMEDIA_TYPE_VIDEO) {// 视频
            LOGD("当前类型是 视频流\n")
            video_channel = new VideoChannel(stream_index, codecContext);
            video_channel->setRenderCallback(renderCallback);
        } else if (codec->type == AVMediaType::AVMEDIA_TYPE_AUDIO) {// 音频
            LOGD("当前类型是 音频流\n")
        }
    }// for end

    if (!video_channel) {
        if (helper) {
            helper->error("没有获取到视频流");
        }
        LOGE("没有获取到视频流\n")
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

        if (isPlaying && video_channel->packets.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }

        if (!isPlaying) {
            break;
        }

        AVPacket *packet = av_packet_alloc();
        int ret = av_read_frame(formatContext, packet);
        if (ret == AVERROR_EOF) {
            LOGI("已经读取到文件的末尾了")
            if (video_channel->packets.empty()) {
                LOGI("已经没有包了，退出循环")
                break;
            }
        } else if (ret < 0) {
            LOGE("获取原始数据包失败 msg :%s\n", av_err2str(ret))
            break;
        }

        if (video_channel && video_channel->stream_index == packet->stream_index) {
            LOGI("获取到的是视频流的包")
            video_channel->packets.insertData(packet);
        }
    }

    isPlaying = STOP;
}




