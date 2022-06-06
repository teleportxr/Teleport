// (C) Copyright 2018 Simul.co

#pragma once

#include <libavstream/decoders/dec_interface.hpp>
#include <jni.h>
#include "Platform/CrossPlatform/Texture.h"

class DecodeEventInterface
{
public:
	virtual void OnFrameAvailable() = 0;
};

class VideoDecoder final : public avs::DecoderBackendInterface
{
public:
	VideoDecoder(JNIEnv* env, DecodeEventInterface* eventInterface);
	virtual ~VideoDecoder();

	/* Begin avs::DecoderBackendInterface */
	avs::Result initialize(const avs::DeviceHandle& device, int frameWidth, int frameHeight, const avs::DecoderParams& params) override;
	avs::Result reconfigure(int frameWidth, int frameHeight, const avs::DecoderParams& params) override;
	avs::Result shutdown() override;
	avs::Result registerSurface(const avs::SurfaceBackendInterface* colorSurface, const avs::SurfaceBackendInterface* alphaSurface = nullptr) override;
	avs::Result unregisterSurface() override;
	avs::Result decode(const void* buffer, size_t bufferSizeInBytes, const void* alphaBuffer, size_t alphaBufferSizeInBytes, avs::VideoPayloadType payaloadType, bool lastPayload) override;
	avs::Result display(bool showAlphaAsColor = false) override;
	/* End avs::DecoderBackendInterface */

	void NotifyFrameAvailable();
	static void InitializeJNI(JNIEnv* env);
	static bool IsJNIInitialized()
	{
		return mJNIInitialized;
	}

private:
	void InitializeVideoDecoder(platform::crossplatform::Texture* colorSurfaceTexture, platform::crossplatform::Texture* alphaSurfaceTexture);
	void ShutdownVideoDecoder();

	int mFrameWidth, mFrameHeight;
	bool mUseAlphaLayerDecoding;
	bool mInitialized;
	static bool mJNIInitialized;

	platform::crossplatform::Texture* mColorSurfaceTexture=nullptr;
	platform::crossplatform::Texture* mAlphaSurfaceTexture=nullptr;
	DecodeEventInterface* mEventInterface=nullptr;

	JNIEnv* mEnv=nullptr;
	jobject mColorDecoder=nullptr;
	jobject mAlphaDecoder=nullptr;

	struct JNI {
		jclass videoDecoderClass;
		jmethodID ctorMethod;
		jmethodID initializeMethod;
		jmethodID shutdownMethod;
		jmethodID decodeMethod;
		jmethodID displayMethod;
	};
	static JNI jni;
};


