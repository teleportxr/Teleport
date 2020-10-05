// (C) Copyright 2018-2020 Simul Software Ltd

#include "SL_AudioPlayer.h"
#include <android/ndk-version.h>
#include <iostream>
#include <vector>

#define FAILED(r)      (((SLresult)(r)) != SL_RESULT_SUCCESS)

// Forward declaration of utility functions and callback
SLuint32 getDefaultByteOrder();
SLuint32 channelCountToChannelMask(int channelCount);
static void bufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

// Google will add below variables to <SLES/OpenSLES_Android.h> (eventually)
static constexpr int SL_ANDROID_SPEAKER_STEREO = (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT);

static constexpr int SL_ANDROID_SPEAKER_QUAD = (SL_ANDROID_SPEAKER_STEREO
										 | SL_SPEAKER_BACK_LEFT | SL_SPEAKER_BACK_RIGHT);

static constexpr int SL_ANDROID_SPEAKER_5DOT1 = (SL_ANDROID_SPEAKER_QUAD
										  | SL_SPEAKER_FRONT_CENTER  | SL_SPEAKER_LOW_FREQUENCY);

static constexpr int SL_ANDROID_SPEAKER_7DOT1 = (SL_ANDROID_SPEAKER_5DOT1 | SL_SPEAKER_SIDE_LEFT
		| SL_SPEAKER_SIDE_RIGHT);

static constexpr SLuint32  SL_ANDROID_UNKNOWN_CHANNELMASK  = 0;

SL_AudioPlayer::SL_AudioPlayer()
{
	
}

SL_AudioPlayer::~SL_AudioPlayer()
{
	if (mConfigured) {
        deconfigure();
    }

	if (mOutputMixObject != nullptr) {
		(*mOutputMixObject)->Destroy(mOutputMixObject);
		mOutputMixObject = nullptr;
	}

    if (mEngineObject != nullptr)
    {
        (*mEngineObject)->Destroy(mEngineObject);
		mEngineObject = nullptr;
        mEngineInterface = nullptr;
    }
}

