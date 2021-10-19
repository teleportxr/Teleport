// libavstream
// (c) Copyright 2018-2021 Simul Software Ltd

#include "VideoDecoder.h"
#include "Common.h"
#include "Platform/Crossplatform/Texture.h"
#if IS_D3D12
#include "Platform/DirectX12/VideoDecoder.h"
#endif

using namespace avs;

namespace cp = simul::crossplatform;

VideoDecoder::VideoDecoder(cp::RenderPlatform* renderPlatform, cp::Texture* surfaceTexture)
	: m_renderPlatform(renderPlatform)
	, m_surfaceTexture(surfaceTexture)
	, m_outputTexture(nullptr)
{
}

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
	if (device.type != DeviceType::Direct3D12)
	{
		SCR_CERR << "VideoDecoder: Platform library only supports D3D12 video decoder currently." << std::endl;
	}

	cp::VideoDecoderParams decParams;

	switch (params.codec)
	{
	case VideoCodec::H264:
		decParams.codec = cp::VideoCodec::H264;
		break;
	case VideoCodec::HEVC:
		decParams.codec = cp::VideoCodec::HEVC;
		break;
	default:
		SCR_CERR << "VideoDecoder: Unsupported video codec type selected" << std::endl;
		return Result::DecoderBackend_CodecNotSupported;
	}

	decParams.decodeFormat = cp::PixelFormat::NV12;
	decParams.outputFormat = cp::PixelFormat::NV12;
	decParams.bitRate = 0;
	decParams.frameRate = 60;
	decParams.deinterlaceMode = cp::DeinterlaceMode::None;
	decParams.width = frameWidth;
	decParams.height = frameHeight;
	decParams.minWidth = frameWidth;
	decParams.minHeight = frameHeight;
	decParams.maxDecodePictureBufferCount = 20;

#if IS_D3D12
	m_decoder.reset(new simul::dx12::VideoDecoder());
#endif

	if (DEC_FAILED(m_decoder->Initialize(m_renderPlatform, decParams)))
	{
		return Result::DecoderBackend_InitFailed;
	}

	// The output texture is in native decode format.
	// The surface texture will only be written to in the display function.
	m_outputTexture = m_renderPlatform->CreateTexture();
	m_outputTexture->ensureTexture2DSizeAndFormat(m_renderPlatform, decParams.width, decParams.height, simul::crossplatform::RGBA_8_UNORM, true, true, false);

	m_deviceType = device.type;
	m_params = params;
	m_frameWidth = frameWidth;
	m_frameHeight = frameHeight;

	m_arguments.resize(MAX_ARGS);
	m_argCount = 0;

	return Result::OK;
}

Result VideoDecoder::reconfigure(int frameWidth, int frameHeight, const DecoderParams& params)
{
	if (!m_decoder)
	{
		SCR_CERR << "VideoDecoder: Can't reconfigure because decoder not initialized";
		return Result::DecoderBackend_NotInitialized;
	}

	return Result::OK;
}

Result VideoDecoder::shutdown()
{
	m_params = {};
	m_deviceType = DeviceType::Invalid;
	m_frameWidth = 0;
	m_frameHeight = 0;
	m_displayPictureIndex = -1;

	if (m_decoder)
	{
		if (DEC_FAILED(m_decoder->Shutdown()))
		{
			SCR_CERR << "VideoDecoder: Failed to shut down the decoder";
			return Result::DecoderBackend_ShutdownFailed;
		}
	}

	SAFE_DELETE(m_outputTexture);

	return Result::OK;
}

Result VideoDecoder::registerSurface(const SurfaceBackendInterface* surface, const SurfaceBackendInterface* alphaSurface)
{
	if (!m_decoder)
	{
		SCR_CERR << "VideoDecoder: Decoder not initialized";
		return Result::DecoderBackend_NotInitialized;
	}
	if (!surface || !surface->getResource())
	{
		SCR_CERR << "VideoDecoder: Invalid surface handle";
		return Result::DecoderBackend_InvalidSurface;
	}
	if (surface->getWidth() != m_frameWidth || surface->getHeight() != m_frameHeight)
	{
		SCR_CERR << "VideoDecoder: Output surface dimensions do not match video frame dimensions";
		return Result::DecoderBackend_InvalidSurface;
	}

	return Result::OK;
}

Result VideoDecoder::unregisterSurface()
{
	if (!m_decoder)
	{
		SCR_CERR << "VideoDecoder: Decoder not initialized";
		return Result::DecoderBackend_NotInitialized;
	}

	return Result::OK;
}

Result VideoDecoder::decode(const void* buffer, size_t bufferSizeInBytes, const void* alphaBuffer, size_t alphaBufferSizeInBytes, VideoPayloadType payloadType, bool lastPayload)
{
	if (!m_decoder)
	{
		return Result::DecoderBackend_NotInitialized;
	}
	if (!buffer || bufferSizeInBytes == 0)
	{
		return Result::DecoderBackend_InvalidParam;
	}

	switch (payloadType)
	{
	case VideoPayloadType::PPS:
	case VideoPayloadType::SPS:
	case VideoPayloadType::VPS:
	case VideoPayloadType::ALE:
		m_argCount %= MAX_ARGS;
		m_arguments[m_argCount].size = bufferSizeInBytes;
		m_arguments[m_argCount].data = const_cast<void*>(buffer);
		m_arguments[m_argCount].type = cp::VideoDecodeArgumentType::PictureParameters;
		m_argCount++;
		return Result::OK;
	case VideoPayloadType::FirstVCL:
	case VideoPayloadType::VCL:
		break;
	default:
		return Result::DecoderBackend_InvalidPayload;
	}

	if (DEC_FAILED(m_decoder->Decode(m_outputTexture, buffer, bufferSizeInBytes, m_arguments.data(), m_argCount)))
	{
		SCR_CERR << "VideoDecoder: Error occurred while trying to decode the frame.";
		return Result::DecoderBackend_DecodeFailed;
	}

	m_argCount = 0;

	return Result::DecoderBackend_ReadyToDisplay;
}

Result VideoDecoder::display(bool showAlphaAsColor)
{
	return Result::OK;
}

	