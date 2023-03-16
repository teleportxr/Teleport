// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "AudioCommon.h"
#include <libavstream/common.hpp>
#include <wrl.h>
#include <atomic>
#include <thread>

struct IMMDevice;
struct IAudioClient3;
struct IAudioCaptureClient;

namespace teleport
{
	namespace audio
	{
		extern std::string GetMessageForHresult(long h);
		/*! A class to capture audio for PC
		*/
		class PC_AudioCapture
		{
		public:
			PC_AudioCapture();
			~PC_AudioCapture();

			Result initializeAudioDevice();

			Result configure(const AudioSettings& audioSettings);

			Result startRecording(std::function<void(const uint8_t* data, size_t dataSize)> recordingCallback, bool async = false);

			Result processRecordedAudio();

			Result stopRecording();

			Result deconfigure();

		private:
			void asyncCaptureAudio();
			Result captureAudio();

			Microsoft::WRL::ComPtr<IMMDevice> mDevice;
			Microsoft::WRL::ComPtr<IAudioClient3> mAudioClient;
			Microsoft::WRL::ComPtr<IAudioCaptureClient> mAudioCaptureClient;
			std::function<void(const uint8_t* data, size_t dataSize)> mRecordingCallback;
			//HANDLE mAudioReceivedEvent = nullptr;
			std::thread mCaptureThread;
			std::atomic_bool mAsyncCapturingAudio = false;
			bool mAsync = false;
		};
	}
}