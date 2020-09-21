// (C) Copyright 2018-2020 Simul Software Ltd

#include "PC_AudioPlayer.h"
#include "Platform/CrossPlatform/Macros.h"
#include <chrono>
#include <thread>
#include "xaudio2.h"



class VoiceCallback : public IXAudio2VoiceCallback
{
public:
	VoiceCallback() {}
	~VoiceCallback() {  }

	//Called when the voice has just finished playing a contiguous audio stream.
	void OnStreamEnd() {  }

	//Unused methods are stubs
	void OnVoiceProcessingPassEnd() { }
	void OnVoiceProcessingPassStart(UINT32 SamplesRequired) {    }
	void OnBufferEnd(void* pBufferContext)
	{ 
		//((sca::ThreadSafeQueue<AudioBuffer>*)pBufferContext)->pop();
		delete[] pBufferContext;
	}
	void OnBufferStart(void* pBufferContext) {    }
	void OnLoopEnd(void* pBufferContext) {    }
	void OnVoiceError(void* pBufferContext, HRESULT Error) { }
};

PC_AudioPlayer::PC_AudioPlayer() 
	: masteringVoice(nullptr), sourceVoice(nullptr)
{
	voiceCallback.reset(new VoiceCallback());
}

PC_AudioPlayer::~PC_AudioPlayer() 
{
	if (configured)
		deconfigure();

	if (masteringVoice)
		masteringVoice->DestroyVoice();
}

sca::Result PC_AudioPlayer::initializeAudioDevice()
{
	if (initialized)
	{
		return sca::Result::AudioPlayerAlreadyInitialized;
	}

	initResult = std::async(std::launch::async, &PC_AudioPlayer::asyncInitializeAudioDevice, this);

	return sca::Result::OK;
}

sca::Result PC_AudioPlayer::asyncInitializeAudioDevice()
{
	HRESULT hr = S_OK;

	// Get an interface to the main XAudio2 device
	hr = XAudio2Create(device.GetAddressOf());
	if (FAILED(hr))
	{
		SCA_COUT("Error occurred trying to create the XAudio2 device.");
		return sca::Result::AudioDeviceInitializationError;
	}

	// Create master voice
	hr = device->CreateMasteringVoice(&masteringVoice);
	if (FAILED(hr))
	{
		SCA_COUT("Error occurred trying to create the mastering voice.");
		return sca::Result::AudioMasteringVoiceCreationError;
	}

	return sca::Result::OK;
}

sca::Result PC_AudioPlayer::configure(const sca::AudioParams& audioParams)
{
	if (configured)
	{
		SCA_COUT("Audio player has already been configured.");
		return sca::Result::AudioPlayerAlreadyConfigured;
	}

	if (!initialized)
	{
		// This will block until asyncInitializeAudioDevice has finished
		sca::Result result = initResult.get();
		if (!result)
		{
			return result;
		}
		initialized = true;
	}
	
	WAVEFORMATEX waveFormat;
	waveFormat.nChannels = audioParams.numChannels;
	waveFormat.nSamplesPerSec = audioParams.sampleRate;
	waveFormat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
	waveFormat.wBitsPerSample = audioParams.bitsPerSample;
	waveFormat.nBlockAlign = (waveFormat.wBitsPerSample * waveFormat.nChannels) / 8;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
	waveFormat.cbSize = 0; // Ignored for PCM formats

	// Create source voice
	HRESULT hr = device->CreateSourceVoice(&sourceVoice, &waveFormat, 0U, 2.0f, voiceCallback.get());
	if (FAILED(hr))
	{
		masteringVoice->DestroyVoice();
		SCA_COUT("Error occurred trying to create the source voice.");
		return sca::Result::AudioSourceVoiceCreationError;
	}

	device->StartEngine();

	sourceVoice->Start();

	this->audioParams = audioParams;
	configured = true;
}

sca::Result PC_AudioPlayer::deconfigure()
{
	if (!configured)
	{
		SCA_COUT("Can't deconfigure audio player because it is not configured.");
		return sca::Result::AudioPlayerNotConfigured;
	}

	configured = false;

	audioParams = {};

	if (device.Get())
		device->StopEngine();

	if (sourceVoice)
		sourceVoice->DestroyVoice();

	return sca::Result::OK;
}

sca::Result PC_AudioPlayer::playStream(const uint8_t* data, size_t dataSize)
{
	if (!initialized)
	{
		SCA_COUT("Can't play audio stream because the audio player has not been initialized.");
		return sca::Result::AudioPlayerNotInitialized;
	}

	if (!configured)
	{
		SCA_COUT("Can't play audio stream because the audio player has not been configured.");
		return sca::Result::AudioPlayerNotConfigured;
	}

	BYTE* audioData = new BYTE[dataSize];
	memcpy(&audioData[0], &data[0], dataSize);

	XAUDIO2_BUFFER xaBuffer;
	ZeroMemory(&xaBuffer, sizeof(XAUDIO2_BUFFER));
	xaBuffer.AudioBytes = (UINT32)dataSize;
	//xaBuffer.pAudioData = (BYTE* const)&audioBuffer.data[0];
	//xaBuffer.pContext = (void*)&audioBuffers;
	xaBuffer.pAudioData = (BYTE* const)&audioData[0];
	xaBuffer.pContext = (void*)audioData;
	xaBuffer.PlayBegin = 0;
	xaBuffer.PlayLength = 0;

	// Submit the audio buffer to the source voice
	// This will queue the buffer and shouldn't effect sound already being played
	HRESULT hr = sourceVoice->SubmitSourceBuffer(&xaBuffer);
	if (FAILED(hr))
	{
		SCA_COUT("Error occurred trying to submit audio buffer to source voice.");
		return sca::Result::AudioPlayerBufferSubmissionError;
	}

	return sca::Result::OK;
}



