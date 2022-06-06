// (C) Copyright 2018-2022 Simul Software Ltd

#include "VideoDecoder.h"
#include "NdkVideoDecoder.h"

extern "C" {

JNIEXPORT void Java_co_simul_teleportvrquestclient_VideoDecoder_nativeFrameAvailable(JNIEnv* env, jclass clazz, jlong proxyPtr)
{
	VideoDecoder* proxy = reinterpret_cast<VideoDecoder*>(proxyPtr);
	assert(proxy);
	proxy->NotifyFrameAvailable();
}

} // extern "C"

VideoDecoder::JNI VideoDecoder::jni;
bool VideoDecoder::mJNIInitialized=false;

VideoDecoder::VideoDecoder(JNIEnv *env, DecodeEventInterface* eventInterface)
	: mFrameWidth(0), mFrameHeight(0)
	, mUseAlphaLayerDecoding(false)
	, mInitialized(false)
	//, mColorSurfaceTexture(nullptr)
	//, mAlphaSurfaceTexture(nullptr)
	, mEventInterface(eventInterface)
	, mEnv(env)
	, mColorDecoder(nullptr)
	, mAlphaDecoder(nullptr)
{

}

VideoDecoder::~VideoDecoder()
{
	if(mColorDecoder)
	{
		mEnv->DeleteGlobalRef(mColorDecoder);
	}
	if(mAlphaDecoder)
	{
		mEnv->DeleteGlobalRef(mAlphaDecoder);
	}
}

avs::Result VideoDecoder::initialize(const avs::DeviceHandle& device, int frameWidth, int frameHeight, const avs::DecoderParams& params)
{
	if(mInitialized)
	{
		return avs::Result::OK;
	}

	mFrameWidth = frameWidth;
	mFrameHeight = frameHeight;

	mUseAlphaLayerDecoding = params.useAlphaLayerDecoding;

	assert(mEnv);

	jobject colorDecoder = mEnv->NewObject(jni.videoDecoderClass, jni.ctorMethod, this, static_cast<int>(params.codec));
	mColorDecoder = mEnv->NewGlobalRef(colorDecoder);

	if (mUseAlphaLayerDecoding)
	{
		jobject alphaDecoder = mEnv->NewObject(jni.videoDecoderClass, jni.ctorMethod, this, static_cast<int>(params.codec));
		mAlphaDecoder = mEnv->NewGlobalRef(alphaDecoder);
	}

	mInitialized = true;

	return avs::Result::OK;
}

avs::Result VideoDecoder::reconfigure(int frameWidth, int frameHeight, const avs::DecoderParams& params)
{
	if(!mInitialized) {
		return avs::Result::DecoderBackend_NotInitialized;
	}

	mInitialized = false;

	avs::DeviceHandle dummyHandle;

	return initialize(dummyHandle, frameWidth, frameHeight, params);
}

avs::Result VideoDecoder::shutdown()
{
	if(!mInitialized) {
		return avs::Result::DecoderBackend_NotInitialized;
	}
 /*   if(mColorSurfaceTexture) {
		ShutdownVideoDecoder();
	}*/

	mFrameWidth  = 0;
	mFrameHeight = 0;
	mInitialized = false;
	return avs::Result::OK;
}

avs::Result VideoDecoder::registerSurface(const avs::SurfaceBackendInterface* colorSurface, const avs::SurfaceBackendInterface* alphaSurface)
{
	if(!mInitialized) {
		return avs::Result::DecoderBackend_NotInitialized;
	}
/*	if(mColorSurfaceTexture) {
		return avs::Result::DecoderBackend_SurfaceAlreadyRegistered;
	}*/

	/*const VideoSurface* videoColorSurface = dynamic_cast<const VideoSurface*>(colorSurface);
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

	InitializeVideoDecoder(videoColorSurface->GetTexture(), ovrAlphaSurface);*/
	return avs::Result::OK;
}

avs::Result VideoDecoder::unregisterSurface()
{
	/*if(!mColorSurfaceTexture) {
		return avs::Result::DecoderBackend_SurfaceNotRegistered;
	}*/

	ShutdownVideoDecoder();
	return avs::Result::OK;
}

