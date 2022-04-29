// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#if !defined(PLATFORM_ANDROID)

#include "decoders/dec_nvidia.hpp"
#include "platform.hpp"
#include "logger.hpp"
#include <iostream>

namespace {
#include "dec_nvidia.cubin.inl"
	const char* SYM_surfaceRef = "outputSurfaceRef";
	const char* SYM_kNV12toRGBA = "NV12toRGBA";
	const char* SYM_kNV12toABGR = "NV12toABGR";
	const char* SYM_kAlphaNV12toRGBA = "AlphaNV12toRGBA";
	const char* SYM_kAlphaNV12toABGR = "AlphaNV12toABGR";
	const char* SYM_kNV12toR16 = "NV12toR16";
	const char* SYM_kP016toRGBA = "P016toRGBA";
	const char* SYM_kP016toABGR = "PO16toABGR";
	const char* SYM_kYUV444toRGBA = "YUV444toRGBA";
	const char* SYM_kYUV444toABGR = "YUV444toABGR";
	const char* SYM_kYUV444P16toRGBA = "YUV444P16toRGBA";
	const char* SYM_kYUV444P16toABGR = "YUV444P16toABGR";
}

// CUVID API
static tcuvidCreateVideoParser* cuvidCreateVideoParser;
static tcuvidParseVideoData* cuvidParseVideoData;
static tcuvidDestroyVideoParser* cuvidDestroyVideoParser;
static tcuvidGetDecoderCaps* cuvidGetDecoderCaps;
static tcuvidCreateDecoder* cuvidCreateDecoder;
static tcuvidDestroyDecoder* cuvidDestroyDecoder;
static tcuvidReconfigureDecoder* cuvidReconfigureDecoder;
static tcuvidDecodePicture* cuvidDecodePicture;
static tcuvidMapVideoFrame* cuvidMapVideoFrame;
static tcuvidUnmapVideoFrame* cuvidUnmapVideoFrame;

static const unsigned long numDecodeSurfaces = 20; // 20 is Worst case for H264/HEVC.

namespace avs
{
	// Following is from C:\Video_Codec_SDK_9.0.20\Samples\Utils\ColorSpace.cu
	typedef enum ColorSpaceStandard {
		ColorSpaceStandard_BT709 = 0,
		ColorSpaceStandard_BT601 = 2,
		ColorSpaceStandard_BT2020 = 4
	} ColorSpaceStandard;

	void inline GetConstants(int iMatrix, float &wr, float &wb, int &black, int &white, int &max) {
		// Default is BT709
		wr = 0.2126f; wb = 0.0722f;
		black = 16; white = 235;
		max = 255;
		if (iMatrix == ColorSpaceStandard_BT601) {
			wr = 0.2990f; wb = 0.1140f;
		}
		else if (iMatrix == ColorSpaceStandard_BT2020) {
			wr = 0.2627f; wb = 0.0593f;
			// 10-bit only
			black = 64 << 6; white = 940 << 6;
			max = (1 << 16) - 1;
		}
	}

