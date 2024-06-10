#ifndef FMPLAYER_JNICALLBACKHELPER_H
#define FMPLAYER_JNICALLBACKHELPER_H

#include "jni.h"

class JNICallbackHelper {
private:
    JavaVM *vm;
    jobject job;

    jmethodID errorMethodId;
    jmethodID prepareMethodId;
    jmethodID progressMethodId;

public:
    JNICallbackHelper(JavaVM *vm, JNIEnv *env, jobject job);

    void error(char *msg);

    void prepare();

    void onProgress(long progress);
};


#endif //FMPLAYER_JNICALLBACKHELPER_H
