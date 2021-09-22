// libavstream
// (c) Copyright 2018-2021 Simul Software Ltd

#include "VideoDecoder.h"
#include "Common.h"

using namespace avs;

VideoDecoder::~VideoDecoder()
{
	
}

Result VideoDecoder::initialize(const DeviceHandle& device, int frameWidth, int frameHeight, const DecoderParams& params)
{
	if (device.type == DeviceType::Invalid)
	{
		SCR_CERR << "VideoDecoder: Invalid device handle" << std::endl;
		return Result::DecoderBackend_InvalidDevice;
	}
	if (params.codec == VideoCodec::Invalid)
	{
		SCR_CERR << "VideoDecoder: Invalid video codec type" << std::endl;
		return Result::DecoderBackend_InvalidParam;
	}

	switch (params.codec)
	{
	case VideoCodec::H264:
		break;
	case VideoCodec::HEVC:
		break;
	default:
		SCR_CERR << "VideoDecoder: Unsupported video codec type selected" << std::endl;
		return Result::DecoderBackend_CodecNotSupported;
	}

	/*if (!(CUDA::initialize() && initializeCUVID()))
	{
		return Result::DecoderBackend_InitFailed;
	}*/

	if (device.type != DeviceType::Direct3D12)
	{
		SCR_CERR << "VideoDecoder: Platform library only supports D3D12 video decoder currently." << std::endl;
	}
		

	m_deviceType = device.type;
	m_params = params;
	m_frameWidth = frameWidth;
	m_frameHeight = frameHeight;

	return Result::OK;
}

Result VideoDecoder::reconfigure(int frameWidth, int frameHeight, const DecoderParams& params)
{
	/*if (!m_decoder)
	{
		SCR_CERR << "VideoDecoder: Can't reconfigure because decoder not initialized";
		return Result::DecoderBackend_NotInitialized;
	}*/

	return Result::OK;
}

Result VideoDecoder::shutdown()
{
	// Not using ContextGuard here since we're about to destroy current context.

	/*if (m_registeredSurface)
	{
		unregisterSurface();
	}*/

	// Release devices here

	m_params = {};
	m_deviceType = DeviceType::Invalid;
	m_frameWidth = 0;
	m_frameHeight = 0;
	m_displayPictureIndex = -1;

	return Result::OK;
}

// Note: Alpha surface is not needed for the NVidia decoder because we use a CUDA kernel to write alpha to the color surface.
Result VideoDecoder::registerSurface(const SurfaceBackendInterface* surface, const SurfaceBackendInterface* alphaSurface)
{
	//if (!m_decoder)
	//{
	//	SCR_CERR << "VideoDecoder: Decoder not initialized";
	//	return Result::DecoderBackend_NotInitialized;
	//}
	//if (m_registeredSurface)
	//{
	//	SCR_CERR << "VideoDecoder: Output surface already registered";
	//	return Result::DecoderBackend_SurfaceAlreadyRegistered;
	//}
	//if (!surface || !surface->getResource())
	//{
	//	SCR_CERR << "VideoDecoder: Invalid surface handle";
	//	return Result::DecoderBackend_InvalidSurface;
	//}
	//if (surface->getWidth() != m_frameWidth || surface->getHeight() != m_frameHeight)
	//{
	//	SCR_CERR << "DeocderNV: Output surface dimensions do not match video frame dimensions";
	//	return Result::DecoderBackend_InvalidSurface;
	//}

	return Result::OK;
}

Result VideoDecoder::unregisterSurface()
{
	/*if (!m_decoder)
	{
		SCR_CERR << "VideoDecoder: Decoder not initialized";
		return Result::DecoderBackend_NotInitialized;
	}
	if (!m_registeredSurface)
	{
		SCR_CERR << "VideoDecoder: No registered output surface";
		return Result::DecoderBackend_SurfaceNotRegistered;
	}

	if (CUFAILED(cuGraphicsUnregisterResource(m_registeredSurface)))
	{
		SCR_CERR << "VideoDecoder: Failed to unregister surface";
		return Result::DecoderBackend_InvalidSurface;
	}*/

	m_registeredSurfaceFormat = SurfaceFormat::Unknown;

	return Result::OK;
}

// Note: Alpha is included in the color buffer for the NVidia decoder. Only Android needs the alpha buffer.
Result VideoDecoder::decode(const void* buffer, size_t bufferSizeInBytes, const void* alphaBuffer, size_t alphaBufferSizeInBytes, VideoPayloadType payloadType, bool lastPayload)
{
	//if (!m_parser || !m_decoder)
	//{
	//	return Result::DecoderBackend_NotInitialized;
	//}
	if (!buffer || bufferSizeInBytes == 0)
	{
		return Result::DecoderBackend_InvalidParam;
	}
	if (!lastPayload)
	{
		return Result::DecoderBackend_InvalidPayload;
	}

	return m_displayPictureIndex >= 0 ? Result::DecoderBackend_ReadyToDisplay : Result::OK;
}

Result VideoDecoder::display(bool showAlphaAsColor)
{
	return Result::OK;
}

	