	void PrintMatYuv2Rgb(int iMatrix) {
		float wr, wb;
		int black, white, max;
		GetConstants(iMatrix, wr, wb, black, white, max);
		float mat[3][3] = {
			1.0f, 0.0f, (1.0f - wr) / 0.5f,
			1.0f, -wb * (1.0f - wb) / 0.5f / (1 - wb - wr), -wr * (1 - wr) / 0.5f / (1 - wb - wr),
			1.0f, (1.0f - wb) / 0.5f, 0.0f,
		};
		char* standard = "";
		switch (iMatrix)
		{
		case ColorSpaceStandard_BT709:
			standard = "BT709";
			break;
		case ColorSpaceStandard_BT601:
			standard = "BT601";
			break;
		case ColorSpaceStandard_BT2020:
			standard = "BT2020";
			break;
		default:
			return;
		}
		std::cout << "YUV to " << standard << " RGB Matrix:" << std::endl;
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				mat[i][j] = (float)(1.0 * max / (white - black) * mat[i][j]);
				if (j < 2)
					std::cout << mat[i][j] << ", ";
				else
					std::cout << mat[i][j];
			}
			std::cout << std::endl;
		}
	}

	LibraryLoader DecoderNV::m_libCUVID("nvcuvid");

	DecoderNV::~DecoderNV()
	{
		m_libCUVID.unload();
	}

	Result DecoderNV::initialize(const DeviceHandle& device, int frameWidth, int frameHeight, const DecoderParams& params)
	{
		m_gResourceSupport = true;

		PrintMatYuv2Rgb(ColorSpaceStandard_BT709);
		PrintMatYuv2Rgb(ColorSpaceStandard_BT2020);

		if (device.type == DeviceType::Invalid)
		{
			AVSLOG(Error) << "DecoderNV: Invalid device handle";
			return Result::DecoderBackend_InvalidDevice;
		}
		if (params.codec == VideoCodec::Invalid)
		{
			AVSLOG(Error) << "DecoderNV: Invalid video codec type";
			return Result::DecoderBackend_InvalidParam;
		}

		cudaVideoCodec videoCodec;
		switch (params.codec)
		{
		case VideoCodec::H264:
			videoCodec = cudaVideoCodec_H264;
			break;
		case VideoCodec::HEVC:
			videoCodec = cudaVideoCodec_HEVC;
			break;
		default:
			AVSLOG(Error) << "DecoderNV: Unsupported video codec type selected";
			return Result::DecoderBackend_CodecNotSupported;
		}

		if (!(CUDA::initialize() && initializeCUVID()))
		{
			return Result::DecoderBackend_InitFailed;
		}

		switch (device.type)
		{
		case DeviceType::Direct3D11:
#if defined(PLATFORM_WINDOWS)
		{
			unsigned int numDevices;
			if (CUFAILED(cuD3D11GetDevices(&numDevices, &m_device, 1, device.as<ID3D11Device>(), CU_D3D11_DEVICE_LIST_ALL)))
			{
				AVSLOG(Error) << "DecoderNV: Supplied DirectX device is not CUDA capable";
				return Result::DecoderBackend_InvalidDevice;
			}
		}
#else
			AVSLOG(Error) << "DecoderNV: DirectX device is only supported on Windows platform";
			return Result::DecoderBackend_InvalidDevice;
#endif
			break;
		case DeviceType::Direct3D12:
#if !defined(PLATFORM_WINDOWS)
			AVSLOG(Error) << "DecoderNV: DirectX 12 is only supported on Windows platform!";
			return Result::DecoderBackend_InvalidDevice;
#endif
			if (!device)
			{
				AVSLOG(Error) << "DecoderNV: Invalid DirectX 12 device";
				return Result::DecoderBackend_InvalidDevice;
			}
#if (LIBAV_USE_D3D12)
			{
				m_dx12Util.Initialize();
				unsigned int numDevices;
				if (CUFAILED(m_dx12Util.GetCudaDevice(&numDevices, &m_device, device.as<ID3D12Device>())))
				{
					AVSLOG(Error) << "DecoderNV: Supplied DirectX 12 device is not CUDA capable";
					return Result::DecoderBackend_InvalidDevice;
				}
			}
#endif
			m_gResourceSupport = false;
			break;
		case DeviceType::OpenGL:
			AVSLOG(Error) << "DecoderNV: OpenGL device support is not implemented.";
			return Result::DecoderBackend_InvalidDevice;
		}

		// TODO: Is blocking sync the right thing to do?
		if (CUFAILED(cuCtxCreate_v2(&m_context, CU_CTX_SCHED_BLOCKING_SYNC, m_device)))
		{
			AVSLOG(Error) << "DecoderNV: Failed to create CUDA context";
			return Result::DecoderBackend_InitFailed;
		}

		// cuCtxCreate already made context current.
		CUDA::ContextGuard ctx(m_context, false);

		if (CUFAILED(cuModuleLoadFatBinary(&m_module, dec_nvidia_cubin)))
		{
			shutdown();
			AVSLOG(Error) << "DecoderNV: Failed to load CUDA post-processing kernels module";
			return Result::DecoderBackend_InitFailed;
		}

		cuModuleGetSurfRef(&m_surfaceRef, m_module, SYM_surfaceRef);
		cuModuleGetFunction(&m_kNV12toRGBA, m_module, SYM_kNV12toRGBA);
		cuModuleGetFunction(&m_kNV12toABGR, m_module, SYM_kNV12toABGR);
		cuModuleGetFunction(&m_kAlphaNV12toRGBA, m_module, SYM_kAlphaNV12toRGBA);
		cuModuleGetFunction(&m_kAlphaNV12toABGR, m_module, SYM_kAlphaNV12toABGR);
		cuModuleGetFunction(&m_kNV12toR16, m_module, SYM_kNV12toR16);
		cuModuleGetFunction(&m_kP016toRGBA, m_module, SYM_kP016toRGBA);
		cuModuleGetFunction(&m_kP016toABGR, m_module, SYM_kP016toABGR);
		cuModuleGetFunction(&m_kYUV444toRGBA, m_module, SYM_kYUV444toRGBA);
		cuModuleGetFunction(&m_kYUV444toABGR, m_module, SYM_kYUV444toABGR);
		cuModuleGetFunction(&m_kYUV444P16toRGBA, m_module, SYM_kYUV444P16toRGBA);
		cuModuleGetFunction(&m_kYUV444P16toABGR, m_module, SYM_kYUV444P16toABGR);

		{
			CUVIDPARSERPARAMS parseParams = {};
			parseParams.CodecType = videoCodec;
			parseParams.ulMaxNumDecodeSurfaces = numDecodeSurfaces;
			parseParams.ulMaxDisplayDelay = 0;
			parseParams.pfnSequenceCallback = DecoderNV::onSequence;
			parseParams.pfnDisplayPicture = DecoderNV::onDisplayPicture;
			parseParams.pfnDecodePicture = DecoderNV::onDecodePicture;
			parseParams.pUserData = this;
			if (CUFAILED(cuvidCreateVideoParser(&m_parser, &parseParams)))
			{
				shutdown();
				AVSLOG(Error) << "DecoderNV: Failed to create video stream parser";
				return Result::DecoderBackend_InitFailed;
			}
		}

		{
			CUVIDDECODECREATEINFO createInfo = {};
			createInfo.ulWidth = frameWidth;
			createInfo.ulHeight = frameHeight;
			createInfo.ulTargetWidth = createInfo.ulWidth;
			createInfo.ulTargetHeight = createInfo.ulHeight;
			createInfo.CodecType = videoCodec;
			if (params.use10BitDecoding)
			{
				if (params.useYUV444ChromaFormat)
				{
					createInfo.ChromaFormat = cudaVideoChromaFormat_444;
					createInfo.OutputFormat = cudaVideoSurfaceFormat_YUV444_16Bit;
				}
				else
				{
					createInfo.ChromaFormat = cudaVideoChromaFormat_420;
					createInfo.OutputFormat = cudaVideoSurfaceFormat_P016;
				}
				createInfo.bitDepthMinus8 = 2;
			}
			else
			{
				if (params.useYUV444ChromaFormat)
				{
					createInfo.ChromaFormat = cudaVideoChromaFormat_444;
					createInfo.OutputFormat = cudaVideoSurfaceFormat_YUV444;
				}
				else
				{
					createInfo.ChromaFormat = cudaVideoChromaFormat_420;
					createInfo.OutputFormat = cudaVideoSurfaceFormat_NV12;
				}
			}
			createInfo.DeinterlaceMode = cudaVideoDeinterlaceMode_Weave;
			createInfo.ulNumDecodeSurfaces = numDecodeSurfaces;
			createInfo.ulNumOutputSurfaces = 1;
			createInfo.display_area = { 0, 0, (short)frameWidth, (short)frameHeight };

			// Check capabiltiies 
			CUVIDDECODECAPS decodeCaps;
			memset(&decodeCaps, 0, sizeof(decodeCaps));

			decodeCaps.eCodecType = createInfo.CodecType;
			decodeCaps.eChromaFormat = createInfo.ChromaFormat;
			decodeCaps.nBitDepthMinus8 = createInfo.bitDepthMinus8;

			if (CUFAILED(cuvidGetDecoderCaps(&decodeCaps)))
			{
				shutdown();
				AVSLOG(Error) << "DecoderNV: Failed to check video decoder capabilities";
				return Result::DecoderBackend_CapabilityCheckFailed;
			}

			if (!decodeCaps.bIsSupported) {
				shutdown();
				AVSLOG(Error) << "DecoderNV: Video codec with provided format is not supported on this GPU";
				return Result::DecoderBackend_CodecNotSupported;
			}

			if (CUFAILED(cuvidCreateDecoder(&m_decoder, &createInfo)))
			{
				shutdown();
				AVSLOG(Error) << "DecoderNV: Failed to create the video decoder";
				return Result::DecoderBackend_InitFailed;
			}
		}

		m_deviceType = device.type;
		m_params = params;
		m_frameWidth = frameWidth;
		m_frameHeight = frameHeight;

		return Result::OK;
	}

	Result DecoderNV::reconfigure(int frameWidth, int frameHeight, const DecoderParams& params)
	{
		if (!m_decoder)
		{
			AVSLOG(Error) << "DecoderNV: Can't reconfigure because decoder not initialized";
			return Result::DecoderBackend_NotInitialized;
		}

		CUVIDRECONFIGUREDECODERINFO reconfigInfo;
		reconfigInfo.ulWidth = frameWidth;
		reconfigInfo.ulHeight = frameHeight;
		reconfigInfo.ulTargetWidth = reconfigInfo.ulWidth;
		reconfigInfo.ulTargetHeight = reconfigInfo.ulHeight;
		reconfigInfo.ulNumDecodeSurfaces = numDecodeSurfaces;
		

		if (CUFAILED(cuvidReconfigureDecoder(&m_decoder, &reconfigInfo)))
		{
			AVSLOG(Error) << "DecoderNV: Failed to reconfigure the video decoder";
			return Result::DecoderBackend_ReconfigFailed;
		}

		return Result::OK;
	}

	Result DecoderNV::shutdown()
	{
		// Not using ContextGuard here since we're about to destroy current context.
		cuCtxPushCurrent(m_context);

		if (m_surfaceRegistered)
		{
			unregisterSurface();
		}

		if (m_decoder)
		{
			cuvidDestroyDecoder(m_decoder);
			m_decoder = nullptr;
		}

		if (m_parser)
		{
			cuvidDestroyVideoParser(m_parser);
			m_parser = nullptr;
		}
		if (m_module)
		{
			cuModuleUnload(m_module);
			m_module = nullptr;
			m_surfaceRef = nullptr;

			m_kNV12toRGBA = nullptr;
			m_kNV12toABGR = nullptr;
			m_kAlphaNV12toRGBA = nullptr;
			m_kAlphaNV12toABGR = nullptr;
			m_kNV12toR16 = nullptr;
			m_kP016toRGBA = nullptr;
			m_kP016toABGR = nullptr;
			m_kYUV444toRGBA = nullptr;
			m_kYUV444toABGR = nullptr;
			m_kYUV444P16toRGBA = nullptr;
			m_kYUV444P16toABGR = nullptr;
		}
		if (m_context)
		{
			cuCtxDestroy_v2(m_context);
			m_context = nullptr;
		}

		m_params = {};
		m_device = 0;
		m_deviceType = DeviceType::Invalid;
		m_frameWidth = 0;
		m_frameHeight = 0;
		m_displayPictureIndex = -1;
		m_surfaceRegistered = false;

		return Result::OK;
	}

	// Note: Alpha surface is not needed for the NVidia decoder because we use a CUDA kernel to write alpha to the color surface.
	Result DecoderNV::registerSurface(const SurfaceBackendInterface* surface, const SurfaceBackendInterface* alphaSurface)
	{
		const unsigned int registerFlags = CU_GRAPHICS_REGISTER_FLAGS_SURFACE_LDST;

		CUDA::ContextGuard ctx(m_context);

		if (!m_decoder)
		{
			AVSLOG(Error) << "DecoderNV: Decoder not initialized";
			return Result::DecoderBackend_NotInitialized;
		}
		if (m_surfaceRegistered)
		{
			AVSLOG(Error) << "DecoderNV: Output surface already registered";
			return Result::DecoderBackend_SurfaceAlreadyRegistered;
		}
		if (!surface || !surface->getResource())
		{
			AVSLOG(Error) << "DecoderNV: Invalid surface handle";
			return Result::DecoderBackend_InvalidSurface;
		}
		if (surface->getWidth() != m_frameWidth || surface->getHeight() != m_frameHeight)
		{
			AVSLOG(Error) << "DeocderNV: Output surface dimensions do not match video frame dimensions";
			return Result::DecoderBackend_InvalidSurface;
		}

		switch (m_deviceType)
		{
#if defined (PLATFORM_WINDOWS)
			case DeviceType::Direct3D11:
				if (CUFAILED(cuGraphicsD3D11RegisterResource(&m_registeredSurface, reinterpret_cast<ID3D11Texture2D*>(surface->getResource()), registerFlags)))
				{
					AVSLOG(Error) << "DecoderNV: Failed to register D3D11 surface with CUDA";
					return Result::DecoderBackend_InvalidSurface;
				}
				cuGraphicsResourceSetMapFlags(m_registeredSurface, CU_GRAPHICS_MAP_RESOURCE_FLAGS_WRITE_DISCARD);
				break;
#endif
#if (LIBAV_USE_D3D12)
			case DeviceType::Direct3D12:
				if (CUFAILED(m_dx12Util.LoadGraphicsResource(reinterpret_cast<ID3D12Resource*>(surface->getResource()))))
				{
					AVSLOG(Error) << "DecoderNV: Failed to register D3D12 surface with CUDA";
					return Result::DecoderBackend_InvalidSurface;
				}
				break;
#endif
			case DeviceType::OpenGL:
				AVSLOG(Error) << "DecoderNV: OpenGL surfaces are not yet supported";
				return Result::DecoderBackend_InvalidSurface;
			default:
				AVSLOG(Error) << "DecoderNV: Cannot register output surface with invalid device type";
				return Result::DecoderBackend_InvalidDevice;
		}

		
		m_registeredSurfaceFormat = surface->getFormat();

		switch (m_registeredSurfaceFormat)
		{
			case SurfaceFormat::ARGB:
				if (m_params.use10BitDecoding)
				{
					m_colorKernel = m_params.useYUV444ChromaFormat ? m_kYUV444P16toRGBA : m_kP016toRGBA;
				}
				else
				{
					m_colorKernel = m_params.useYUV444ChromaFormat ? m_kYUV444toRGBA : m_kNV12toRGBA;
					m_alphaKernel = m_kAlphaNV12toRGBA;
				}
				break;
			case SurfaceFormat::ABGR:
				if (m_params.use10BitDecoding)
				{
					m_colorKernel = m_params.useYUV444ChromaFormat ? m_kYUV444P16toABGR : m_kP016toABGR;
				}
				else
				{
					m_colorKernel = m_params.useYUV444ChromaFormat ? m_kYUV444toABGR : m_kNV12toABGR;
					m_alphaKernel = m_kAlphaNV12toABGR;
				}
				break;
			case SurfaceFormat::R16:
				m_colorKernel = m_kNV12toR16;
				break;
			case SurfaceFormat::NV12:
				/* Not implemented yet. */
				break;
		}

		m_surfaceRegistered = true;
		return Result::OK;
	}

	Result DecoderNV::unregisterSurface()
	{
		CUDA::ContextGuard ctx(m_context);

		if (!m_decoder)
		{
			AVSLOG(Error) << "DecoderNV: Decoder not initialized";
			return Result::DecoderBackend_NotInitialized;
		}
		if (!m_surfaceRegistered)
		{
			AVSLOG(Error) << "DecoderNV: No registered output surface";
			return Result::DecoderBackend_SurfaceNotRegistered;
		}

		if (m_registeredSurface)
		{
			if (CUFAILED(cuGraphicsUnregisterResource(m_registeredSurface)))
			{
				AVSLOG(Error) << "DecoderNV: Failed to unregister surface";
				return Result::DecoderBackend_InvalidSurface;
			}
		}
		else
		{
#if (LIBAV_USE_D3D12)
			if (m_deviceType == DeviceType::Direct3D12)
			{
				m_dx12Util.UnloadGraphicsResource();
			}
#endif
		}

		m_registeredSurface = nullptr;
		m_colorKernel = nullptr;
		m_alphaKernel = nullptr;
		m_registeredSurfaceFormat = SurfaceFormat::Unknown;
		m_surfaceRegistered = false;

		return Result::OK;
	}

	// Note: Alpha is included in the color buffer for the NVidia decoder. Only Android needs the alpha buffer.
	Result DecoderNV::decode(const void* buffer, size_t bufferSizeInBytes, const void* alphaBuffer, size_t alphaBufferSizeInBytes, VideoPayloadType payloadType, bool lastPayload)
	{
		if (!m_parser || !m_decoder)
		{
			return Result::DecoderBackend_NotInitialized;
		}
		if (!buffer || bufferSizeInBytes == 0)
		{
			return Result::DecoderBackend_InvalidParam;
		}
		if (!lastPayload)
		{
			return Result::DecoderBackend_InvalidPayload;
		}

		CUVIDSOURCEDATAPACKET packet = {};
		packet.payload = reinterpret_cast<const unsigned char*>(buffer);
		packet.payload_size = static_cast<unsigned long>(bufferSizeInBytes);

		// This flag removes one frame of lag but only works when there is no alpha layer.
		if (!m_params.useAlphaLayerDecoding)
		{
			packet.flags = CUVID_PKT_ENDOFPICTURE;
		}
		
		if (CUFAILED(cuvidParseVideoData(m_parser, &packet)))
		{
			AVSLOG(Error) << "DecoderNV: Failed to parse data packet";
			return Result::DecoderBackend_ParseFailed;
		}

		return m_displayPictureIndex >= 0 ? Result::DecoderBackend_ReadyToDisplay : Result::OK;
	}

	Result DecoderNV::display(bool showAlphaAsColor)
	{
		CUDA::ContextGuard ctx(m_context);

		if (m_displayPictureIndex < 0)
		{
			return Result::DecoderBackend_DisplayFailed;
		}

		assert(m_colorKernel);

		CUarray surfaceBaseMipLevel = nullptr;
		if (m_gResourceSupport)
		{
			if (CUFAILED(cuGraphicsMapResources(1, &m_registeredSurface, 0)))
			{
				AVSLOG(Warning) << "DecoderNV: Failed to map registered output surface for post processing";
				return Result::DecoderBackend_DisplayFailed;
			}
			cuGraphicsSubResourceGetMappedArray(&surfaceBaseMipLevel, m_registeredSurface, 0, 0);
		}
		else
		{
#if (LIBAV_USE_D3D12)
			if (m_deviceType == DeviceType::Direct3D12)
			{
				CUresult r = m_dx12Util.GetMipmappedArrayLevel(&surfaceBaseMipLevel, 0);
				if (CUFAILED(r))
				{
					AVSLOG(Warning) << "DecoderNV: Failed to map video frame";
					return Result::DecoderBackend_DisplayFailed;
				}
			}
#endif
		}

		assert(surfaceBaseMipLevel);

		cuSurfRefSetArray(m_surfaceRef, surfaceBaseMipLevel, 0);

		int alphaIndex = m_displayPictureIndex + 1;
		int colorIndex = m_params.useAlphaLayerDecoding && showAlphaAsColor ? alphaIndex : m_displayPictureIndex;

		CUdeviceptr frameDevicePtr;
		unsigned int framePitch;

		CUVIDPROCPARAMS procParams = {};
		procParams.progressive_frame = 1;
		if (CUFAILED(cuvidMapVideoFrame(m_decoder, colorIndex, &frameDevicePtr, &framePitch, &procParams)))
		{
			AVSLOG(Warning) << "DecoderNV: Failed to map color video frame";
			cuGraphicsUnmapResources(1, &m_registeredSurface, 0);
			return Result::DecoderBackend_DisplayFailed;
		}

		const unsigned int blockDimX = 32;
		const unsigned int blockDimY = 2;
		const float dimYDiv = m_params.useYUV444ChromaFormat ? 1.f : 2.f;
		const unsigned int gridDimX = (unsigned int)std::ceilf(float(m_frameWidth) / blockDimX / 2.f);
		const unsigned int gridDimY = (unsigned int)std::ceilf(float(m_frameHeight) / blockDimY / dimYDiv);
		
		void* kernelParams[] = 
		{
			&frameDevicePtr,
			&m_frameWidth,
			&m_frameHeight,
			&framePitch,
		};

		Result result = Result::OK;

		if (CUFAILED(cuLaunchKernel(m_colorKernel, gridDimX, gridDimY, 1, blockDimX, blockDimY, 1, 0, 0, kernelParams, nullptr)))
		{
			AVSLOG(Warning) << "DecoderNV: Failed to launch post processing kernel";
			result = Result::DecoderBackend_DisplayFailed;
		}
		else 
		{
			// Wait for kernel to finish.
			cuStreamSynchronize(0);
		}

		cuvidUnmapVideoFrame(m_decoder, frameDevicePtr);

		if (result && m_params.useAlphaLayerDecoding)
		{
			assert(m_alphaKernel);

			// When alpha layer encoding is enabled, the alpha layer will have its own picture index and always 
			// comes after the color picture.
			if (CUFAILED(cuvidMapVideoFrame(m_decoder, alphaIndex, &frameDevicePtr, &framePitch, &procParams)))
			{
				AVSLOG(Warning) << "DecoderNV: Failed to map video frame";
				cuGraphicsUnmapResources(1, &m_registeredSurface, 0);
				return Result::DecoderBackend_DisplayFailed;
			}

			kernelParams[0] = &frameDevicePtr;

			if (CUFAILED(cuLaunchKernel(m_alphaKernel, gridDimX, gridDimY, 1, blockDimX, blockDimY, 1, 0, 0, kernelParams, nullptr)))
			{
				AVSLOG(Warning) << "DecoderNV: Failed to launch post processing kernel";
				result = Result::DecoderBackend_DisplayFailed;
			}
			else {
				// Wait for kernel to finish.
				cuStreamSynchronize(0);
			}

			cuvidUnmapVideoFrame(m_decoder, frameDevicePtr);
		}

		cuGraphicsUnmapResources(1, &m_registeredSurface, 0);

		m_displayPictureIndex = -1;

		return result;
	}

	int CUDAAPI DecoderNV::onSequence(void*, CUVIDEOFORMAT* cuvideoformat)
	{
		AVSLOG(Warning) << "Video Format " << (int)cuvideoformat->video_signal_description.video_format<<"\n";
		AVSLOG(Warning) << "Video full range flag " << (int)cuvideoformat->video_signal_description.video_full_range_flag << "\n";
		AVSLOG(Warning) << "Video color primaries " << (int)cuvideoformat->video_signal_description.color_primaries << "\n";
		AVSLOG(Warning) << "Video transfer_characteristics " << (int)cuvideoformat->video_signal_description.transfer_characteristics << "\n";
		AVSLOG(Warning) << "Video matrix_coefficients " <<(int) cuvideoformat->video_signal_description.matrix_coefficients << "\n";
		AVSLOG(Warning) << "Video codec " << (int)cuvideoformat->codec << "\n";
		return 1;
 	}

	int CUDAAPI DecoderNV::onDisplayPicture(void* userData, CUVIDPARSERDISPINFO* info)
	{
		assert(userData);
		assert(info);

		DecoderNV* self = reinterpret_cast<DecoderNV*>(userData);
		self->m_displayPictureIndex = info->picture_index;
		return 1;
	}

	int CUDAAPI DecoderNV::onDecodePicture(void* userData, CUVIDPICPARAMS* pic)
	{
		assert(userData);
		assert(pic);

		DecoderNV* self = reinterpret_cast<DecoderNV*>(userData);
		if (CUFAILED(cuvidDecodePicture(self->m_decoder, pic)))
		{
			AVSLOG(Warning) << "DecoderNV: Failed to decode picture";
			return 0;
		}
		return 1;
	}

	bool DecoderNV::initializeCUVID()
	{
		ScopedLibraryHandle hLibrary(m_libCUVID);
		if (!hLibrary)
		{
			AVSLOG(Error) << "DecoderNV: CUVID runtime not found";
			return false;
		}

		CUGETPROC(cuvidCreateVideoParser);
		CUGETPROC(cuvidDestroyVideoParser);
		CUGETPROC(cuvidParseVideoData);
		CUGETPROC(cuvidGetDecoderCaps);
		CUGETPROC(cuvidCreateDecoder);
		CUGETPROC(cuvidReconfigureDecoder);
		CUGETPROC(cuvidDestroyDecoder);
		CUGETPROC(cuvidDecodePicture);
#ifdef PLATFORM_64BIT
		CUGETPROC_EX(cuvidMapVideoFrame, "cuvidMapVideoFrame64");
		CUGETPROC_EX(cuvidUnmapVideoFrame, "cuvidUnmapVideoFrame64");
#else
		CUGETPROC_EX(cuvidMapVideoFrame, "cuvidMapVideoFrame");
		CUGETPROC_EX(cuvidUnmapVideoFrame, "cuvidUnmapVideoFrame");
#endif

		hLibrary.take();
		return true;
	}

} // avs

#endif // !PLATFORM_ANDROID