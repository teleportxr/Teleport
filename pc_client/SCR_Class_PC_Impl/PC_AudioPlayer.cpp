// (C) Copyright 2018-2020 Simul Software Ltd

#include "PC_AudioPlayer.h"
#include "Platform/CrossPlatform/Macros.h"

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
		((ThreadSafeQueue<AudioBuffer>*)pBufferContext)->pop();
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
	if (sourceVoice)
		sourceVoice->DestroyVoice();

	if (masteringVoice)
		masteringVoice->DestroyVoice();

	if (device.Get())
		device->StopEngine();
}

sca::Result PC_AudioPlayer::initialize(const sca::AudioParams& audioParams)
{
	sca::Result result = sca::AudioPlayer::initialize(audioParams);

	if (!result)
	{
		return result;
	}

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
		device->Release();
		return sca::Result::AudioMasteringVoiceCreationError;
	}
	
	device->StartEngine();

	WAVEFORMATEX waveFormat;
	waveFormat.nChannels = audioParams.numChannels;
	waveFormat.nSamplesPerSec = audioParams.sampleRate;
	waveFormat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
	waveFormat.wBitsPerSample = audioParams.bitsPerSample;
	waveFormat.nBlockAlign = (waveFormat.wBitsPerSample * waveFormat.nChannels) / 8;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
	waveFormat.cbSize = 0; // Ignored for PCM formats

	// Create source voice
	hr = device->CreateSourceVoice(&sourceVoice, &waveFormat, 0U, 2.0f, voiceCallback.get());
	if (FAILED(hr))
	{
		SCA_COUT("Error occurred trying to create the source voice.");
		masteringVoice->DestroyVoice();
		device->StopEngine();
		device->Release();
		return sca::Result::AudioSourceVoiceCreationError;
	}

	sourceVoice->Start();

	return sca::Result::OK;
}

sca::Result PC_AudioPlayer::playStream(const uint8_t* data, size_t dataSize)
{
	if (!initialized)
	{
		SCA_COUT("Can't play audio stream because the audio player has not been initialized.");
		return sca::Result::AudioPlayerNotInitialized;
	}

	auto& audioBuffer = audioBuffers.emplace(AudioBuffer());
	audioBuffer.data.resize(dataSize);
	memcpy(&audioBuffer.data[0], &data[0], dataSize);
	ZeroMemory(&audioBuffer.buffer, sizeof(XAUDIO2_BUFFER));
	audioBuffer.buffer.AudioBytes = (UINT32)dataSize;
	audioBuffer.buffer.pAudioData = (BYTE* const)&audioBuffer.data[0];
	audioBuffer.buffer.pContext = (void*)&audioBuffers;
	audioBuffer.buffer.PlayBegin = 0;
	audioBuffer.buffer.PlayLength = 0;

	// Submit the audio buffer to the source voice
	// This will queue the buffer and shouldn't effect sound already being played
	HRESULT hr = sourceVoice->SubmitSourceBuffer(&audioBuffer.buffer);
	if (FAILED(hr))
	{
		SCA_COUT("Error occurred trying to submit audio buffer to source voice.");
		return sca::Result::AudioPlayerBufferSubmissionError;
	}

	return sca::Result::OK;
}



