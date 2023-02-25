//
// Created by 曾辉 on 2019-06-05.
//
#include "media_player.h"

MediaPlayer::MediaPlayer() {
    pthread_mutex_init(&releaseMutex, NULL);
    pPlayStatus = new PlayStatus();
    // av_register_all: 作用是初始化所有组件，只有调用了该函数，才能使用复用器和编解码器（源码）
    avformat_network_init();
}

void MediaPlayer::data_source(const char *url) {
    // 复制 url
    this->url = (char *) (malloc(strlen(url) + 1));
    memcpy(this->url, url, strlen(url) + 1);
}

void MediaPlayer::player_call(JNIPlayerCall *jni_player_call) {
    this->pPlayerCall = jni_player_call;
}

void *decodeAudioThread(void *data) {
    auto *fFmpeg = (MediaPlayer *) data;
    fFmpeg->preparedAudio(THREAD_CHILD);
    pthread_exit(0);
}

void MediaPlayer::prepare() {
    pthread_create(&preparedThread, NULL, decodeAudioThread, this);
    //preparedAudio(THREAD_MAIN);
}

void MediaPlayer::prepare_async() {
    pthread_create(&preparedThread, NULL, decodeAudioThread, this);
}

void MediaPlayer::preparedAudio(ThreadMode mode) {
    pthread_mutex_lock(&releaseMutex);

    int format_open_res = avformat_open_input(&format_ctx, url, NULL, NULL);
    if (format_open_res < 0) {
        LOGE("Can't open url : %s, %s", url, av_err2str(format_open_res))
        on_error(mode, format_open_res, av_err2str(format_open_res));
        return;
    }

    int find_stream_info_res = avformat_find_stream_info(format_ctx, NULL);
    if (find_stream_info_res < 0) {
        LOGE("Can't find stream info url : %s, %s", url, av_err2str(find_stream_info_res))
        on_error(mode, find_stream_info_res, av_err2str(find_stream_info_res));
        return;
    }

    // 获取视频流对应的索引
    int audio_stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO,
                                                 -1, -1, NULL, 0);
    int frame_stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO,
                                                 -1, -1, NULL, 0);
    if (audio_stream_index >= 0) {
        audio = new AudioCore(audio_stream_index, pPlayStatus, pPlayerCall);
        audio->prepare(mode, format_ctx);
    } else {
        LOGW("Can't find audio stream info url : %s", url);
    }
    if (frame_stream_index >= 0) {
        //pVideo = new FrameCore(frame_stream_index, pPlayStatus, pPlayerCall, audio);
        //pVideo->prepare(mode, format_ctx);
    } else {
        LOGW("Can't find video stream info url : %s", url);
    }

    if (frame_stream_index < 0 && audio_stream_index < 0) {
        LOGE("Can't find audio and video stream info url : %s", url);
        on_error(mode, FIND_VIDEO_STREAM_ERROR_CODE, "Can't find video stream info url.");
        return;
    }


    pPlayerCall->onCallPrepared(mode);
    pthread_mutex_unlock(&releaseMutex);
}

MediaPlayer::~MediaPlayer() {
    avformat_network_deinit();
    pPlayerCall = NULL;
}

void *read_frame(void *data) {
    auto *player = (MediaPlayer *) (data);
    while (player->pPlayStatus != NULL && !player->pPlayStatus->isExit) {
        AVPacket *packet = av_packet_alloc();
        // 提取每一帧的音频流
        if (av_read_frame(player->format_ctx, packet) >= 0) {
            // 必须要是音频流
            if ((player->audio != NULL) && (player->audio->stream_index == packet->stream_index)) {
                player->audio->packet_queue->push(packet);
            } else if ((player->pVideo != NULL) && (player->pVideo->stream_index == packet->stream_index)) {
                player->pVideo->packet_queue->push(packet);
            } else {
                av_packet_free(&packet);
            }
        } else {
            av_packet_free(&packet);
        }
    }
    pthread_exit((void *) 1);
}

void MediaPlayer::start() {
    this->decodeFrame();
    if (audio != NULL) {
        audio->play();
    }
    if (pVideo != NULL) {
        pVideo->play();
    }
}

void MediaPlayer::pause() {
    if ((audio == NULL) || (pPlayStatus == NULL)) return;
    audio->pause();
    pPlayStatus->isPause = true;
}

void MediaPlayer::resume() {
    if ((audio == NULL) || (pPlayStatus == NULL)) return;
    pPlayStatus->isPause = false;
    audio->resume();
}

void MediaPlayer::stop() {
    if ((audio == NULL) || (pPlayStatus == NULL)) return;
    audio->stop();
    pPlayStatus->isExit = true;
}

void MediaPlayer::reset(){

}

void MediaPlayer::release() {
    pthread_mutex_lock(&releaseMutex);
    if (audio->play_status->isExit) {
        return;
    }

    audio->play_status->isExit = true;

    if (audio != NULL) {
        audio->release();
        delete audio;
        audio = NULL;
    }

    if (pVideo != NULL) {
        pVideo->release();
        delete pVideo;
        pVideo = NULL;
    }

    if (format_ctx != NULL) {
        avformat_close_input(&format_ctx);
        avformat_free_context(format_ctx);
        format_ctx = NULL;
    }

    free(url);

    pthread_mutex_unlock(&releaseMutex);
    pthread_mutex_destroy(&releaseMutex);
}

void MediaPlayer::on_error(ThreadMode mode, int errorCode, const char *errorMsg) {
    if (pPlayerCall != NULL) {
        pPlayerCall->onCallError(mode, errorCode, errorMsg);
    }
    if (format_ctx != NULL) {
        avformat_close_input(&format_ctx);
        avformat_free_context(format_ctx);
        format_ctx = NULL;
    }
    free(url);
    pthread_mutex_unlock(&releaseMutex);
    pthread_mutex_destroy(&releaseMutex);
}

void MediaPlayer::seek(uint64_t seconds) {
    if (pPlayStatus != NULL) {
        pPlayStatus->isSeek = true;
    }

    if (seconds >= 0) {
        int64_t rel = seconds * AV_TIME_BASE;
        av_seek_frame(format_ctx, -1, rel, AVSEEK_FLAG_BACKWARD);
    }

    if (pVideo != NULL) {
        pVideo->seek(seconds);
    }

    if (audio != NULL) {
        audio->seek(seconds);
    }

    if (pPlayStatus != NULL) {
        pPlayStatus->isSeek = false;
    }
}

void MediaPlayer::decodeFrame() {
    pthread_t decodeFrameThreadT;
    pthread_create(&decodeFrameThreadT, NULL, read_frame, this);
}

