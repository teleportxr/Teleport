// (C) Copyright 2018-2020 Simul Software Ltd

#include "SL_AudioPlayer.h"
#include <android/ndk-version.h>
#include <vector>

#define FAILED(r)      (((SLresult)(r)) != SL_RESULT_SUCCESS)


class RecordBuffer
{
public:
	RecordBuffer(size_t size)
		: mSize(size)
	{
		mBuffers = new char*[2];
		for(int i = 0; i < 2; i++)
		{
			mBuffers[i] = new char[size];
		}
	}

	~RecordBuffer()
	{
		for(int i = 0; i < 2; i++)
		{
			delete[] mBuffers[i];
		}
		delete[] mBuffers;
	}

	size_t getSize() const
	{
		return mSize;
	}

	char* getCurrentBuffer()
	{
		return mBuffers[mIndex];
	}

	char* getNextBuffer()
	{
		mIndex = mIndex == 0 ? 1 : 0;
		return mBuffers[mIndex];
	}

private:
	size_t mSize;
	int mIndex = -1;
	char** mBuffers;
};

// Forward declaration of utility functions and callback
SLuint32 getDefaultByteOrder();
SLuint32 channelCountToChannelMask(int channelCount);
static void outputBufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void* context);
static void inputBufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void* context);

// Google will add below variables to <SLES/OpenSLES_Android.h> (eventually)
static constexpr int SL_ANDROID_SPEAKER_STEREO = (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT);

static constexpr int SL_ANDROID_SPEAKER_QUAD = (SL_ANDROID_SPEAKER_STEREO
										 | SL_SPEAKER_BACK_LEFT | SL_SPEAKER_BACK_RIGHT);

static constexpr int SL_ANDROID_SPEAKER_5DOT1 = (SL_ANDROID_SPEAKER_QUAD
										  | SL_SPEAKER_FRONT_CENTER  | SL_SPEAKER_LOW_FREQUENCY);

static constexpr int SL_ANDROID_SPEAKER_7DOT1 = (SL_ANDROID_SPEAKER_5DOT1 | SL_SPEAKER_SIDE_LEFT
		| SL_SPEAKER_SIDE_RIGHT);

static constexpr SLuint32  SL_ANDROID_UNKNOWN_CHANNELMASK  = 0;

static std::mutex playingMutex;
static std::mutex recordingMutex;

SL_AudioPlayer::SL_AudioPlayer()
{
	
}

SL_AudioPlayer::~SL_AudioPlayer()
{
	if (mConfigured) {
        deconfigure();
    }

	std::lock_guard<std::mutex> playingGuard(playingMutex);
	std::lock_guard<std::mutex> recordingGuard(recordingMutex);

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
		SCA_CERR << "SL_AudioPlayer: Audio player has already been initialized." << std::endl;
		return sca::Result::AudioPlayerAlreadyInitialized;
	}

    SLEngineOption engineOption[] = {
            {(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE}
    };

	// Create the engine
    if (FAILED(slCreateEngine( &mEngineObject, 1, engineOption, 0, NULL, NULL)))
    {
        SCA_CERR << "SL_AudioPlayer: Error occurred trying to create audio engine." << std::endl;
        return sca::Result::AudioDeviceInitializationError;
    }

    // Realize the engine
    if (FAILED((*mEngineObject)->Realize(mEngineObject, SL_BOOLEAN_FALSE)))
    {
        SCA_CERR << "SL_AudioPlayer: Error occurred trying to realize the audio engine object." << std::endl;
        return sca::Result::AudioDeviceInitializationError;
    }

    // Get the engine interface which can be used to create other objects
	if (FAILED((*mEngineObject)->GetInterface(mEngineObject, SL_IID_ENGINE, &mEngineInterface)))
	{
		SCA_CERR << "SL_AudioPlayer: Error occurred trying to create audio engine interface." << std::endl;
		return sca::Result::AudioDeviceInitializationError;
    }

	if (FAILED((*mEngineInterface)->CreateOutputMix(mEngineInterface, &mOutputMixObject, 0, 0, 0)))
	{
		SCA_CERR << "SL_AudioPlayer: Error occurred trying to create output mixer." << std::endl;
		return sca::Result::AudioOutputMixerInitializationError;
	}

	if (FAILED((*mOutputMixObject)->Realize(mOutputMixObject, SL_BOOLEAN_FALSE)))
	{
		SCA_CERR << "SL_AudioPlayer: Error occurred trying to realize output mixer object." << std::endl;
		return sca::Result::AudioOutputMixerInitializationError;
	}

	mInitialized = true;

	return sca::Result::OK;
}

