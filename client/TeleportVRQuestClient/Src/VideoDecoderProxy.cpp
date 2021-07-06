// (C) Copyright 2018-2019 Simul Software Ltd

#include "VideoDecoderProxy.h"
#include "VideoSurface.h"

extern "C" {

JNIEXPORT void Java_co_simul_teleportvrquestclient_VideoDecoder_nativeFrameAvailable(JNIEnv* env, jclass clazz, jlong proxyPtr)
{
    VideoDecoderProxy* proxy = reinterpret_cast<VideoDecoderProxy*>(proxyPtr);
    assert(proxy);
    proxy->NotifyFrameAvailable();
}

} // extern "C"

VideoDecoderProxy::JNI VideoDecoderProxy::jni;
bool VideoDecoderProxy::mJNIInitialized=false;

VideoDecoderProxy::VideoDecoderProxy(JNIEnv *env, DecodeEventInterface* eventInterface)
    : mFrameWidth(0), mFrameHeight(0)
    , mUseAlphaLayerDecoding(false)
    , mInitialized(false)
    , mColorSurfaceTexture(nullptr)
    , mAlphaSurfaceTexture(nullptr)
    , mEventInterface(eventInterface)
    , mEnv(env)
{

}

VideoDecoderProxy::~VideoDecoderProxy()
{
    if(mVideoDecoder) {
        mEnv->DeleteGlobalRef(mVideoDecoder);
    }
}

avs::Result VideoDecoderProxy::initialize(const avs::DeviceHandle& device, int frameWidth, int frameHeight, const avs::DecoderParams& params)
{
    if(mInitialized) {
        return avs::Result::OK;
    }

    mFrameWidth = frameWidth;
    mFrameHeight = frameHeight;

    mUseAlphaLayerDecoding = params.useAlphaLayerDecoding;

    assert(mEnv);
    jobject videoDecoder = mEnv->NewObject(jni.videoDecoderClass, jni.ctorMethod, this, static_cast<int>(params.codec), mUseAlphaLayerDecoding);
    mVideoDecoder = mEnv->NewGlobalRef(videoDecoder);

    mInitialized = true;

    return avs::Result::OK;
}

avs::Result VideoDecoderProxy::reconfigure(int frameWidth, int frameHeight, const avs::DecoderParams& params)
{
    if(!mInitialized) {
        return avs::Result::DecoderBackend_NotInitialized;
    }

    mInitialized = false;

    avs::DeviceHandle dummyHandle;

    return initialize(dummyHandle, frameWidth, frameHeight, params);
}

avs::Result VideoDecoderProxy::shutdown()
{
    if(!mInitialized) {
        return avs::Result::DecoderBackend_NotInitialized;
    }
    if(mColorSurfaceTexture) {
        ShutdownVideoDecoder();
    }

    mFrameWidth  = 0;
    mFrameHeight = 0;
    mInitialized = false;
    return avs::Result::OK;
}

avs::Result VideoDecoderProxy::registerSurface(const avs::SurfaceBackendInterface* colorSurface, const avs::SurfaceBackendInterface* alphaSurface)
{
    if(!mInitialized) {
        return avs::Result::DecoderBackend_NotInitialized;
    }
    if(mColorSurfaceTexture) {
        return avs::Result::DecoderBackend_SurfaceAlreadyRegistered;
    }

    const VideoSurface* videoColorSurface = dynamic_cast<const VideoSurface*>(colorSurface);
    if(!videoColorSurface) {
        return avs::Result::DecoderBackend_InvalidSurface;
    }

    OVRFW::SurfaceTexture* ovrAlphaSurface = nullptr;
    if (mUseAlphaLayerDecoding)
    {
        const VideoSurface* videoAlphaSurface = dynamic_cast<const VideoSurface*>(alphaSurface);
        if(!videoAlphaSurface) {
            return avs::Result::DecoderBackend_InvalidSurface;
        }
        ovrAlphaSurface = videoAlphaSurface->GetTexture();
    }

    InitializeVideoDecoder(videoColorSurface->GetTexture(), ovrAlphaSurface);
    return avs::Result::OK;
}

