//
// Created by Administrator on 2022/11/22.
//

#ifndef HOLA_CONSTS_H
#define HOLA_CONSTS_H

#include <android/log.h>

#define TAG "MediaPlayer"
#define LOGE(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR, TAG, FORMAT, ##__VA_ARGS__);
#define LOGW(FORMAT, ...) __android_log_print(ANDROID_LOG_WARN, TAG, FORMAT, ##__VA_ARGS__);

#define FIND_AUDIO_STREAM_ERROR_CODE -0x011
#define FIND_AUDIO_DECODER_ERROR_CODE -0x012
#define FIND_VIDEO_STREAM_ERROR_CODE -0x013
#define SWS_GET_CONTEXT_ERROR_CODE -0x014

#endif //HOLA_CONSTS_H
