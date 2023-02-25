//
// Created by admin on 2022/11/25.
//

#ifndef HOLA_MEDIA_CORE_H
#define HOLA_MEDIA_CORE_H

#include "jni_player_call.h"
#include "play_status.h"
#include "packet_queue.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
};

/**
 * 基类，公共内容
 */
class MediaCore {
public:
    pthread_t playThreadT;
    uint8_t *stream_buffer = NULL;
    int stream_buffer_size = 0;

    /**
     * 当前流的角标（音频/视频/字幕）
     */
    int stream_index;
    /**
     * 解码器上下文
     */
    AVCodecContext *codec_ctx;
    /**
     * 回调 Java 层的 Call
     */
    JNIPlayerCall *jni_player_call;
    /**
     * 播放状态
     */
    PlayStatus *play_status;
    /**
     * AVPacket 队列
     */
    PacketQueue *packet_queue;
    /**
     * 整个视频的时长
     */
    long duration = 0;
    /**
     * 当前播放的时长
     */
    double position = 0;

    /**
     * 上次更新的时间，主要用于控制回调到 Java 层的频率
     */
    double last_update_time = 0;

    /**
     * 时间机
     */
    AVRational rational;

    /**
     * seek 时的 mutex
     */
    pthread_mutex_t seek_mutex;

protected:
    /**
    * 解析公共的解码器上下文
    */
    virtual void prepare(ThreadMode mode, AVFormatContext *pFormatContext);

    virtual void decode() = 0;

    /**
     * 准备解析数据过程中出错的回调
     * @param threadType 线程类型
     * @param errorCode 错误码
     * @param errorMsg 错误信息
     */
    void on_error(ThreadMode mode, int errorCode, const char *errorMsg);

    /**
    * 释放资源
    */
    virtual void release();

public:
    MediaCore(int index, PlayStatus *status, JNIPlayerCall *call);

    ~MediaCore();

    virtual void play() = 0;

    virtual void pause() = 0;

    virtual void resume() = 0;

    virtual void stop() = 0;

    /**
     * seek到当前时间
     * @param seconds 秒
     */
    virtual void seek(uint64_t seconds);
};


#endif //HOLA_MEDIA_CORE_H
