// (C) Copyright 2018-2020 Simul Software Ltd
#pragma once

#include <libavstream/node.hpp>
#include <libavstream/audio/audio_interface.h>

#include <functional>

namespace teleport
{
	namespace audio
	{
		/*! A class to receive streamed audio and play it through a callback
		*/
		class CustomAudioStreamTarget final : public avs::AudioTargetBackendInterface
		{
		public:
			CustomAudioStreamTarget(std::function<void(const uint8_t* data, size_t dataSize)> playCallback);

			~CustomAudioStreamTarget();

			avs::Result process(const void* buffer, size_t bufferSizeInBytes, avs::AudioPayloadType payloadType) override;

			avs::Result deconfigure() override;

		private:
			std::function<void(const uint8_t* data, size_t dataSize)> mPlayCallback;
		};
	}
}