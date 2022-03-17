#include "AudioEncoder.h"

#include <algorithm>
#include <set>

#include "libavstream/common.hpp"

#include "ServerSettings.h"

#include "ErrorHandling.h"


namespace teleport
{
	AudioEncoder::AudioEncoder(const ServerSettings* settings)
		:settings(settings)
	{}

	avs::Result AudioEncoder::initialize(const avs::AudioEncoderParams& params)
	{
		this->params = params;
		return avs::Result::OK;
	}

	avs::Result AudioEncoder::encode(uint32_t timestamp, uint8_t* captureData, size_t captureDataSize)
	{
		return avs::Result::OK;
	}

	avs::Result AudioEncoder::mapOutputBuffer(void *& bufferPtr, size_t & bufferSizeInBytes)
	{
		return avs::Result::OK;
	}

	avs::Result AudioEncoder::unmapOutputBuffer()
	{
		return avs::Result::OK;
	}

	avs::Result AudioEncoder::shutdown()
	{
		return avs::Result::OK;
	}
}
