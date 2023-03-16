// (C) Copyright 2018-2020 Simul Software Ltd

#include "PC_AudioCapture.h"
#include <chrono>
#include <thread>
#include <Windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <comdef.h>
#include <fmt/format.h>
#include <xaudio2.h>
#include "TeleportCore/StringFunctions.h"


using namespace teleport;
using namespace audio;
std::string teleport::audio::GetMessageForHresult(long h)
{
	HRESULT hr = (HRESULT)h;
	switch (hr)
	{
	case XAUDIO2_E_INVALID_CALL: return "XAUDIO2_E_INVALID_CALL";
			//Returned by XAudio2 for certain API usage errors(invalid calls and so on)
			//that are hard to avoid completely and should be handled by a title at runtime.
			//(API usage errors that are completely avoidable, such as invalid parameters,
			//	cause an ASSERT in debug builds and undefined behavior in retail builds,
			//	so no error code is defined for them.)
	case XAUDIO2_E_XMA_DECODER_ERROR: return "XAUDIO2_E_XMA_DECODER_ERROR";
			//The Xbox 360 XMA hardware suffered an unrecoverable error.
	case XAUDIO2_E_XAPO_CREATION_FAILED: return "XAUDIO2_E_XAPO_CREATION_FAILED";
			//An effect failed to instantiate.
	case XAUDIO2_E_DEVICE_INVALIDATED: return "XAUDIO2_E_DEVICE_INVALIDATED";
		default:
			break;
	}
	_com_error error(hr);
	std::string cs;
	std::wstring wstr = error.ErrorMessage();
	cs=fmt::format("Error 0x{0:x}: {1}", (long long)hr,teleport::core::WStringToString(wstr));
	return cs;
}

	PC_AudioCapture::PC_AudioCapture()
	{
		
	}

	PC_AudioCapture::~PC_AudioCapture()
	{
		
	}

	Result PC_AudioCapture::initializeAudioDevice()
	{
		Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;

		HRESULT hr;
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)enumerator.GetAddressOf());
		if (FAILED(hr))
		{
			std::string message = std::system_category().message(hr);
			TELEPORT_INTERNAL_CERR("CoCreateInstance Error: {0}\n", message);
			return Result::AudioRecorderCreationError;
		}

		hr = enumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, mDevice.GetAddressOf());
		if (FAILED(hr))
		{
			std::string message = std::system_category().message(hr);
			TELEPORT_INTERNAL_CERR("GetDefaultAudioEndpoint Error: {0}\n", message);
			return Result::AudioRecorderCreationError;
		}

		if (!mDevice.Get())
		{
			return Result::NoAudioInputDeviceFound;
		}

		hr = mDevice->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, NULL, (void**)mAudioClient.GetAddressOf());
		if (FAILED(hr))
		{
			std::string message = std::system_category().message(hr);
			TELEPORT_INTERNAL_CERR("mDevice->Activate Error: {0}\n", message);
			return Result::AudioRecorderCreationError;
		}

		mAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)mAudioCaptureClient.GetAddressOf());

		//mAudioReceivedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

		return Result::OK;
	}

	Result PC_AudioCapture::configure(const AudioSettings& audioSettings)
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
		HRESULT hr = mAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 200000, 0, &format, nullptr);
		if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT)
		{
			TELEPORT_INTERNAL_CERR("AUDCLNT_E_UNSUPPORTED_FORMAT\n");
		}
		if (FAILED(hr))
		{
			std::string message = GetMessageForHresult(hr);
			TELEPORT_INTERNAL_CERR("PC_AudioCapture: mAudioClient->Initialize Error: {0}\n",message);
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
			std::string message = std::system_category().message(hr);
			TELEPORT_INTERNAL_CERR("PC_AudioCapture: mAudioClient->Start Error: {0}\n", message);
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
