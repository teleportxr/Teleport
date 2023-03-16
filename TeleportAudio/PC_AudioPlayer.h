// (C) Copyright 2018-2020 Simul Software Ltd
#pragma once

#include "AudioPlayer.h"
#include <wrl.h>
#include <future>
#include <libavstream/common.hpp>

interface IXAudio2;
interface IXAudio2MasteringVoice;
interface IXAudio2SourceVoice;

namespace teleport
{
	namespace audio
	{
		/*! A class to play audio from streams and files for PC
		*/
		class PC_AudioPlayer final : public AudioPlayer
		{
		public:
			PC_AudioPlayer();
			~PC_AudioPlayer();

			Result initializeAudioDevice() override;

			Result configure(const AudioSettings& audioSettings) override;

			Result playStream(const uint8_t* data, size_t dataSize) override;

			Result startRecording(std::function<void(const uint8_t* data, size_t dataSize)> recordingCallback) override;

			Result processRecordedAudio() override;

			Result stopRecording() override;

			Result deconfigure() override;

			void onAudioProcessed() override;

		private:
			Result asyncInitializeAudioDevice();

			std::future<Result> mInitResult;

			Microsoft::WRL::ComPtr<IXAudio2> mDevice;
			IXAudio2MasteringVoice* mMasteringVoice;
			IXAudio2SourceVoice* mSourceVoice;

			std::unique_ptr<class VoiceCallback> mVoiceCallback;

			avs::ThreadSafeQueue<std::vector<uint8_t>> mAudioBufferQueue;

			std::unique_ptr<class PC_AudioCapture> mAudioCapture;
		};
	}
}