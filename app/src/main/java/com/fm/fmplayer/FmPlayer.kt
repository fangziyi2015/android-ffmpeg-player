package com.fm.fmplayer

import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent

class FmPlayer : SurfaceHolder.Callback, LifecycleObserver {

    companion object {
        init {
            System.loadLibrary("fmplayer")
        }

        var nativeObj: Long = 0L
        private const val TAG = "FmPlayer"
    }

    var prepareCallback: (() -> Unit)? = null
    var errorCallback: ((String) -> Unit)? = null

    var dataSource: String = ""

    private var surfaceHolder: SurfaceHolder? = null

    fun setSurfaceView(surfaceView: SurfaceView) {
        surfaceHolder?.let {
            it.removeCallback(this)
        }

        surfaceHolder = surfaceView.holder
        surfaceHolder?.addCallback(this)
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_RESUME)
    fun prepare() {
        Log.i(TAG, "prepare dataSource:$dataSource")
        nativeObj = prepareNative(dataSource)
    }

    fun start() {
        startNative(nativeObj)
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_PAUSE)
    fun stop() {
        Log.i(TAG, "stop: ")
        stopNative(nativeObj)
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_DESTROY)
    fun release() {
        Log.i(TAG, "release: ")
        releaseNative(nativeObj)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        bindSurfaceView(nativeObj, holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
    }


    /**
     * 准备完成回调
     */
    fun onPrepareJNICallback() {
        prepareCallback?.invoke()
    }

    /**
     * 出错时回调
     */
    fun onErrorJNICallback(error: String) {
        errorCallback?.invoke(error)
    }

    private external fun prepareNative(dataSource: String): Long
    private external fun startNative(nativeObj: Long)
    private external fun stopNative(nativeObj: Long)
    private external fun releaseNative(nativeObj: Long)
    private external fun bindSurfaceView(nativeObj: Long, surface: Surface)

}