sca::Result SL_AudioPlayer::initializeAudioDevice()
{
	if (mInitialized)
	{
		SCA_CERR("SL_AudioPlayer: Audio player has already been initialized.");
		return sca::Result::AudioPlayerAlreadyInitialized;
	}

    SLEngineOption engineOption[] = {
            {(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE}
    };

	// Create the engine
    if (FAILED(slCreateEngine( &mEngineObject, 1, engineOption, 0, NULL, NULL)))
    {
        SCA_CERR("SL_AudioPlayer: Error occurred trying to create audio engine.");
        return sca::Result::AudioDeviceInitializationError;
    }

    // Realize the engine
    if (FAILED((*mEngineObject)->Realize(mEngineObject, SL_BOOLEAN_FALSE)))
    {
        SCA_CERR("SL_AudioPlayer: Error occurred trying to realize the audio engine object.");
        return sca::Result::AudioDeviceInitializationError;
    }

    // Get the engine interface which can be used to create other objects
	if (FAILED((*mEngineObject)->GetInterface(mEngineObject, SL_IID_ENGINE, &mEngineInterface)))
	{
		SCA_CERR("SL_AudioPlayer: Error occurred trying to create audio engine interface.");
		return sca::Result::AudioDeviceInitializationError;
    }

	if (FAILED((*mEngineInterface)->CreateOutputMix(mEngineInterface, &mOutputMixObject, 0, 0, 0)))
	{
		SCA_CERR("SL_AudioPlayer: Error occurred trying to create output mixer.");
		return sca::Result::AudioOutputMixerInitializationError;
	}

	if (FAILED((*mOutputMixObject)->Realize(mOutputMixObject, SL_BOOLEAN_FALSE)))
	{
		SCA_CERR("SL_AudioPlayer: Error occurred trying to realize output mixer object.");
		return sca::Result::AudioOutputMixerInitializationError;
	}

	mInitialized = true;

	return sca::Result::OK;
}

sca::Result SL_AudioPlayer::configure(const sca::AudioParams& audioParams)
{
	if (!mInitialized)
	{
		SCA_CERR("SL_AudioPlayer: Audio player has not been initialized.");
		return sca::Result::AudioPlayerNotInitialized;
	}

	if (mConfigured)
	{
		SCA_CERR("SL_AudioPlayer: Audio player has already been configured.");
		return sca::Result::AudioPlayerAlreadyConfigured;
	}

	sca::Result result = sca::Result::OK;

    SLuint32 bitsPerSample = static_cast<SLuint32>(audioParams.bitsPerSample);

    constexpr int bufferQueueLength = 2;

    // Configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,    // locatorType
            static_cast<SLuint32>(bufferQueueLength)};   // numBuffers

	/**
    * API 21 (Lollipop) introduced support for floating-point data representation and an extended
    * data format type: SLAndroidDataFormat_PCM_EX. Would need change for older APIs but we likely won't need to target older ones
    */
    // Define the audio data format.
	SLAndroidDataFormat_PCM_EX format_pcm_ex = {
			SL_ANDROID_DATAFORMAT_PCM_EX,       // formatType
            static_cast<SLuint32>(audioParams.numChannels),           // numChannels
            static_cast<SLuint32>(audioParams.sampleRate * 1000),    // milliSamplesPerSec
            bitsPerSample,                      // bitsPerSample
            bitsPerSample,                      // containerSize;
            channelCountToChannelMask(audioParams.numChannels), // channelMask
            getDefaultByteOrder(),
			SL_ANDROID_PCM_REPRESENTATION_FLOAT
    };

    SLDataSource audioSrc = {&loc_bufq, &format_pcm_ex};

	SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, mOutputMixObject};
	SLDataSink audioSink = {&loc_outmix, NULL};

	const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION};
	const SLboolean reqs[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

	// Stream setup
    if (FAILED((*mEngineInterface)->CreateAudioPlayer(mEngineInterface, &mPlayerInterface, &audioSrc,
    		&audioSink, sizeof(ids) / sizeof(ids[0]), ids, reqs)))
	{
		SCA_CERR("SL_AudioPlayer: Error occurred trying to create audio stream.");
		return sca::Result::AudioStreamCreationError;
	}

	SLAndroidConfigurationItf configItf = nullptr;

    // Configure the stream.
    if (FAILED((*mPlayerInterface)->GetInterface(mPlayerInterface, SL_IID_ANDROIDCONFIGURATION, (void *)&configItf)))
	{
		SCA_CERR("SL_AudioPlayer: Error occurred trying to get android configuration for audio stream.");
		return sca::Result::AudioStreamConfigurationError;
	}

	SLresult pmResult = SL_RESULT_SUCCESS;

	// 12 for low latency
    SLuint32 performanceMode = SL_ANDROID_PERFORMANCE_LATENCY;

	pmResult = (*configItf)->SetConfiguration(configItf, SL_ANDROID_KEY_PERFORMANCE_MODE, &performanceMode, sizeof(performanceMode));
	if (SL_RESULT_SUCCESS != pmResult) {
		SCA_CERR("SL_AudioPlayer: Setting low latency performance mode failed. Default will be used.");
	}

	SLuint32 presetValue = SL_ANDROID_STREAM_MEDIA;
	if (FAILED((*configItf)->SetConfiguration(configItf, SL_ANDROID_KEY_STREAM_TYPE, &presetValue, sizeof(presetValue))))
	{
		SCA_CERR("SL_AudioPlayer: Error occurred trying to set the stream type.");
		return sca::Result::AudioStreamConfigurationError;
	}

	if (FAILED((*mPlayerInterface)->Realize(mPlayerInterface, SL_BOOLEAN_FALSE)))
	{
		SCA_CERR("SL_AudioPlayer: Error occurred trying to realize the audio stream interface.");
		return sca::Result::AudioStreamConfigurationError;
	}

	if (FAILED((*mPlayerInterface)->GetInterface(mPlayerInterface, SL_IID_PLAY, &mPlayInterface)))
	{
		SCA_CERR("SL_AudioPlayer: Error occurred trying to acquire the play interface.");
		return sca::Result::AudioStreamConfigurationError;
	}

	// Buffer setup
    if (FAILED((*mPlayerInterface)->GetInterface(mPlayerInterface, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &mSimpleBufferQueueInterface)))
    {
        SCA_CERR("SL_AudioPlayer: Error occurred trying to acquire the simple buffer queue interface.");
        return sca::Result::AudioBufferInitializationError;
    }

    if (FAILED((*mSimpleBufferQueueInterface)->RegisterCallback(mSimpleBufferQueueInterface, bufferQueueCallback, this)))
    {
        SCA_CERR("SL_AudioPlayer: Failed to register buffer queue callback.");
        return sca::Result::AudioBufferInitializationError;
    }

	mAudioParams = audioParams;
	mConfigured = true;

	return result;
}

