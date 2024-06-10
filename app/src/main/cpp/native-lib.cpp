#include <jni.h>
#include <string>
#include <android/native_window_jni.h>
#include "FmPlayer.h"

JavaVM *jvm;
ANativeWindow *window;

pthread_mutex_t mutex_t = PTHREAD_MUTEX_INITIALIZER;

jint JNI_OnLoad(JavaVM *vm, void *) {
    ::jvm = vm;
    return JNI_VERSION_1_6;
}

void renderCallback(::uint8_t *src_data, int src_linesize, int w, int h) {
    pthread_mutex_lock(&mutex_t);
    if (!window) {
        pthread_mutex_unlock(&mutex_t);
        return;
    }

    ANativeWindow_setBuffersGeometry(window, w, h, WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer windowBuffer;
    if (ANativeWindow_lock(window, &windowBuffer, 0)) {// 锁住失败
        ANativeWindow_release(window);
        window = nullptr;
        pthread_mutex_unlock(&mutex_t);
        return;
    }

    ::uint8_t *dst_data = static_cast<uint8_t *>(windowBuffer.bits);
    int dst_linesize = windowBuffer.stride * 4;
    for (int i = 0; i < windowBuffer.height; ++i) {
        ::memcpy(dst_data + i * dst_linesize, src_data + i * src_linesize, dst_linesize);
    }

    ANativeWindow_unlockAndPost(window);
    pthread_mutex_unlock(&mutex_t);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_fm_fmplayer_FmPlayer_prepareNative(JNIEnv *env, jobject thiz, jstring data_source) {
    const char *c_data_source = env->GetStringUTFChars(data_source, nullptr);
    JNICallbackHelper *helper = new JNICallbackHelper(jvm, env, thiz);
    auto *player = new FmPlayer(c_data_source, helper, renderCallback);
    // 准备数据源
    player->prepare();
    env->ReleaseStringUTFChars(data_source, c_data_source);
    return reinterpret_cast<jlong>(player);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_fm_fmplayer_FmPlayer_startNative(JNIEnv *env, jobject thiz, jlong native_obj) {
    auto *player = reinterpret_cast<FmPlayer *>(native_obj);
    if (player) {
        player->start();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_fm_fmplayer_FmPlayer_stopNative(JNIEnv *env, jobject thiz, jlong native_obj) {
    auto *player = reinterpret_cast<FmPlayer *>(native_obj);
    if (player) {
        player->stop();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_fm_fmplayer_FmPlayer_releaseNative(JNIEnv *env, jobject thiz, jlong native_obj) {
}

extern "C"
JNIEXPORT void JNICALL
Java_com_fm_fmplayer_FmPlayer_bindSurfaceView(JNIEnv *env, jobject thiz, jlong native_obj,
                                              jobject surface) {

    pthread_mutex_lock(&mutex_t);
    if (window) {
        ANativeWindow_release(window);
        window = nullptr;
    }

    window = ANativeWindow_fromSurface(env, surface);
    pthread_mutex_unlock(&mutex_t);
}


extern "C"
JNIEXPORT jlong JNICALL
Java_com_fm_fmplayer_FmPlayer_durationNative(JNIEnv *env, jobject thiz, jlong native_obj) {
    auto *player = reinterpret_cast<FmPlayer *>(native_obj);
    if (player) {
        return player->getDuration();
    }
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_fm_fmplayer_FmPlayer_seekNative(JNIEnv *env, jobject thiz, jlong native_obj,
                                         jint progress) {
    auto *player = reinterpret_cast<FmPlayer *>(native_obj);
    if (player) {
        LOGI("progress:%d\n", progress)
        player->seek(progress);
    }
}