avs::Result VideoDecoderProxy::unregisterSurface()
{
    if(!mColorSurfaceTexture) {
        return avs::Result::DecoderBackend_SurfaceNotRegistered;
    }

    ShutdownVideoDecoder();
    return avs::Result::OK;
}

avs::Result VideoDecoderProxy::decode(const void* buffer, size_t bufferSizeInBytes, avs::VideoPayloadType payloadType, bool lastPayload)
{
    if(!mInitialized) {
        return avs::Result::DecoderBackend_NotInitialized;
    }
    if(!mColorSurfaceTexture) {
        return avs::Result::DecoderBackend_InvalidSurface;
    }

    jobject jbuffer = mEnv->NewDirectByteBuffer(const_cast<void*>(buffer), bufferSizeInBytes);
    jboolean isReadyToDisplay = mEnv->CallBooleanMethod(mVideoDecoder, jni.decodeMethod, jbuffer, payloadType, lastPayload);
    mEnv->DeleteLocalRef(jbuffer);
    return isReadyToDisplay ? avs::Result::DecoderBackend_ReadyToDisplay : avs::Result::OK;
}

avs::Result VideoDecoderProxy::display(bool showAlphaAsColor)
{
    if(!mInitialized) {
        return avs::Result::DecoderBackend_NotInitialized;
    }
    if(!mColorSurfaceTexture) {
        return avs::Result::DecoderBackend_InvalidSurface;
    }
    jboolean displayResult = mEnv->CallBooleanMethod(mVideoDecoder, jni.displayMethod);
    // Switched around. true return means OK!!
    return displayResult ?  avs::Result::OK : avs::Result::DecoderBackend_DisplayFailed ;
}

void VideoDecoderProxy::NotifyFrameAvailable()
{
    if(mEventInterface) {
        mEventInterface->OnFrameAvailable();
    }
}

void VideoDecoderProxy::InitializeJNI(JNIEnv* env)
{
    assert(env);

    jclass videoDecoderClass = env->FindClass("co/simul/teleportvrquestclient/VideoDecoder");
	assert(videoDecoderClass);
    jni.videoDecoderClass = (jclass)env->NewGlobalRef((jobject)videoDecoderClass);

    jni.ctorMethod = env->GetMethodID(jni.videoDecoderClass, "<init>", "(JIZ)V");
    jni.initializeMethod = env->GetMethodID(jni.videoDecoderClass, "initialize", "(Landroid/graphics/SurfaceTexture;Landroid/graphics/SurfaceTexture;II)V");
    jni.shutdownMethod = env->GetMethodID(jni.videoDecoderClass, "shutdown", "()V");
    jni.decodeMethod = env->GetMethodID(jni.videoDecoderClass, "decode", "(Ljava/nio/ByteBuffer;IZ)Z");
    jni.displayMethod = env->GetMethodID(jni.videoDecoderClass, "display", "()Z");
    mJNIInitialized=true;
}

void VideoDecoderProxy::InitializeVideoDecoder(OVRFW::SurfaceTexture* colorSurfaceTexture, OVRFW::SurfaceTexture* alphaSurfaceTexture)
{
    assert(mFrameWidth > 0 && mFrameHeight > 0);
    assert(surfaceTexture);

    mColorSurfaceTexture = colorSurfaceTexture;
    mAlphaSurfaceTexture = alphaSurfaceTexture;

    jobject alphaTextureJObject = nullptr;

    if (mAlphaSurfaceTexture)
    {
        alphaTextureJObject = mAlphaSurfaceTexture->GetJavaObject();
    }

    mEnv->CallVoidMethod(mVideoDecoder, jni.initializeMethod, mColorSurfaceTexture->GetJavaObject(), alphaTextureJObject, mFrameWidth, mFrameHeight);
}

void VideoDecoderProxy::ShutdownVideoDecoder()
{
    mEnv->CallVoidMethod(mVideoDecoder, jni.shutdownMethod);
    mColorSurfaceTexture = nullptr;
    mAlphaSurfaceTexture = nullptr;
}

