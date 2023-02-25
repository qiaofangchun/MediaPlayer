//
// Created by hcDarren on 2019/6/23.
//

#include "media_core.h"
#include "consts.h"

MediaCore::MediaCore(int stream_index, PlayStatus *play_status, JNIPlayerCall *jni_player_call) {
    this->stream_index = stream_index;
    this->play_status = play_status;
    this->jni_player_call = jni_player_call;
    this->packet_queue = new PacketQueue(play_status);
    pthread_mutex_init(&seek_mutex, NULL);
}

MediaCore::~MediaCore() {
    release();
}

void MediaCore::prepare(ThreadMode mode, AVFormatContext *format_ctx) {
    AVCodecParameters *codec_ptr = format_ctx->streams[stream_index]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codec_ptr->codec_id);
    if (!codec) {
        LOGE("Can't find audio decoder");
        on_error(mode, FIND_AUDIO_DECODER_ERROR_CODE, "Can't find audio decoder.");
        return;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    int ptr_copy_res = avcodec_parameters_to_context(codec_ctx, codec_ptr);
    if (ptr_copy_res < 0) {
        LOGE("codec parameters to_context error :%s", av_err2str(ptr_copy_res));
        on_error(mode, ptr_copy_res, av_err2str(ptr_copy_res));
        return;
    }

    int codec_open_res = avcodec_open2(codec_ctx, codec, NULL);
    if (codec_open_res < 0) {
        LOGE("codec open error : %s", av_err2str(codec_open_res));
        on_error(mode, codec_open_res, av_err2str(codec_open_res));
        return;
    }

    duration = format_ctx->duration / AV_TIME_BASE;
    rational = format_ctx->streams[stream_index]->time_base;
}


void MediaCore::on_error(ThreadMode mode, int errorCode, const char *errorMsg) {
    release();
    if (jni_player_call != NULL) {
        jni_player_call->onCallError(mode, errorCode, errorMsg);
    }
}

void MediaCore::release() {
    if (codec_ctx != NULL) {
        avcodec_close(codec_ctx);
        avcodec_free_context(&codec_ctx);
        codec_ctx = NULL;
    }

    if (packet_queue != NULL) {
        delete (packet_queue);
        packet_queue = NULL;
    }

    pthread_mutex_destroy(&seek_mutex);
}

void MediaCore::seek(uint64_t seconds) {
    if (duration <= 0) {
        return;
    }

    pthread_mutex_lock(&seek_mutex);

    if (seconds >= 0 && seconds < duration) {
        packet_queue->clear();
        last_update_time = 0;
        position = 0;
        avcodec_flush_buffers(codec_ctx);
    }

    pthread_mutex_unlock(&seek_mutex);
}