sca::Result SL_AudioPlayer::playStream(const uint8_t* data, size_t dataSize)
{
	if (!mInitialized)
	{
		SCA_CERR("SL_AudioPlayer: Can't play audio stream because the audio player has not been initialized.");
		return sca::Result::AudioPlayerNotInitialized;
	}

	if (!mConfigured)
	{
		SCA_CERR("SL_AudioPlayer: Can't play audio stream because the audio player has not been configured.");
		return sca::Result::AudioPlayerNotConfigured;
	}

	//int32_t numFrames = (int32_t)dataSize / (mAudioParams.bitsPerSample * mAudioParams.numChannels);

    mAudioBufferQueue.emplace(std::vector<uint8_t>(data, data + dataSize));

	if(FAILED((*mSimpleBufferQueueInterface)->Enqueue(mSimpleBufferQueueInterface, mAudioBufferQueue.back().data(), dataSize)))
	{
        SCA_CERR("SL_AudioPlayer: Error occured trying to enqueue the audio buffer.");
		return sca::Result::AudioWriteError;
	}

    if(FAILED((*mPlayInterface)->SetPlayState(mPlayInterface, SL_PLAYSTATE_PLAYING)))
    {
        SCA_CERR("SL_AudioPlayer: Error occured trying to set state to 'Playing'.");
        return sca::Result::AudioSetStateError;
    }

	return sca::Result::OK;
}

void SL_AudioPlayer::onWriteEnd()
{
    mAudioBufferQueue.pop();
}

sca::Result SL_AudioPlayer::deconfigure()
{
    if (!mConfigured)
    {
        SCA_CERR("SL_AudioPlayer: Can't deconfigure audio player because it is not configured.");
        return sca::Result::AudioPlayerNotConfigured;
    }

    mSimpleBufferQueueInterface = nullptr;
    mPlayInterface = nullptr;

    if (mPlayerInterface != nullptr)
    {
        (*mPlayerInterface)->Destroy(mPlayerInterface);
        mPlayerInterface = nullptr;
    }

    mConfigured = false;

    mAudioParams = {};

    return sca::Result::OK;
}

SLuint32 channelCountToChannelMask(int channelCount)  {
	SLuint32 channelMask = 0;

	switch (channelCount) {
		case 1:
			channelMask = SL_SPEAKER_FRONT_CENTER;
			break;

		case 2:
			channelMask = SL_ANDROID_SPEAKER_STEREO;
			break;

		case 4: // Quad
			channelMask = SL_ANDROID_SPEAKER_QUAD;
			break;

		case 6: // 5.1
			channelMask = SL_ANDROID_SPEAKER_5DOT1;
			break;

		case 8: // 7.1
			channelMask = SL_ANDROID_SPEAKER_7DOT1;
			break;

		default:
			channelMask = SL_ANDROID_UNKNOWN_CHANNELMASK;
			break;
	}
	return channelMask;
}

static bool isLittleEndian() {
	static uint32_t value = 1;
	return (*reinterpret_cast<uint8_t *>(&value) == 1);  // Does address point to LSB?
}

SLuint32 getDefaultByteOrder() {
	return isLittleEndian() ? SL_BYTEORDER_LITTLEENDIAN : SL_BYTEORDER_BIGENDIAN;
}

// This callback handler is called every time a buffer has been processed by OpenSL ES.
void bufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    (reinterpret_cast<SL_AudioPlayer*>(context))->onWriteEnd();
}




