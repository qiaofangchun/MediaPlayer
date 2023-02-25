//
// Created by 曾辉 on 2019-06-05.
//

#include <jni.h>
#include "consts.h"

#ifndef HOLA_JNI_PLAYER_CALL_H
#define HOLA_JNI_PLAYER_CALL_H

enum ThreadMode {
    THREAD_MAIN, THREAD_CHILD
};

class JNIPlayerCall{
public:
    JavaVM *java_vm = NULL;
    JNIEnv *jni_env = NULL;
    jobject player_obj;
    jmethodID prepared_mid;
    jmethodID loadingMid;
    jmethodID progressMid;
    jmethodID errorMid;
    jmethodID completeMid;
    jmethodID renderMid;
    jmethodID isSupportStiffCodecMid;
    jmethodID initMediaCodecMid;
    jmethodID decodePacketMid;
public:
    JNIPlayerCall(JavaVM *java_vm, JNIEnv *jni_env, jobject player_obj);

    ~JNIPlayerCall();

    void onCallPrepared(ThreadMode mode);

    void onCallLoading(ThreadMode mode, bool loading);

    void onCallProgress(ThreadMode mode, int current, int total);

    void onCallError(ThreadMode mode, int errorCode, const char *errorMsg);

    void onCallComplete(ThreadMode mode);

    void onCallRenderYUV420P(int width, int height, uint8_t *fy, uint8_t *fu, uint8_t *fv);

    bool onCallIsSupportStiffCodec(ThreadMode mode, const char *codecName);

    void onCallInitMediaCodec(ThreadMode mode, const char *mime, int width, int height,
            int csd0Size, int csd1Size, uint8_t *csd0, uint8_t *csd1);

    void onCallDecodePacket(int i, uint8_t *string);
};


#endif //HOLA_JNI_PLAYER_CALL_H
