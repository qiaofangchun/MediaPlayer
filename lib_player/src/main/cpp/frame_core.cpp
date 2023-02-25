//
// Created by hcDarren on 2019/6/23.
//
#include "frame_core.h"

FrameCore::FrameCore(int videoStreamIndex, PlayStatus *pPlayStatus, JNIPlayerCall *pPlayerCall,
                       AudioCore *pAudio) : MediaCore(videoStreamIndex, pPlayStatus, pPlayerCall) {
    this->pAudio = pAudio;
}

FrameCore::~FrameCore() {
    release();
}

void FrameCore::decode() {

}

void *threadPlay(void *context) {
    FrameCore *pVideo = (FrameCore *) context;
    AVPacket *pPacket = av_packet_alloc();
    AVFrame *pFrame = av_frame_alloc();
    while (pVideo->play_status != NULL && !pVideo->play_status->isExit) {
        // 是不是暂停或者 seek 中
        if (pVideo->play_status != NULL) {
            if (pVideo->play_status->isPause || pVideo->play_status->isSeek) {
                av_usleep(10 * 1000);
                continue;
            }
        }
        // 加锁，防止 seek 时获取到脏数据
        pthread_mutex_lock(&pVideo->seek_mutex);
        pVideo->packet_queue->pop(pPacket);
        // 是否支持硬解码
        if (pVideo->supportStiffCodec) {
            int bsfSendPacketRes = av_bsf_send_packet(pVideo->pBSFContext, pPacket);
            pthread_mutex_unlock(&pVideo->seek_mutex);
            if (bsfSendPacketRes == 0) {
                while (av_bsf_receive_packet(pVideo->pBSFContext, pPacket) == 0) {
                    double sleepTime = pVideo->getFrameSleepTime(pPacket->pts);
                    av_usleep(sleepTime * 1000000);
                    pVideo->jni_player_call->onCallDecodePacket(pPacket->size, pPacket->data);
                    av_packet_unref(pPacket);
                }
            }
        } else {
            // 解码 avcodec_send_packet -> avcodec_receive_frame
            int send_packet_res = avcodec_send_packet(pVideo->codec_ctx, pPacket);
            if (send_packet_res == 0) {
                int receive_frame_res = avcodec_receive_frame(pVideo->codec_ctx, pFrame);
                pthread_mutex_unlock(&pVideo->seek_mutex);
                if (receive_frame_res == 0) {
                    double sleepTime = pVideo->getFrameSleepTime(pFrame->pts);
                    av_usleep(sleepTime * 1000000);

                    if (pVideo->codec_ctx->pix_fmt == AVPixelFormat::AV_PIX_FMT_YUV420P) {
                        // 不需要转可以直用 OpenGLES 去渲染
                        pVideo->jni_player_call->onCallRenderYUV420P(pVideo->codec_ctx->width,
                                pVideo->codec_ctx->height,
                                pFrame->data[0],
                                pFrame->data[1],
                                pFrame->data[2]);
                    } else {
                        // 需要转换
                        sws_scale(pVideo->pSwsContext, pFrame->data, pFrame->linesize, 0,
                                pFrame->height,
                                pVideo->pFrameYUV420P->data,
                                pVideo->pFrameYUV420P->linesize);
                        // OpenGLES 去渲染
                        pVideo->jni_player_call->onCallRenderYUV420P(pVideo->codec_ctx->width,
                                pVideo->codec_ctx->height,
                                pVideo->pFrameYUV420P->data[0],
                                pVideo->pFrameYUV420P->data[1],
                                pVideo->pFrameYUV420P->data[2]);
                    }
                }
            } else {
                pthread_mutex_unlock(&pVideo->seek_mutex);
            }
            av_frame_unref(pFrame);
        }

        // 释放 data 数据，释放 AVPacket 开辟的内存
        av_packet_unref(pPacket);
    }
    av_packet_free(&pPacket);
    av_frame_free(&pFrame);
    return 0;
}

void FrameCore::play() {
    pthread_create(&playThreadT, NULL, threadPlay, this);
}

void FrameCore::pause() {
}

void FrameCore::stop() {
}

void FrameCore::resume() {
}

void FrameCore::release() {
    MediaCore::release();

    if (pFrameYUV420P != NULL) {
        av_frame_free(&pFrameYUV420P);
        pFrameYUV420P = NULL;
    }

    if (stream_buffer != NULL) {
        free(stream_buffer);
        stream_buffer = NULL;
    }

    if (pSwsContext != NULL) {
        sws_freeContext(pSwsContext);
        av_free(pSwsContext);
        pSwsContext = NULL;
    }

    if (pBSFContext != NULL) {
        av_bsf_free(&pBSFContext);
        av_free(pBSFContext);
        pBSFContext = NULL;
    }
}

