// libavstream
// (c) Copyright 2018-2021 Simul Software Ltd

#pragma once

#include <libavstream/decoders/dec_interface.hpp>
#include <memory>
#include <vector>
#include "Platform/Crossplatform/BaseRenderer.h"
#include "Platform/Crossplatform/VideoDecoder.h"
#include "Platform/Crossplatform/RenderPlatform.h"

namespace simul
{
	namespace crossplatform
	{
		class Texture;
	}	
}
class VideoDecoder final : public avs::DecoderBackendInterface
{
public:
	VideoDecoder(simul::crossplatform::RenderPlatform* renderPlatform, simul::crossplatform::Texture* surfaceTexture);
	~VideoDecoder();

	/* Begin DecoderBackendInterface */
	avs::Result initialize(const avs::DeviceHandle& device, int frameWidth, int frameHeight, const avs::DecoderParams& params) override;
	avs::Result reconfigure(int frameWidth, int frameHeight, const avs::DecoderParams& params) override;
	avs::Result shutdown() override;
	avs::Result registerSurface(const avs::SurfaceBackendInterface* surface, const avs::SurfaceBackendInterface* alphaSurface = nullptr) override;
	avs::Result unregisterSurface() override;

	avs::Result decode(const void* buffer, size_t bufferSizeInBytes, const void* alphaBuffer, size_t alphaBufferSizeInBytes, avs::VideoPayloadType payloadType, bool lastPayload) override;
	avs::Result display(bool showAlphaAsColor = false) override;
	/* End DecoderBackendInterface */

private:

	avs::DeviceType m_deviceType = avs::DeviceType::Invalid;

	avs::DecoderParams m_params = {};

	unsigned int m_frameWidth = 0;
	unsigned int m_frameHeight = 0;
	int m_displayPictureIndex = -1;

	std::unique_ptr<simul::crossplatform::VideoDecoder> m_decoder;

	static constexpr uint32_t MAX_ARGS = 10;

	std::vector<simul::crossplatform::VideoDecodeArgument> m_arguments;
	uint32_t m_argCount = 0;
	simul::crossplatform::RenderPlatform* m_renderPlatform;
	simul::crossplatform::Texture* m_outputTexture;
	simul::crossplatform::Texture* m_surfaceTexture;
};

