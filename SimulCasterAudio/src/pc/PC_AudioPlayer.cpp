// (C) Copyright 2018-2020 Simul Software Ltd

#include "PC_AudioPlayer.h"
#include <chrono>
#include <thread>
#include "xaudio2.h"

namespace sca
{
	class VoiceCallback : public IXAudio2VoiceCallback
	{
	public:
		VoiceCallback() {}
		~VoiceCallback() {  }

		// Called when the voice has just finished playing a contiguous audio stream.
		void OnStreamEnd() {  }

		// Unused methods are stubs
		void OnVoiceProcessingPassEnd() { }
		void OnVoiceProcessingPassStart(UINT32 SamplesRequired) {    }
		void OnBufferEnd(void* pBufferContext)
		{
			reinterpret_cast<PC_AudioPlayer*>(pBufferContext)->onAudioProcessed();
		}
		void OnBufferStart(void* pBufferContext) {    }
		void OnLoopEnd(void* pBufferContext) {    }
		void OnVoiceError(void* pBufferContext, HRESULT Error) { }
	};

	PC_AudioPlayer::PC_AudioPlayer()
		: mMasteringVoice(nullptr), mSourceVoice(nullptr)
	{
		mVoiceCallback.reset(new VoiceCallback());
	}

	PC_AudioPlayer::~PC_AudioPlayer()
	{
		if (mConfigured)
		{
			deconfigure();
		}

		if (mMasteringVoice)
		{
			mMasteringVoice->DestroyVoice();
		}
	}

	Result PC_AudioPlayer::initializeAudioDevice()
	{
		if (mInitialized)
		{
			return sca::Result::AudioPlayerAlreadyInitialized;
		}

		mInitResult = std::async(std::launch::async, &PC_AudioPlayer::asyncInitializeAudioDevice, this);

		return sca::Result::OK;
	}

	Result PC_AudioPlayer::asyncInitializeAudioDevice()
	{
		HRESULT hr = S_OK;

		// Get an interface to the main XAudio2 device
		hr = XAudio2Create(mDevice.GetAddressOf());
		if (FAILED(hr))
		{
			SCA_COUT << "PC_AudioPlayer: Error occurred trying to create the XAudio2 device." << std::endl;
			return sca::Result::AudioDeviceInitializationError;
		}

		// Create master voice
		hr = mDevice->CreateMasteringVoice(&mMasteringVoice);
		if (FAILED(hr))
		{
			SCA_COUT << "PC_AudioPlayer: Error occurred trying to create the mastering voice." << std::endl;
			return sca::Result::AudioMasteringVoiceCreationError;
		}

		return sca::Result::OK;
	}

	Result PC_AudioPlayer::configure(const sca::AudioSettings& audioSettings)
	{
		if (mConfigured)
		{
			SCA_COUT << "PC_AudioPlayer: Audio player has already been configured." << std::endl;
			return sca::Result::AudioPlayerAlreadyConfigured;
		}

		if (!mInitialized)
		{
			// This will block until asyncInitializeAudioDevice has finished
			sca::Result result = mInitResult.get();
			if (!result)
			{
				return result;
			}
			mInitialized = true;
		}

		WAVEFORMATEX waveFormat;
		waveFormat.nChannels = audioSettings.numChannels;
		waveFormat.nSamplesPerSec = audioSettings.sampleRate;
		waveFormat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
		waveFormat.wBitsPerSample = audioSettings.bitsPerSample;
		waveFormat.nBlockAlign = (waveFormat.wBitsPerSample * waveFormat.nChannels) / 8;
		waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
		waveFormat.cbSize = 0; // Ignored for PCM formats

		// Create source voice
		HRESULT hr = mDevice->CreateSourceVoice(&mSourceVoice, &waveFormat, 0U, 2.0f, mVoiceCallback.get());
		if (FAILED(hr))
		{
			mMasteringVoice->DestroyVoice();
			SCA_COUT << "PC_AudioPlayer: Error occurred trying to create the source voice." << std::endl;
			return sca::Result::AudioSourceVoiceCreationError;
		}

		mDevice->StartEngine();

		mSourceVoice->Start();

		mAudioSettings = audioSettings;
		mConfigured = true;

		return sca::Result::OK;
	}

	Result PC_AudioPlayer::playStream(const uint8_t* data, size_t dataSize)
	{
		if (!mInitialized)
		{
			SCA_CERR << "PC_AudioPlayer: Can't play audio stream because the audio player has not been initialized." << std::endl;
			return sca::Result::AudioPlayerNotInitialized;
		}

		if (!mConfigured)
		{
			SCA_CERR << "PC_AudioPlayer: Can't play audio stream because the audio player has not been configured." << std::endl;
			return sca::Result::AudioPlayerNotConfigured;
		}

		mAudioBufferQueue.emplace(std::vector<uint8_t>(data, data + dataSize));

		XAUDIO2_BUFFER xaBuffer;
		ZeroMemory(&xaBuffer, sizeof(XAUDIO2_BUFFER));
		xaBuffer.AudioBytes = (UINT32)dataSize;
		xaBuffer.pAudioData = (BYTE* const)mAudioBufferQueue.back().data();
		xaBuffer.pContext = (void*)this;
		xaBuffer.PlayBegin = 0;
		xaBuffer.PlayLength = 0;

		// Submit the audio buffer to the source voice
		// This will queue the buffer and shouldn't effect sound already being played
		HRESULT hr = mSourceVoice->SubmitSourceBuffer(&xaBuffer);
		if (FAILED(hr))
		{
			SCA_CERR << "PC_AudioPlayer: Error occurred trying to submit audio buffer to source voice." << std::endl;
			return sca::Result::AudioPlayerBufferSubmissionError;
		}

		return sca::Result::OK;
	}

	Result PC_AudioPlayer::startRecording(std::function<void(const uint8_t * data, size_t dataSize)> recordingCallback)
	{
		if (!mInitialized)
		{
			SCA_CERR << "PC_AudioPlayer: Can't record audio because the audio player has not been initialized." << std::endl;
			return sca::Result::AudioPlayerNotInitialized;
		}

		if (!mConfigured)
		{
			SCA_CERR << "PC_AudioPlayer: Can't record audio because the audio player has not been configured." << std::endl;
			return sca::Result::AudioPlayerNotConfigured;
		}

		if (!mRecordingAllowed)
		{
			SCA_CERR << "PC_AudioPlayer: The user has not granted permission to record audio." << std::endl;
			return sca::Result::AudioRecordingNotPermitted;
		}

		if (mRecording)
		{
			SCA_COUT << "PC_AudioPlayer: Already recording." << std::endl;
			return sca::Result::OK;
		}

		mRecording = true;
		return sca::Result::OK;
	}

	Result PC_AudioPlayer::stopRecording()
	{
		if (!mRecording)
		{
			SCA_COUT << "PC_AudioPlayer: Not recording." << std::endl;
			return sca::Result::OK;
		}

		mRecording = false;

		return sca::Result::OK;
	}

	Result PC_AudioPlayer::deconfigure()
	{
		if (!mConfigured)
		{
			SCA_COUT << "PC_AudioPlayer: Can't deconfigure audio player because it is not configured." << std::endl;
			return sca::Result::AudioPlayerNotConfigured;
		}

		mRecordingAllowed = false;

		mConfigured = false;

		mAudioSettings = {};

		if (mDevice.Get())
			mDevice->StopEngine();

		if (mSourceVoice)
			mSourceVoice->DestroyVoice();

		return sca::Result::OK;
	}

	void PC_AudioPlayer::onAudioProcessed()
	{
		mAudioBufferQueue.pop();
	}
}


