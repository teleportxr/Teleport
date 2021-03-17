// (C) Copyright 2018-2021 Simul Software Ltd

#include "Android_MemoryUtil.h"	

Android_MemoryUtil::JNI Android_MemoryUtil::jni;
bool Android_MemoryUtil::mJNIInitialized=false;

Android_MemoryUtil::Android_MemoryUtil(JNIEnv* env)
    : mEnv(env)
{
    assert(mEnv);
    jobject memoryUtil = mEnv->NewObject(jni.memoryUtilClass, jni.ctorMethod);
    mMemoryUtilKt = mEnv->NewGlobalRef(memoryUtil);
}

Android_MemoryUtil::~Android_MemoryUtil()
{
    if (mMemoryUtilKt)
    {
        mEnv->DeleteGlobalRef(mMemoryUtilKt);
    }
}

long Android_MemoryUtil::getAvailableMemory() const
{
    return mEnv->CallLongMethod(mMemoryUtilKt, jni.getAvailMemMethod);
}

long Android_MemoryUtil::getTotalMemory() const
{
    return mEnv->CallLongMethod(mMemoryUtilKt, jni.getAvailMemMethod);
}

void Android_MemoryUtil::printMemoryStats() const
{
    mEnv->CallVoidMethod(mMemoryUtilKt, jni.printMemStatsMethod);
}

void Android_MemoryUtil::InitializeJNI(JNIEnv* env)
{
    assert(env);

    jclass memoryUtilClass = env->FindClass("co/simul/teleportvrquestclient/MemoryUtil");
    assert(memoryUtilClass);
    jni.memoryUtilClass = (jclass)env->NewGlobalRef((jobject)memoryUtilClass);

    jni.ctorMethod = env->GetMethodID(jni.memoryUtilClass, "<init>", "()V");
    jni.getAvailMemMethod = env->GetMethodID(jni.memoryUtilClass, "getAvailableMemory", "()J");
    jni.getTotalMemMethod = env->GetMethodID(jni.memoryUtilClass, "getTotalMemory", "()J");
    jni.printMemStatsMethod = env->GetMethodID(jni.memoryUtilClass, "printMemoryStats", "()V");
    mJNIInitialized=true;
}