sca::Result SL_AudioPlayer::configure(const sca::AudioParams& audioParams)
{
	if (!mInitialized)
	{
		SCA_CERR << "SL_AudioPlayer: Audio player has not been initialized." << std::endl;
		return sca::Result::AudioPlayerNotInitialized;
	}

	if (mConfigured)
	{
		SCA_CERR << "SL_AudioPlayer: Audio player has already been configured." << std::endl;
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

	/////////////////////////////////////////////////////////////////////////////////////////////////
	// Player
	/////////////////////////////////////////////////////////////////////////////////////////////////

    SLDataSource outputAudioSrc = {&loc_bufq, &format_pcm_ex};

	SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, mOutputMixObject};
	SLDataSink outputAudioSink = {&loc_outmix, NULL};

	const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION};
	const SLboolean reqs[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

	// Stream setup
    if (FAILED((*mEngineInterface)->CreateAudioPlayer(mEngineInterface, &mPlayerObject, &outputAudioSrc,
    		&outputAudioSink, sizeof(ids) / sizeof(ids[0]), ids, reqs)))
	{
		SCA_CERR << "SL_AudioPlayer: Error occurred trying to create audio stream." << std::endl;
		return sca::Result::AudioStreamCreationError;
	}

	SLAndroidConfigurationItf configItf = nullptr;

    // Configure the stream.
    if (FAILED((*mPlayerObject)->GetInterface(mPlayerObject, SL_IID_ANDROIDCONFIGURATION, (void *)&configItf)))
	{
		SCA_CERR << "SL_AudioPlayer: Error occurred trying to get android configuration for audio stream." << std::endl;
		return sca::Result::AudioStreamConfigurationError;
	}

	SLresult pmResult = SL_RESULT_SUCCESS;

	// 12 for low latency
    SLuint32 performanceMode = SL_ANDROID_PERFORMANCE_LATENCY;

	pmResult = (*configItf)->SetConfiguration(configItf, SL_ANDROID_KEY_PERFORMANCE_MODE, &performanceMode, sizeof(performanceMode));
	if (SL_RESULT_SUCCESS != pmResult) {
		SCA_CERR << "SL_AudioPlayer: Setting low latency performance mode failed. Default will be used." << std::endl;
	}

	SLuint32 presetValue = SL_ANDROID_STREAM_MEDIA;
	if (FAILED((*configItf)->SetConfiguration(configItf, SL_ANDROID_KEY_STREAM_TYPE, &presetValue, sizeof(presetValue))))
	{
		SCA_CERR << "SL_AudioPlayer: Error occurred trying to set the stream type." << std::endl;
		return sca::Result::AudioStreamConfigurationError;
	}

	if (FAILED((*mPlayerObject)->Realize(mPlayerObject, SL_BOOLEAN_FALSE)))
	{
		SCA_CERR << "SL_AudioPlayer: Error occurred trying to realize the audio stream interface." << std::endl;
		return sca::Result::AudioStreamConfigurationError;
	}

	if (FAILED((*mPlayerObject)->GetInterface(mPlayerObject, SL_IID_PLAY, &mPlayInterface)))
	{
		SCA_CERR << "SL_AudioPlayer: Error occurred trying to acquire the play interface." << std::endl;
		return sca::Result::AudioStreamConfigurationError;
	}

	// Buffer setup
    if (FAILED((*mPlayerObject)->GetInterface(mPlayerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &mOutputBufferQueueInterface)))
    {
        SCA_CERR << "SL_AudioPlayer: Error occurred trying to acquire the output buffer queue interface." << std::endl;
        return sca::Result::AudioBufferInitializationError;
    }

    if (FAILED((*mOutputBufferQueueInterface)->RegisterCallback(mOutputBufferQueueInterface, outputBufferQueueCallback, this)))
    {
        SCA_CERR << "SL_AudioPlayer: Failed to register output buffer queue callback." << std::endl;
        return sca::Result::AudioBufferInitializationError;
    }

	/////////////////////////////////////////////////////////////////////////////////////////////////
	// Recorder
	/////////////////////////////////////////////////////////////////////////////////////////////////

	// Setting up the IO device (microphone)
	SLDataLocator_IODevice ioDevice = {
			SL_DATALOCATOR_IODEVICE,         // Types of
			SL_IODEVICE_AUDIOINPUT,          // device type selected audio input type
			SL_DEFAULTDEVICEID_AUDIOINPUT,   // deviceID
			NULL                             // device instance
	};
	SLDataSource inputAudioSrc = {
			&ioDevice,                       // SLDataLocator_IODevice configuration input
			NULL                   // Input format, the collection does not need
	};

	SLDataSink inputAudioSink = {
			&loc_bufq,                   // SLDataFormat_PCM_EX configuration output
			&format_pcm_ex               // Output data format
	};

	// Create a recorded object
	const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
	const SLboolean req[1] = {SL_BOOLEAN_TRUE};
	if (FAILED((*mEngineInterface)->CreateAudioRecorder(mEngineInterface, &mRecorderObject, &inputAudioSrc,
														&inputAudioSink, 1, id, req)))
	{
		SCA_CERR << "SL_AudioPlayer: Error occurred trying to create audio recorder object." << std::endl;
		return sca::Result::AudioRecorderCreationError;
	}

	// Instantiate this recording object
	if (FAILED((*mRecorderObject)->Realize(mRecorderObject, SL_BOOLEAN_FALSE)))
	{
		SCA_CERR << "SL_AudioPlayer: Error occurred trying to instantiate audio recorder object." << std::endl;
		return sca::Result::AudioRecorderInitializationError;
	}

	// Get the recording interface
	if (FAILED((*mRecorderObject)->GetInterface(mRecorderObject, SL_IID_RECORD, &mRecordInterface)))
	{
		SCA_CERR << "SL_AudioPlayer: Error occurred trying to get the recorder interface." << std::endl;
		return sca::Result::AudioRecorderInitializationError;
	}

	mAudioParams = audioParams;
	mConfigured = true;

	return result;
}

