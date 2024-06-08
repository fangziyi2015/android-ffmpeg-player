#include "JNICallbackHelper.h"

void JNICallbackHelper::error(char *msg) {
    JNIEnv *env;
    vm->AttachCurrentThread(&env, nullptr);
    jstring error_msg = env->NewStringUTF(msg);
    env->CallVoidMethod(job, errorMethodId, error_msg);
    vm->DetachCurrentThread();
}

void JNICallbackHelper::prepare() {
    JNIEnv *env;
    vm->AttachCurrentThread(&env, nullptr);
    env->CallVoidMethod(job, prepareMethodId);
    vm->DetachCurrentThread();
}

JNICallbackHelper::JNICallbackHelper(JavaVM *vm, JNIEnv *env, jobject job) : vm(vm) {
    this->job = env->NewGlobalRef(job);
    jclass cls = env->GetObjectClass(this->job);
    errorMethodId = env->GetMethodID(cls, "onErrorJNICallback", "(Ljava/lang/String;)V");
    prepareMethodId = env->GetMethodID(cls, "onPrepareJNICallback", "()V");
}
