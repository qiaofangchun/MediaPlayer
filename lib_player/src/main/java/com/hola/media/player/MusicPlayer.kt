package com.hola.media.player

import android.media.MediaPlayer

class MusicPlayer {

    private var url: String = ""
    private var isStop = true

    // called from jni
    private fun onError(code: Int, msg: String) {

    }

    // called from jni
    private fun onPrepared() {

    }

    // Called from jni
    private fun onLoading(loading: Boolean) {
    }

    // Called from jni
    private fun onProgress(current: Int, total: Int) {
    }

    // Called from jni
    private fun onComplete() {
    }

    // Called from jni
    private fun onRenderYUV420P(width: Int, height: Int, y: ByteArray, u: ByteArray, v: ByteArray) {
    }

    // Called from jni
    private fun decodePacket(dataSize: Int, data: ByteArray) {

    }

    private fun isSupportStiffCodec(codeName: String): Boolean {
        return false
    }

    private fun initMediaCodec(
        codecName: String,
        width: Int,
        height: Int,
        csd0: ByteArray,
        csd1: ByteArray
    ) {

    }

    fun setDataSource(path: String) {
        this.url = path
        native_init()
        native_data_source(path)
        val aa = MediaPlayer()
        aa.reset();
    }

    fun prepare() {
        native_prepare()
    }

    fun play(){
        if (isStop) {
            native_start()
            isStop = false
        } else {
            if (!native_is_playing()) {
                native_resume()
            }
        }
    }

    fun pause() = native_pause()

    fun stop() = native_stop()

    fun seekTo(msec: Long) = native_seek(msec)

    fun reset() = native_reset()

    fun release() = native_deinit()

    private external fun native_init()

    private external fun native_data_source(path: String)

    private external fun native_prepare()

    private external fun native_start()

    private external fun native_stop()

    private external fun native_pause()

    private external fun native_resume()

    private external fun native_seek(msec: Long)

    private external fun native_reset()

    private external fun native_is_playing(): Boolean

    private external fun native_deinit()

    private companion object {
        private const val TAG = "MusicPlayer"

        init {
            System.loadLibrary("MediaPlayer")
        }
    }
}