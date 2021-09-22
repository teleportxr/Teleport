// libavstream
// (c) Copyright 2018-2021 Simul Software Ltd

#pragma once

#include <libavstream/decoders/dec_interface.hpp>

class VideoDecoder final : public avs::DecoderBackendInterface
{
public:
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

	avs::SurfaceFormat m_registeredSurfaceFormat = avs::SurfaceFormat::Unknown;
};

