// (C) Copyright 2018-2020 Simul Software Ltd

#include "PC_AudioCapture.h"
#include <chrono>
#include <thread>
#include <Windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

namespace sca
{
	PC_AudioCapture::PC_AudioCapture()
	{
		
	}

	PC_AudioCapture::~PC_AudioCapture()
	{
		
	}

	Result PC_AudioCapture::initializeAudioDevice()
	{
		Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;

		if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)enumerator.GetAddressOf())))
		{
			SCA_CERR << "PC_AudioCapture: Error occurred trying to create instance of device enumerator." << std::endl;
			return sca::Result::AudioRecorderCreationError;
		}

		if (FAILED(enumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, mDevice.GetAddressOf())))
		{
			SCA_CERR << "PC_AudioCapture: Error occurred trying to get default audio endpoint." << std::endl;
			return sca::Result::AudioRecorderCreationError;
		}

		if (!mDevice.Get())
		{
			return Result::NoAudioInputDeviceFound;
		}

		if (FAILED(mDevice->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, NULL, (void**)mAudioClient.GetAddressOf())))
		{
			SCA_CERR << "PC_AudioCapture: Error occurred trying to get handle to audio client." << std::endl;
			return sca::Result::AudioRecorderCreationError;
		}

		mAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)mAudioCaptureClient.GetAddressOf());

		//mAudioReceivedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

		return Result::OK;
	}

	Result PC_AudioCapture::configure(const sca::AudioSettings& audioSettings)
	{
		WAVEFORMATEX format;
		format.wFormatTag = WAVE_FORMAT_PCM;
		format.nChannels = audioSettings.numChannels;
		format.nSamplesPerSec = audioSettings.sampleRate;
		format.wBitsPerSample = audioSettings.bitsPerSample;
		format.nBlockAlign = (audioSettings.bitsPerSample * audioSettings.numChannels) / 8;
		format.nAvgBytesPerSec = audioSettings.sampleRate * format.nBlockAlign;
		format.cbSize = 0;
		if(!mAudioClient)
		{
			SCA_CERR << "PC_AudioCapture::configure: mAudioClient is null." << std::endl;
			return Result::AudioRecorderInitializationError;
		}
		if (FAILED(mAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 200000, 0, &format, nullptr)))
		{
			SCA_CERR << "PC_AudioCapture: Error occurred trying to initialize audio client." << std::endl;
			return Result::AudioRecorderInitializationError;
		}

		return Result::OK;
	}

	Result PC_AudioCapture::startRecording(std::function<void(const uint8_t* data, size_t dataSize)> recordingCallback, bool async)
	{
		mRecordingCallback = recordingCallback;

		HRESULT hr = mAudioClient->Start();
		if (FAILED(hr))
		{
			SCA_CERR << "PC_AudioCapture: Error occurred trying to start the audio client." << std::endl;
			return Result::AudioRecorderStartError;
		}

		if (async)
		{
			if (!mCaptureThread.joinable())
			{
				mAsyncCapturingAudio = true;
				mCaptureThread = std::thread(&PC_AudioCapture::asyncCaptureAudio, this);
			}
			mAsync = async;
		}

		return Result::OK;
	}

	Result PC_AudioCapture::processRecordedAudio()
	{
		if (mAsync)
		{
			SCA_CERR << "PC_AudioCapture: processRecordedAudio can't be called when in async mode." << std::endl;
			return Result::AudioProcessingError;
		}

		return captureAudio();
	}

	Result PC_AudioCapture::stopRecording()
	{
		HRESULT hr = mAudioClient->Stop();
		if (FAILED(hr))
		{
			SCA_CERR << "PC_AudioCapture: Error occurred trying to stop the audio client." << std::endl;
			return Result::AudioRecorderStartError;
		}

		if (mAsync)
		{
			if (mCaptureThread.joinable())
			{
				mAsyncCapturingAudio = false;
			}
		}

		return Result::OK;
	}

	Result PC_AudioCapture::deconfigure()
	{
		return Result::OK;
	}


	Result PC_AudioCapture::captureAudio()
	{
		uint32_t packetSize;
		HRESULT hr = mAudioCaptureClient->GetNextPacketSize(&packetSize);
		if (FAILED(hr))
		{
			SCA_CERR << "PC_AudioCapture: Error occurred trying to get the next packet size from audio capture client." << std::endl;
			return Result::AudioProcessingError;
		}

		uint8_t* data = nullptr;
		DWORD flags = 0;
		uint32_t numFramesAvailable = 0;

		// Wait for next buffer event to be signaled.
		//DWORD r = WaitForSingleObject(mAudioReceivedEvent, 200);
		//if (r == WAIT_OBJECT_0)

		while (SUCCEEDED(hr) && packetSize != 0)
		{
			hr = mAudioCaptureClient->GetBuffer(&data, &numFramesAvailable, &flags, nullptr, nullptr);
			if (SUCCEEDED(hr))
			{
				mRecordingCallback(data, packetSize);
				hr = mAudioCaptureClient->ReleaseBuffer(numFramesAvailable);
			}
			hr = mAudioCaptureClient->GetNextPacketSize(&packetSize);
		}
		return Result::OK;
	}

	void PC_AudioCapture::asyncCaptureAudio()
	{
		while (mAsyncCapturingAudio)
		{
			captureAudio();
			std::this_thread::yield();
		}
	}
}