sca::Result SL_AudioPlayer::playStream(const uint8_t* data, size_t dataSize)
{
	if (!mInitialized)
	{
		SCA_CERR << "SL_AudioPlayer: Can't play audio stream because the audio player has not been initialized." << std::endl;
		return sca::Result::AudioPlayerNotInitialized;
	}

	if (!mConfigured)
	{
		//spamming: SCA_CERR << "SL_AudioPlayer: Can't play audio stream because the audio player has not been configured." << std::endl;
		return sca::Result::AudioPlayerNotConfigured;
	}

	//int32_t numFrames = (int32_t)dataSize / (mAudioParams.bitsPerSample * mAudioParams.numChannels);

	mOutputBufferQueue.emplace(std::vector<uint8_t>(data, data + dataSize));

	if(FAILED((*mOutputBufferQueueInterface)->Enqueue(mOutputBufferQueueInterface, mOutputBufferQueue.back().data(), dataSize)))
	{
        SCA_CERR << "SL_AudioPlayer: Error occured trying to enqueue the audio buffer." << std::endl;
		return sca::Result::AudioWriteError;
	}

    if(FAILED((*mPlayInterface)->SetPlayState(mPlayInterface, SL_PLAYSTATE_PLAYING)))
    {
        SCA_CERR << "SL_AudioPlayer: Error occured trying to set state to 'Playing'." << std::endl;
        return sca::Result::AudioSetStateError;
    }

	return sca::Result::OK;
}

