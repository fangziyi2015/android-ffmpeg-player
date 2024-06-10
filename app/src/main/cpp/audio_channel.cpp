#include "audio_channel.h"

AudioChannel::AudioChannel(int streamIndex, AVCodecContext *codecContext, AVRational time_base)
        : BaseChannel(streamIndex,
                      codecContext, time_base) {

    out_audio_channel_num = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    out_sample_size = av_get_bytes_per_sample(AVSampleFormat::AV_SAMPLE_FMT_S16);
    out_sample_rate = 44100;

    out_buffer_size = out_audio_channel_num * out_sample_size * out_sample_rate;
    out_buffer = static_cast<::uint8_t *>(::malloc(out_buffer_size));
    swr_ctx = swr_alloc_set_opts(
            nullptr,
            AV_CH_LAYOUT_STEREO,
            AVSampleFormat::AV_SAMPLE_FMT_S16,
            out_sample_rate,
            codecContext->channel_layout,
            codecContext->sample_fmt,
            codecContext->sample_rate,
            0,
            nullptr
    );

    swr_init(swr_ctx);

}

void playCallback(SLAndroidSimpleBufferQueueItf bq, void *ptr);

void *audio_start_task(void *ptr) {
    auto *channel = static_cast<AudioChannel *>(ptr);
    channel->_start();
    return nullptr;
}

void *audio_play_task(void *ptr) {
    auto *channel = static_cast<AudioChannel *>(ptr);
    channel->_play();
    return nullptr;
}

void AudioChannel::start() {
    isPlaying = PLAYING;
    packets.setWork(1);
    frames.setWork(1);

    pthread_create(&pid_start, nullptr, audio_start_task, this);

    pthread_create(&pid_play, nullptr, audio_play_task, this);
}

void AudioChannel::_start() {
    AVPacket *packet = nullptr;
    while (isPlaying) {
        if (isPlaying && frames.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }
        int ret = packets.getData(packet);
        if (!ret) {
            LOGE("音频包获取失败，重新获取\n")
            continue;
        }

        ret = avcodec_send_packet(codecContext, packet);
        if (ret) {
            LOGE("音频包发送失败，重新发送，err:%s\n", av_err2str(ret))
            break;
        }

        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            LOGE("音频也可能有B帧，重新获取\n")
            continue;
        } else if (ret) {
            LOGE("音频包接收失败，退出循环，err:%s\n", av_err2str(ret))
            break;
        }

        frames.insertData(frame);
        av_packet_unref(packet);
        releaseAVPacket(&packet);
    }
    av_packet_unref(packet);
    releaseAVPacket(&packet);
}

