//
// Created by 曾辉 on 2019-06-06.
//
#include "audio_core.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

AudioCore::AudioCore(int stream_index, PlayStatus *pPlayStatus, JNIPlayerCall *pPlayerCall)
        : MediaCore(stream_index, pPlayStatus, pPlayerCall) {
}

AudioCore::~AudioCore() {
    release();
}

void AudioCore::prepare(ThreadMode mode, AVFormatContext *pFormatContext) {
    MediaCore::prepare(mode, pFormatContext);

    // 处理一些异常的问题
    if (codec_ctx->channels > 0 && codec_ctx->channel_layout == 0) {
        codec_ctx->channel_layout = av_get_default_channel_layout(codec_ctx->channels);
    } else if (codec_ctx->channels == 0 && codec_ctx->channel_layout > 0) {
        codec_ctx->channels = av_get_channel_layout_nb_channels(codec_ctx->channel_layout);
    }

    // 初始化 SwrContext
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO; //输出的声道布局
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16; //输出的采样位数
    int out_sample_rate = codec_ctx->sample_rate; //输出的采样率
    AVChannelLayout in_ch_layout = codec_ctx->ch_layout; //输入的声道布局
    enum AVSampleFormat in_sample_fmt = codec_ctx->sample_fmt; //输入的采样位数
    int in_sample_rate = codec_ctx->sample_rate; //输入的采样率
    swr_alloc_set_opts2(&swrContext, &out_ch_layout, out_sample_fmt, out_sample_rate,
                        &in_ch_layout, in_sample_fmt, in_sample_rate, 0, nullptr);
    int swr_init_res = swr_init(swrContext);
    if (swrContext == NULL || swr_init_res < 0) {
        LOGE("init SwrContext error : %s", av_err2str(swr_init_res));
        on_error(mode, swr_init_res, av_err2str(swr_init_res));
        return;
    }

    stream_buffer_size = codec_ctx->frame_size * 2 * 2;
    stream_buffer = (uint8_t *) malloc(stream_buffer_size);
}

void AudioCore::decode() {
    AVPacket *avPacket = av_packet_alloc();
    AVFrame *avFrame = av_frame_alloc();
    while (play_status != NULL && !play_status->isExit) {
        // 是不是暂停或者 seek 中
        if (play_status != NULL) {
            if (play_status->isPause || play_status->isSeek) {
                av_usleep(10 * 1000);
                continue;
            }
        }

        // 根据队列中是否有数据来判断是否加载中
        if (packet_queue != NULL && packet_queue->empty()) {
            if (play_status != NULL && !play_status->isLoading) {
                play_status->isLoading = true;
                if (jni_player_call != NULL) {
                    jni_player_call->onCallLoading(THREAD_CHILD, play_status->isLoading);
                }
            }
            continue;
        } else {
            if (play_status != NULL && play_status->isLoading) {
                play_status->isLoading = false;
                if (jni_player_call != NULL) {
                    jni_player_call->onCallLoading(THREAD_CHILD, play_status->isLoading);
                }
            }
        }

        // 加锁，防止 seek 时获取到脏数据
        pthread_mutex_lock(&seek_mutex);
        packet_queue->pop(avPacket);

        // 解码 avcodec_send_packet -> avcodec_receive_frame
        int send_packet_res = avcodec_send_packet(codec_ctx, avPacket);
        if (send_packet_res == 0) {
            int receive_frame_res = avcodec_receive_frame(codec_ctx, avFrame);
            pthread_mutex_unlock(&seek_mutex);
            if (receive_frame_res == 0) {
                swr_convert(swrContext, &stream_buffer, avFrame->nb_samples, (
                        const uint8_t **) (avFrame->data), avFrame->nb_samples);
                int time = avFrame->pts * av_q2d(rational);
                if (time > position) {
                    position = time;
                }
                break;
            }
        } else {
            pthread_mutex_unlock(&seek_mutex);
        }

        // 释放 data 数据，释放 AVPacket 开辟的内存
        av_packet_unref(avPacket);
        av_frame_unref(avFrame);
    }
    av_packet_free(&avPacket);
    av_frame_free(&avFrame);
}

void *threadDecodePlay(void *data) {
    AudioCore *audio = (AudioCore *) (data);
    LOGE("audio -> %p", audio);
    audio->init();
    pthread_exit((void *) 1);
}

void AudioCore::play() {
    pthread_create(&playThreadT, NULL, threadDecodePlay, this);
}

