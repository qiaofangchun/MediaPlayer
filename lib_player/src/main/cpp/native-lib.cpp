//
// Created by Administrator on 2022/11/25.
//

#include <jni.h>
#include <string>
#include <android/native_window_jni.h>
#include <zconf.h>
#include "consts.h"
#include "media_player.h"

extern "C" {
#include "libavcodec/codec.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "unistd.h"
}

JNIPlayerCall *jni_call;
MediaPlayer *player;
JavaVM *jvm = NULL;

// 重写 so 被加载时会调用的一个方法
// 小作业，去了解动态注册
extern "C" JNIEXPORT
jint JNICALL JNI_OnLoad(JavaVM *javaVM, void *reserved) {
    jvm = javaVM;
    JNIEnv *env;
    if (javaVM->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }
    return JNI_VERSION_1_6;
}

// 初始化
extern "C"
JNIEXPORT void JNICALL
Java_com_hola_media_player_MusicPlayer_native_1init(JNIEnv *env, jobject thiz) {
    if (player == NULL) {
        player = new MediaPlayer();
        jni_call = new JNIPlayerCall(jvm, env, thiz);
        player->player_call(jni_call);
    }
}
// 释放
extern "C"
JNIEXPORT void JNICALL
Java_com_hola_media_player_MusicPlayer_native_1deinit(JNIEnv *env, jobject thiz) {

}
// 设置文件路径
extern "C"
JNIEXPORT void JNICALL
Java_com_hola_media_player_MusicPlayer_native_1data_1source(JNIEnv *env, jobject thiz, jstring path) {
    if (player == NULL) return;
    const char *url = env->GetStringUTFChars(path, 0);
    player->data_source(url);
    env->ReleaseStringUTFChars(path, url);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_hola_media_player_MusicPlayer_native_1prepare(JNIEnv *env, jobject thiz) {
    if (player == NULL) return;
    player->prepare();
}
// 播放
extern "C"
JNIEXPORT void JNICALL
Java_com_hola_media_player_MusicPlayer_native_1start(JNIEnv *env, jobject thiz) {
    if (player == NULL) return;
    player->start();
}
// 暂停
extern "C"
JNIEXPORT void JNICALL
Java_com_hola_media_player_MusicPlayer_native_1pause(JNIEnv *env, jobject thiz) {
    if (player == NULL) return;
    player->pause();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_hola_media_player_MusicPlayer_native_1resume(JNIEnv *env, jobject thiz) {
    if (player == NULL) return;
    player->resume();
}

// 停止
extern "C"
JNIEXPORT void JNICALL
Java_com_hola_media_player_MusicPlayer_native_1stop(JNIEnv *env, jobject thiz) {
    if (player == NULL) return;
    player->stop();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_hola_media_player_MusicPlayer_native_1seek(JNIEnv *env, jobject thiz, jlong msec) {
    if (player == NULL) return;
    player->seek(msec);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_hola_media_player_MusicPlayer_native_1reset(JNIEnv *env, jobject thiz) {
    if (player == NULL) return;
    player->reset();
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_hola_media_player_MusicPlayer_native_1is_1playing(JNIEnv *env, jobject thiz) {
    if (player == NULL) return false;
    return !player->pPlayStatus->isPause;
}