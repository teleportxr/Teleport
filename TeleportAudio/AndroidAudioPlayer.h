// (C) Copyright 2018-2020 Simul Software Ltd
#pragma once

#include "AudioPlayer.h"
#include <android/ndk-version.h>

/*! A class to play audio from streams and files for PC
*/
namespace teleport
{
	namespace audio
	{
		class AndroidAudioPlayer final : public AudioPlayer
		{
			struct Private;
		public:
			AndroidAudioPlayer();
			~AndroidAudioPlayer();

			Result playStream(const uint8_t* data, size_t dataSize) override;

			Result initializeAudioDevice() override;

			Result configure(const AudioSettings& audioSettings) override;

			Result startRecording(std::function<void(const uint8_t* data, size_t dataSize)> recordingCallback) override;

			Result processRecordedAudio() override;

			Result stopRecording() override;

			Result deconfigure() override;

			void onAudioProcessed() override;

		private:
			Private* m_data = nullptr;
		};
	}
}