void bufferQueueCallback(SLAndroidSimpleBufferQueueItf bufferQueueItf, void *context) {
    AudioCore *audio = (AudioCore *) context;
    if (audio != NULL) {
        audio->decode();
        audio->position +=
                audio->stream_buffer_size / ((double) (audio->codec_ctx->sample_rate * 2 * 2));
        // 0.5 回调更新一次进度
        if (audio->position - audio->last_update_time > 1) {
            audio->last_update_time = audio->position;
            if (audio->jni_player_call != NULL) {
                audio->jni_player_call->onCallProgress(THREAD_CHILD, audio->position,
                                                       audio->duration);
            }
        }

        if (audio->duration > 0 && audio->duration <= audio->position) {
            audio->jni_player_call->onCallComplete(THREAD_CHILD);
        }

        (*bufferQueueItf)->Enqueue(bufferQueueItf, (char *) audio->stream_buffer,
                                   audio->stream_buffer_size);
    }
}

void AudioCore::init() {
    SLEngineItf engine;
    SLEnvironmentalReverbItf outputMixEnvironmentalReverb;
    SLEnvironmentalReverbSettings reverbSettings = SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;

    const SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean interfaceRequired[1] = {SL_BOOLEAN_FALSE};
    // 创建引擎
    slCreateEngine(&engineObj, 0, 0, 0, 0, 0);
    (*engineObj)->Realize(engineObj, SL_BOOLEAN_FALSE);
    (*engineObj)->GetInterface(engineObj, SL_IID_ENGINE, &engine);
    // 创建混音器
    (*engine)->CreateOutputMix(engine, &mixObj, 1, ids, interfaceRequired);
    (*mixObj)->Realize(mixObj, SL_BOOLEAN_FALSE);
    (*mixObj)->GetInterface(mixObj, SL_IID_ENVIRONMENTALREVERB, &outputMixEnvironmentalReverb);
    (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
            outputMixEnvironmentalReverb, &reverbSettings);
    // 创建播放器
    SLDataLocator_AndroidBufferQueue androidBufferQueue = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            2};
    SLuint32 samplesPerSec = codec_ctx->sample_rate * 1000;
    SLDataFormat_PCM formatPcm = {
            SL_DATAFORMAT_PCM,
            2,
            samplesPerSec,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
            SL_BYTEORDER_LITTLEENDIAN
    };
    SLDataSource pAudioSrc = {&androidBufferQueue, &formatPcm};
    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, mixObj};
    SLDataSink pAudioSnk = {&outputMix, NULL};
    const SLInterfaceID pInterfaceIds[3] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME, SL_IID_PLAYBACKRATE};
    const SLboolean pInterfaceRequired[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    (*engine)->CreateAudioPlayer(engine, &pPlayer, &pAudioSrc, &pAudioSnk,
                                 3, pInterfaceIds, pInterfaceRequired);
    (*pPlayer)->Realize(pPlayer, SL_BOOLEAN_FALSE);
    (*pPlayer)->GetInterface(pPlayer, SL_IID_PLAY, &slPlayItf);
    // 设置缓冲
    (*pPlayer)->GetInterface(pPlayer, SL_IID_BUFFERQUEUE, &androidBufferQueueItf);
    (*androidBufferQueueItf)->RegisterCallback(androidBufferQueueItf, bufferQueueCallback, this);
    (*slPlayItf)->SetPlayState(slPlayItf, SL_PLAYSTATE_PLAYING);

    //第六步----------------------------------------
    // 主动调用回调函数开始工作
    bufferQueueCallback(androidBufferQueueItf, this);
}

void AudioCore::pause() {
    if (slPlayItf != NULL) {
        (*slPlayItf)->SetPlayState(slPlayItf, SL_PLAYSTATE_PAUSED);
    }
}

void AudioCore::resume() {
    if (slPlayItf != NULL) {
        (*slPlayItf)->SetPlayState(slPlayItf, SL_PLAYSTATE_PLAYING);
    }
}

void AudioCore::stop() {
    if (slPlayItf != NULL) {
        (*slPlayItf)->SetPlayState(slPlayItf, SL_PLAYSTATE_STOPPED);
    }
}

void AudioCore::release() {
    stop();

    if (pPlayer != NULL) {
        (*pPlayer)->Destroy(pPlayer);
        pPlayer = NULL;
    }

    if (mixObj != NULL) {
        (*mixObj)->Destroy(mixObj);
        mixObj = NULL;
    }

    if (engineObj != NULL) {
        (*engineObj)->Destroy(engineObj);
        engineObj = NULL;
    }

    free(stream_buffer);
    if (swrContext != NULL) {
        swr_free(&swrContext);
        swrContext = NULL;
    }
}
