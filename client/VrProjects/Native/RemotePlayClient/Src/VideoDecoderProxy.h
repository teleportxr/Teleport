// (C) Copyright 2018 Simul.co

#pragma once

#include <libavstream/decoders/dec_interface.hpp>
#include <SurfaceTexture.h>

class DecodeEventInterface
{
public:
    virtual void OnFrameAvailable() = 0;
};

class VideoDecoderProxy final : public avs::DecoderBackendInterface
{
public:
    VideoDecoderProxy(JNIEnv* env, DecodeEventInterface* eventInterface, avs::VideoCodec codecType);
    virtual ~VideoDecoderProxy();

    /* Begin avs::DecoderBackendInterface */
    avs::Result initialize(const avs::DeviceHandle& device, int frameWidth, int frameHeight, const avs::DecoderParams& params) override;
    avs::Result shutdown() override;
    avs::Result registerSurface(const avs::SurfaceBackendInterface* surface) override;
    avs::Result unregisterSurface(const avs::SurfaceBackendInterface* surface) override;
    avs::Result decode(const void* buffer, size_t bufferSizeInBytes, avs::VideoPayloadType payaloadType) override;
    /* End avs::DecoderBackendInterface */

    void NotifyFrameAvailable();
    static void InitializeJNI(JNIEnv* env);

private:
    void InitializeVideoDecoder(OVR::SurfaceTexture* surfaceTexture);
    void ShutdownVideoDecoder();

    int mFrameWidth, mFrameHeight;
    bool mInitialized;

    OVR::SurfaceTexture* mSurfaceTexture;
    DecodeEventInterface* mEventInterface;

    JNIEnv* mEnv;
    jobject mVideoDecoder;

    struct JNI {
        jclass videoDecoderClass;
        jmethodID ctorMethod;
        jmethodID initializeMethod;
        jmethodID shutdownMethod;
        jmethodID decodeMethod;
    };
    static JNI jni;
};