avs::Result VideoDecoder::decode(const void* buffer, size_t bufferSizeInBytes, const void* alphaBuffer, size_t alphaBufferSizeInBytes, avs::VideoPayloadType payloadType, bool lastPayload)
{
	if(!mInitialized)
	{
		return avs::Result::DecoderBackend_NotInitialized;
	}
  /*  if(!mColorSurfaceTexture)
	{
		return avs::Result::DecoderBackend_InvalidSurface;
	}*/

	jobject jbuffer = mEnv->NewDirectByteBuffer(const_cast<void*>(buffer), bufferSizeInBytes);
	jboolean isReadyToDisplay = mEnv->CallBooleanMethod(mColorDecoder, jni.decodeMethod, jbuffer, payloadType, lastPayload);
	mEnv->DeleteLocalRef(jbuffer);

	if (mUseAlphaLayerDecoding)
	{
		jbuffer = mEnv->NewDirectByteBuffer(const_cast<void*>(alphaBuffer), alphaBufferSizeInBytes);
		isReadyToDisplay &= mEnv->CallBooleanMethod(mAlphaDecoder, jni.decodeMethod, jbuffer, payloadType, lastPayload);
		mEnv->DeleteLocalRef(jbuffer);
	}

	return isReadyToDisplay ? avs::Result::DecoderBackend_ReadyToDisplay : avs::Result::OK;
}

avs::Result VideoDecoder::display(bool showAlphaAsColor)
{
	if(!mInitialized) {
		return avs::Result::DecoderBackend_NotInitialized;
	}
  /*  if(!mColorSurfaceTexture) {
		return avs::Result::DecoderBackend_InvalidSurface;
	}*/
	jboolean displayResult = mEnv->CallBooleanMethod(mColorDecoder, jni.displayMethod);
	if (mUseAlphaLayerDecoding && displayResult)
	{
		displayResult = mEnv->CallBooleanMethod(mAlphaDecoder, jni.displayMethod);
	}
	return displayResult ? avs::Result::OK : avs::Result::DecoderBackend_DisplayFailed;
}

void VideoDecoder::NotifyFrameAvailable()
{
	if(mEventInterface)
	{
		mEventInterface->OnFrameAvailable();
	}
}

void VideoDecoder::InitializeJNI(JNIEnv* env)
{
	assert(env);

	jclass videoDecoderClass = env->FindClass("co/simul/teleportvrquestclient/VideoDecoder");
	assert(videoDecoderClass);
	jni.videoDecoderClass = (jclass)env->NewGlobalRef((jobject)videoDecoderClass);

	jni.ctorMethod = env->GetMethodID(jni.videoDecoderClass, "<init>", "(JI)V");
	jni.initializeMethod = env->GetMethodID(jni.videoDecoderClass, "initialize", "(Landroid/graphics/SurfaceTexture;II)V");
	jni.shutdownMethod = env->GetMethodID(jni.videoDecoderClass, "shutdown", "()V");
	jni.decodeMethod = env->GetMethodID(jni.videoDecoderClass, "decode", "(Ljava/nio/ByteBuffer;IZ)Z");
	jni.displayMethod = env->GetMethodID(jni.videoDecoderClass, "display", "()Z");
	mJNIInitialized=true;
}

void VideoDecoder::InitializeVideoDecoder(platform::crossplatform::Texture* colorSurfaceTexture, platform::crossplatform::Texture* alphaSurfaceTexture)
{
	assert(mFrameWidth > 0 && mFrameHeight > 0);
	assert(colorSurfaceTexture);

	mColorSurfaceTexture = colorSurfaceTexture;
	mAlphaSurfaceTexture = alphaSurfaceTexture;

	mEnv->CallVoidMethod(mColorDecoder, jni.initializeMethod, mColorSurfaceTexture->GetJavaObject(), mFrameWidth, mFrameHeight);

	if (mAlphaSurfaceTexture)
	{
		mEnv->CallVoidMethod(mAlphaDecoder, jni.initializeMethod, mAlphaSurfaceTexture->GetJavaObject(), mFrameWidth, mFrameHeight);
	}
}

void VideoDecoder::ShutdownVideoDecoder()
{
	mEnv->CallVoidMethod(mColorDecoder, jni.shutdownMethod);
	mColorDecoder = nullptr;
	if (mUseAlphaLayerDecoding)
	{
		mEnv->CallVoidMethod(mAlphaDecoder, jni.shutdownMethod);
		mAlphaDecoder = nullptr;
	}
   // mColorSurfaceTexture = nullptr;
   // mAlphaSurfaceTexture = nullptr;
}