void AudioChannel::_play() {
    SLresult result;

    // 1、创建音频引擎
    result = slCreateEngine(&audio_engine,
                            0,
                            nullptr,
                            0,
                            nullptr,
                            nullptr
    );

    if (SL_RESULT_SUCCESS != result) {
        LOGE("音频引擎创建失败\n")
        return;
    }
    LOGI("音频引擎创建 成功\n")

    // 2、初始化引擎
    result = (*audio_engine)->Realize(audio_engine, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("音频引擎初始化失败\n")
        return;
    }
    LOGI("音频引擎初始化 成功\n")

    // 3、获取引擎接口
    result = (*audio_engine)->GetInterface(
            audio_engine,
            SL_IID_ENGINE,
            &audio_engine_ift
    );
    if (SL_RESULT_SUCCESS != result) {
        LOGE("音频引擎接口获取失败\n")
        return;
    }
    LOGI("音频引擎接口获取失败 成功\n")

    // 4、创建混音器接口
    result = (*audio_engine_ift)->CreateOutputMix(audio_engine_ift, &outputMixInterface, 0, 0, 0);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("创建混音器接口 失败")
        return;
    }
    LOGI("创建混音器接口 成功\n")
    // 5、初始化混音器接口
    result = (*outputMixInterface)->Realize(outputMixInterface, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("初始化混音器接口 失败")
        return;
    }
    LOGI("初始化混音器接口 成功\n")

    // 不启用混响可以不用获取混音器接口 【声音的效果】
    // 获得混音器接口
    /*
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                             &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
    // 设置混响 ： 默认。
    SL_I3DL2_ENVIRONMENT_PRESET_ROOM: 室内
    SL_I3DL2_ENVIRONMENT_PRESET_AUDITORIUM : 礼堂 等
    const SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;
    (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
           outputMixEnvironmentalReverb, &settings);
    }
    */
    // 6、创建buff
    SLDataLocator_AndroidSimpleBufferQueue locabuff =
            {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 10};

    // 7、设置音频格式参数
    // pcm数据格式 == PCM是不能直接播放，mp3可以直接播放(参数集)，人家不知道PCM的参数
    //  SL_DATAFORMAT_PCM：数据格式为pcm格式
    //  2：双声道
    //  SL_SAMPLINGRATE_44_1：采样率为44100
    //  SL_PCMSAMPLEFORMAT_FIXED_16：采样格式为16bit
    // SL_PCMSAMPLEFORMAT_FIXED_16：数据大小为16bit
    // SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT：左右声道（双声道）
    // SL_BYTEORDER_LITTLEENDIAN：小端模式
    SLDataFormat_PCM formatPcm = {
            SL_DATAFORMAT_PCM,
            2,
            SL_SAMPLINGRATE_44_1,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
            SL_BYTEORDER_LITTLEENDIAN
    };

    // 8、把 buff 和 pcm 包装起来
    SLDataSource slDataSource = {&locabuff, &formatPcm};

    // 9、设置混音器为输出类型
    SLDataLocator_OutputMix outputMix = {
            SL_DATALOCATOR_OUTPUTMIX,
            outputMixInterface
    };

    // 10、混音器成果
    SLDataSink audioSnk = {&outputMix, NULL};

    // 11、音频接口buff
    const SLInterfaceID slInterfaceId[1] = {SL_IID_BUFFERQUEUE};
    // 12、是否需要buff
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};

    // 13、创建音频播放器
    result = (*audio_engine_ift)->CreateAudioPlayer(
            audio_engine_ift,// 参数1：引擎接口
            &audioPlayer,// 参数2：播放器
            &slDataSource,// 参数3：音频配置信息
            &audioSnk,// 参数4：混音器
            1,// 参数5：开放的参数的个数
            slInterfaceId,// 参数6：代表我们需要 Buff
            req // 参数7：代表我们上面的Buff 需要开放出去
    );

    if (SL_RESULT_SUCCESS != result) {
        LOGE("创建音频播放器 失败")
        return;
    }
    LOGI("创建音频播放器 成功\n")

    // 14、初始化音频播放器
    result = (*audioPlayer)->Realize(audioPlayer, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("初始化音频播放器 失败")
        return;
    }
    LOGI("初始化音频播放器 成功\n")
    // 15、获取播放器接口
    result = (*audioPlayer)->GetInterface(
            audioPlayer,
            SL_IID_PLAY,
            &playerInterface
    );

    if (SL_RESULT_SUCCESS != result) {
        LOGE("获取播放器接口 失败")
        return;
    }
    LOGI("获取播放器接口 成功\n")

    // 16、获取播放器队列接口 SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue： 播放需要的队列
    result = (*audioPlayer)->GetInterface(
            audioPlayer,
            SL_IID_BUFFERQUEUE,
            &playerQueueInterface
    );

    if (SL_RESULT_SUCCESS != result) {
        LOGE("获取播放器队列接口 失败")
        return;
    }
    LOGI("获取播放器队列接口 成功\n")
    // 17、注册播放器队列回调函数
    result = (*playerQueueInterface)->RegisterCallback(
            playerQueueInterface,// 传入刚刚设置好的队列
            playCallback,
            this
    );
    if (SL_RESULT_SUCCESS != result) {
        LOGE("注册播放器队列回调函数 失败")
        return;
    }

    LOGI("注册播放器队列回调函数 成功\n")

    // 18、设置播放器状态
    result = (*playerInterface)->SetPlayState(playerInterface, SL_PLAYSTATE_PLAYING);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("设置播放器状态 失败")
        return;
    }
    LOGI("设置播放器状态 成功\n")

    // 19、需要手动调用一下这个函数，激活回调
    playCallback(playerQueueInterface, this);
    LOGI("手动激活回调函数 Success\n")
}

SLint32 AudioChannel::get_pcm_data_size() {
    int pcm_data_size = 0;
    AVFrame *frame = nullptr;
    while (isPlaying) {
        int ret = frames.getData(frame);
        if (!ret) {
            LOGE("音频frame获取失败,重新获取\n")
            continue;
        }

        // 获取单通道的样本数
        ::int64_t dst_sample_size = av_rescale_rnd(
                swr_get_delay(swr_ctx, frame->sample_rate) + frame->nb_samples,
                out_sample_rate,
                frame->sample_rate,
                AV_ROUND_UP
        );

        // pcm的处理逻辑
        // 音频播放器的数据格式是我们自己在下面定义的
        // 而原始数据（待播放的音频pcm数据）
        // 返回的结果：每个通道输出的样本数(注意：是转换后的)    做一个简单的重采样实验(通道基本上都是:1024)
        int sample_per_channel = swr_convert(
                swr_ctx,
                &out_buffer,
                dst_sample_size,
                (const uint8_t **) (frame->data),
                frame->nb_samples
        );

        pcm_data_size = sample_per_channel * out_audio_channel_num * out_sample_size;

        // 在这里计算音频的time_base
        audio_time = frame->best_effort_timestamp * av_q2d(time_base);
        LOGI("音频进度  best_effort_timestamp：%lld   av_q2d(time_base): %lf audio_time: %lf\n",
             frame->best_effort_timestamp, av_q2d(time_base), audio_time)
        if (helper) {
            long progress = static_cast<long>(audio_time) + 1;
            //LOGI("音频进度：%ld\n",progress)
            helper->onProgress(progress);
        }
        break;
    }
    av_frame_unref(frame);
    releaseAVFrame(&frame);
    return pcm_data_size;
}

AudioChannel::~AudioChannel() {
    if (out_buffer) {
        ::free(out_buffer);
        out_buffer = nullptr;
    }

    if (swr_ctx) {
        swr_free(&swr_ctx);
        swr_ctx = nullptr;
    }
}

void playCallback(SLAndroidSimpleBufferQueueItf bq, void *ptr) {
    auto *channel = static_cast<AudioChannel *>(ptr);
    SLint32 pcm_data_size = channel->get_pcm_data_size();
    (*bq)->Enqueue(
            bq,
            channel->out_buffer,
            pcm_data_size
    );
}