sca::Result SL_AudioPlayer::startRecording(std::function<void(const uint8_t * data, size_t dataSize)> recordingCallback)
{
	if (!mInitialized)
	{
		SCA_CERR << "SL_AudioPlayer: Can't record audio because the audio player has not been initialized." << std::endl;
		return sca::Result::AudioPlayerNotInitialized;
	}

	if (!mConfigured)
	{
		// spamming
		// SCA_CERR << "SL_AudioPlayer: Can't record audio because the audio player has not been configured." << std::endl;
		return sca::Result::AudioPlayerNotConfigured;
	}

	if (mRecording)
	{
		SCA_CERR << "SL_AudioPlayer: Already recording." << std::endl;
		return sca::Result::OK;
	}

	SLAndroidSimpleBufferQueueItf recorderBufferQueue;

	// Get the Buffer interface
	if (FAILED((*mRecorderObject)->GetInterface(mRecorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &recorderBufferQueue)))
	{
		SCA_CERR << "SL_AudioPlayer: Error occurred trying to get the recording buffer interface." << std::endl;
		return sca::Result::AudioRecorderInitializationError;
	}

	static constexpr int NUM_RECORD_FRAMES = 2048;

	mRecordBuffer.reset(new RecordBuffer(NUM_RECORD_FRAMES * mAudioParams.numChannels * (mAudioParams.bitsPerSample / 8)));

	if (FAILED((*recorderBufferQueue)->Enqueue(recorderBufferQueue, mRecordBuffer->getNextBuffer(), 1024)))
	{
		SCA_CERR << "SL_AudioPlayer: Error occurred trying to enqueue the buffer interface." << std::endl;
		return sca::Result::AudioBufferInitializationError;
	}

	if (FAILED((*recorderBufferQueue)->RegisterCallback(recorderBufferQueue, inputBufferQueueCallback, this)))
	{
		SCA_CERR << "SL_AudioPlayer: Error occurred trying to register the recorder callback." << std::endl;
		return sca::Result::AudioBufferInitializationError;
	}

	if (FAILED((*mRecordInterface)->SetCallbackEventsMask(mRecordInterface, SL_RECORDEVENT_BUFFER_FULL)))
	{
		SCA_CERR << "SL_AudioPlayer: Error occurred trying to set callback event mask." << std::endl;
		return sca::Result::AudioBufferInitializationError;
	}

	mRecordingCallback = recordingCallback;
	mRecording = true;

	// Start recording
	if(FAILED((*mRecordInterface)->SetRecordState(mRecordInterface, SL_RECORDSTATE_RECORDING)))
	{
		SCA_CERR << "SL_AudioPlayer: Error occured trying to set recording state." << std::endl;
		mRecording = false;
		return sca::Result::AudioSetStateError;
	}

	return sca::Result::OK;
}

sca::Result SL_AudioPlayer::stopRecording()
{
	if (!mRecording)
	{
		SCA_CERR << "SL_AudioPlayer: Not recording." << std::endl;
		return sca::Result::OK;
	}

	// Buffer memory will be subsequently released in recorder callback.
	mRecording = false;

	return sca::Result::OK;
}

sca::Result SL_AudioPlayer::deconfigure()
{
	std::lock_guard<std::mutex> playingGuard(playingMutex);
	std::lock_guard<std::mutex> recordingGuard(recordingMutex);

    if (!mConfigured)
    {
        SCA_CERR << "SL_AudioPlayer: Can't deconfigure audio player because it is not configured." << std::endl;
        return sca::Result::AudioPlayerNotConfigured;
    }

    if (mRecording)
	{
		(*mRecordInterface)->SetRecordState(mRecordInterface, SL_RECORDSTATE_STOPPED);
		stopRecording();
	}

	mOutputBufferQueueInterface = nullptr;
    mPlayInterface = nullptr;

    if (mPlayerObject != nullptr)
    {
        (*mPlayerObject)->Destroy(mPlayerObject);
		mPlayerObject = nullptr;
    }

	if (mRecorderObject != nullptr)
	{
		(*mRecorderObject)->Destroy(mRecorderObject);
		mRecordInterface = nullptr;
	}

    mConfigured = false;

    mAudioParams = {};

    return sca::Result::OK;
}

void SL_AudioPlayer::onAudioProcessed()
{
	mOutputBufferQueue.pop();
}

void SL_AudioPlayer::onAudioRecorded(SLAndroidSimpleBufferQueueItf bq)
{
	if (!!mConfigured)
	{
		return;
	}
	if (!mRecording)
	{
		(*mRecordInterface)->SetRecordState(mRecordInterface, SL_RECORDSTATE_STOPPED);

		// Release the memory
		 mRecordBuffer.reset();
	}
	else
	{
		if(mRecordBuffer.get() != nullptr)
		{
			char* currentBuffer = mRecordBuffer->getCurrentBuffer();
			// Enqueue the other buffer to allow recorder to keep recording
			(*bq)->Enqueue(bq, mRecordBuffer->getNextBuffer(), mRecordBuffer->getSize());
			mRecordingCallback((uint8_t*)currentBuffer, mRecordBuffer->getSize());
		}
	}
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

// This callback handler is called every time an output buffer has been processed by OpenSL ES.
void outputBufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	std::lock_guard<std::mutex> playingGuard(playingMutex);

	SL_AudioPlayer* player = reinterpret_cast<SL_AudioPlayer*>(context);
	if (player)
	{
		player->onAudioProcessed();
	}
}

// This callback handler is called every time an input buffer has been filled by OpenSL ES.
void inputBufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	std::lock_guard<std::mutex> recordingGuard(recordingMutex);

	SL_AudioPlayer* player = reinterpret_cast<SL_AudioPlayer*>(context);
	if (player)
	{
		player->onAudioRecorded(bq);
	}
}