void FrameCore::prepare(ThreadMode mode, AVFormatContext *pFormatContext) {
    MediaCore::prepare(mode, pFormatContext);

    pFrameYUV420P = av_frame_alloc();
    stream_buffer_size = av_image_get_buffer_size(AVPixelFormat::AV_PIX_FMT_YUV420P,
            codec_ctx->width,
            codec_ctx->height, 1);
    stream_buffer = (uint8_t *)malloc(stream_buffer_size);
    av_image_fill_arrays(pFrameYUV420P->data, pFrameYUV420P->linesize, stream_buffer,
            AV_PIX_FMT_YUV420P, codec_ctx->width, codec_ctx->height, 1);

    pSwsContext = sws_getContext(codec_ctx->width, codec_ctx->height,
                                 codec_ctx->pix_fmt, codec_ctx->width,
                                 codec_ctx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL,
            NULL, NULL);
    if (pSwsContext == NULL) {
        on_error(mode, SWS_GET_CONTEXT_ERROR_CODE, "sws get context error.");
    }

    int num = pFormatContext->streams[stream_index]->avg_frame_rate.num;
    int den = pFormatContext->streams[stream_index]->avg_frame_rate.den;
    if (num != 0 && den != 0) {
        defaultDelayTime = 1.0 * den / num;
    }

    const char *codecName = codec_ctx->codec->name;
    supportStiffCodec = jni_player_call->onCallIsSupportStiffCodec(mode, codecName);
    if (supportStiffCodec) {
        // 如果支持硬解码
        const AVBitStreamFilter *pBSFilter = NULL;
        if (strcasecmp("h264", codecName) == 0) {
            pBSFilter = av_bsf_get_by_name("h264_mp4toannexb");
        } else if (strcasecmp("h265", codecName) == 0) {
            pBSFilter = av_bsf_get_by_name("hevc_mp4toannexb");
        }

        if (pBSFilter == NULL) {
            supportStiffCodec = false;
            return;
        }

        int bsfAllocRes = av_bsf_alloc(pBSFilter, &pBSFContext);
        if (bsfAllocRes != 0) {
            supportStiffCodec = false;
            return;
        }

        AVCodecParameters *pCodecParameters = pFormatContext->streams[stream_index]->codecpar;
        int codecParametersCopyRes = avcodec_parameters_copy(pBSFContext->par_in, pCodecParameters);
        if (codecParametersCopyRes < 0) {
            supportStiffCodec = false;
            return;
        }

        int bsfInitRes = av_bsf_init(pBSFContext);
        if (bsfInitRes != 0) {
            supportStiffCodec = false;
            return;
        }

        // 调用 java 层初始化 MediaCodec
        jni_player_call->onCallInitMediaCodec(mode, codecName, codec_ctx->width,
                                              codec_ctx->height, codec_ctx->extradata_size, codec_ctx->extradata_size,
                                              codec_ctx->extradata, codec_ctx->extradata);

        pBSFContext->time_base_in = rational;
    }
}

/**
 * 获取当前视频帧应该休眠的时间
 * @param pFrame 当前视频帧
 * @return 睡眠时间
 */
double FrameCore::getFrameSleepTime(int64_t pts) {
    // 如果 < 0 那么还是用之前的时间
    double times = pts * av_q2d(rational);
    if (times > position) {
        position = times;
    }
    // 音频和视频之间的差值
    double diffTime = pAudio->position - position;

    // 第一次控制，0.016s ～ -0.016s
    if (diffTime > 0.016 || diffTime < -0.016) {
        if (diffTime > 0.016) {
            delayTime = delayTime * 2 / 3;
        } else if (diffTime < -0.016) {
            delayTime = delayTime * 3 / 2;
        }

        // 第二次控制，defaultDelayTime * 2 / 3 ～ defaultDelayTime * 3 / 2
        if (delayTime < defaultDelayTime / 2) {
            delayTime = defaultDelayTime * 2 / 3;
        } else if (delayTime > defaultDelayTime * 2) {
            delayTime = defaultDelayTime * 3 / 2;
        }
    }

    // 第三次控制，0～defaultDelayTime * 2
    if (diffTime >= 0.25) {
        delayTime = 0;
    } else if (diffTime <= -0.25) {
        delayTime = defaultDelayTime * 2;
    }

    // 假设1秒钟25帧，不出意外情况，delayTime 是 0.2 , 0.4 , 0.6
    return delayTime;
}
