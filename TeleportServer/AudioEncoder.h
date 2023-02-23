#pragma once

#include "libavstream/audio/audio_interface.h"

namespace teleport
{
	namespace server
	{
		struct ServerSettings;

		//! Implementation of the default audio encoding.
		class AudioEncoder : public avs::AudioEncoderBackendInterface
		{
		public:
			AudioEncoder(const ServerSettings* settings);
			~AudioEncoder() = default;

			// Inherited via AudioEncoderBackendInterface
			avs::Result initialize(const avs::AudioEncoderParams& params) override;
			avs::Result encode(uint32_t timestamp, uint8_t* captureData, size_t captureDataSize) override;
			avs::Result mapOutputBuffer(void*& bufferPtr, size_t& bufferSizeInBytes) override;
			avs::Result unmapOutputBuffer() override;
			avs::Result shutdown() override;

		protected:

		private:
			const ServerSettings* settings;
			avs::AudioEncoderParams params;
		};
	}
}