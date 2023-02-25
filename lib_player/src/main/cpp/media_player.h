//
// Created by admin on 2022/11/25.
//

#ifndef HOLA_MEDIA_PLAYER_H
#define HOLA_MEDIA_PLAYER_H

#include <jni.h>
#include <pthread.h>
#include "jni_player_call.h"
#include "consts.h"
#include "audio_core.h"
#include "packet_queue.h"
#include "frame_core.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
}

/*enum PlayStatus {
    PLAYING, PAUSE, STOP, SEEK
};*/

class MediaPlayer {
public:
    JNIPlayerCall *pPlayerCall = NULL;
    char *url = NULL;
    pthread_t preparedThread;
    AVFormatContext *format_ctx = NULL;
    AudioCore *audio = NULL;
    FrameCore *pVideo = NULL;
    // 跟退出相关，为了防止准备过程中退出，导致异常终止
    pthread_mutex_t releaseMutex;
    PlayStatus *pPlayStatus;

public:
    MediaPlayer();

    ~MediaPlayer();

    void data_source(const char *url);

    void player_call(JNIPlayerCall *jni_player_call);

    void prepare();

    void prepare_async();

    void preparedAudio(ThreadMode mode);

    void start();

    void pause();

    void resume();

    void stop();

    void seek(uint64_t seconds);

    void reset();

    void release();

    void on_error(ThreadMode mode, int errorCode, const char *errorMsg);

    void decodeFrame();
};


#endif //HOLA_MEDIA_PLAYER_H
