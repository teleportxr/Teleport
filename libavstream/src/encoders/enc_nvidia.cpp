// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#if !defined(PLATFORM_ANDROID)

#include <algorithm>
#include <map>

#include <common_p.hpp>
#include <encoders/enc_nvidia.hpp>

#include <libavstream/surfaces/surface_dx11.hpp>
#include <libavstream/surfaces/surface_dx12.hpp>

/**  
    Low-latency use cases like game-streaming, video conferencing etc.

	1. Ultra-low latency or low latency Tuning Info
	2. Rate control mode = CBR
	3. Multi Pass – Quarter/Full (evaluate and decide)
	4. Very low VBV buffer size (e.g. single frame = bitrate/framerate)
	5. No B Frames
	6. Infinite GOP length
	7. Adaptive quantization (AQ) enabled**
	8. Long term reference pictures***
	9. Intra refresh***
	10. Non-reference P frames***
	11. Force IDR***
*/

namespace
{
#include "enc_nvidia.cubin.inl"
	const char* SYM_surfaceRef = "inputSurfaceRef";

	const char* SYM_kCopyPixels = "CopyPixels";
	const char* SYM_kCopyPixelsSwapRB = "CopyPixelsSwapRB";
	const char* SYM_kCopyPixels16 = "CopyPixels16";
	const char* SYM_kCopyPixels16SwapRB = "CopyPixels16SwapRB";

	const char* SYM_kRGBAtoNV12 = "RGBAtoNV12";
	const char* SYM_kBGRAtoNV12 = "BGRAtoNV12";
	const char* SYM_kR16toNV12 = "R16toNV12";
}

static NVENCSTATUS g_nvstatus;
#define NVFAILED(x) ((g_nvstatus = (x)) != NV_ENC_SUCCESS)

namespace avs
{
	
	namespace
	{
		NV_ENCODE_API_FUNCTION_LIST g_api;
	}
	
	LibraryLoader EncoderNV::m_libNVENC(
#if defined(PLATFORM_64BIT)
		"nvEncodeAPI64"
#else
		"nvEncodeAPI"
#endif
	);

	EncoderNV::~EncoderNV()
	{
		shutdown();
		releaseEncodeLibrary();
	}

	Result EncoderNV::initialize(const DeviceHandle& device, int frameWidth, int frameHeight, const EncoderParams& params)
	{
		m_gResourceSupport = true;

		if (device.type == DeviceType::Invalid)
		{
			AVSLOG(Error) << "EncoderNV: Invalid device handle";
			return Result::EncoderBackend_InvalidDevice;
		}

		if (!CUDA::initialize() || !initializeEncodeLibrary())
		{
			return Result::EncoderBackend_InitFailed;
		}

		CUdevice cudaDevice;
		unsigned int numDevices;
		switch (device.type)
		{
		case DeviceType::Direct3D11:
#if !defined(PLATFORM_WINDOWS)
			AVSLOG(Error) << "EncoderNV: DirectX 11 is only supported on Windows platform";
			return Result::EncoderBackend_InvalidDevice;
#endif
			if (!device)
			{
				AVSLOG(Error) << "EncoderNV: Invalid DirectX 11 device";
				return Result::EncoderBackend_InvalidDevice;
			}
			{
				
				if (CUFAILED(cuD3D11GetDevices(&numDevices, &cudaDevice, 1, device.as<ID3D11Device>(), CU_D3D11_DEVICE_LIST_ALL)))
				{
					AVSLOG(Error) << "EncoderNV: Supplied DirectX 11 device is not CUDA capable";
					return Result::EncoderBackend_InvalidDevice;
				}
			}
			break;
		case DeviceType::Direct3D12:
#if !defined(PLATFORM_WINDOWS)
			AVSLOG(Error) << "EncoderNV: DirectX 12 is only supported on Windows platform";
			return Result::EncoderBackend_InvalidDevice;
#endif
			if (!device)
			{
				AVSLOG(Error) << "EncoderNV: Invalid DirectX 12 device";
				return Result::EncoderBackend_InvalidDevice;
			}
			{
				m_dx12Util.Initialize();
				if (CUFAILED(m_dx12Util.GetCudaDevice(&numDevices, &cudaDevice, device.as<ID3D12Device>())))
				{
					AVSLOG(Error) << "EncoderNV: Supplied DirectX 12 device is not CUDA capable";
					return Result::EncoderBackend_InvalidDevice;
				}
			}
			m_gResourceSupport = false;
			break;
		case DeviceType::OpenGL:
			// TODO: Implement getting CUDA device.
			AVSLOG(Error) << "EncoderNV: OpenGL device is not yet supported";
			return Result::EncoderBackend_InvalidDevice;
		default:
			AVSLOG(Error) << "EncoderNV: Unsupported device";
			return Result::EncoderBackend_InvalidDevice;
		}

		// TODO: Is blocking sync the right thing to do?
		CUresult c = cuCtxCreate_v2(&m_context, CU_CTX_SCHED_BLOCKING_SYNC, cudaDevice);
		if (CUFAILED(c))
		{
			AVSLOG(Error) << "EncoderNV: Failed to create CUDA context";
			return Result::EncoderBackend_InitFailed;
		}

		// cuCtxCreate already made context current.
		CUDA::ContextGuard ctx(m_context, false);

		if (CUFAILED(cuModuleLoadFatBinary(&m_module, enc_nvidia_cubin)))
		{
			shutdown();
			AVSLOG(Error) << "EncoderNV: Failed to load CUDA pre-processing kernels module";
			return Result::EncoderBackend_InitFailed;
		}

		cuModuleGetSurfRef(&m_surfaceRef, m_module, SYM_surfaceRef);
		assert(m_surfaceRef);

		cuModuleGetFunction(&m_kCopyPixels, m_module, SYM_kCopyPixels);
		assert(m_kCopyPixels);
		cuModuleGetFunction(&m_kCopyPixelsSwapRB, m_module, SYM_kCopyPixelsSwapRB);
		assert(m_kCopyPixelsSwapRB);
		cuModuleGetFunction(&m_kCopyPixels16, m_module, SYM_kCopyPixels16);
		assert(m_kCopyPixels16);
		cuModuleGetFunction(&m_kCopyPixels16SwapRB, m_module, SYM_kCopyPixels16SwapRB);
		assert(m_kCopyPixels16SwapRB);
		cuModuleGetFunction(&m_kRGBAtoNV12, m_module, SYM_kRGBAtoNV12);
		assert(m_kRGBAtoNV12);
		cuModuleGetFunction(&m_kBGRAtoNV12, m_module, SYM_kBGRAtoNV12);
		assert(m_kBGRAtoNV12);
		cuModuleGetFunction(&m_kR16toNV12, m_module, SYM_kR16toNV12);
		assert(m_kR16toNV12);

		{
			NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS params = {};
			params.apiVersion = NVENCAPI_VERSION;
			params.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
			params.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
			params.device = m_context;
			if (NVFAILED(g_api.nvEncOpenEncodeSessionEx(&params, &m_encoder)))
			{
				shutdown();
				AVSLOG(Error) << "EncoderNV: Failed to open NVENC encode session - error is "<< g_nvstatus;
				return Result::EncoderBackend_InitFailed;
			}
		}

		Result result = initializeEncoder(frameWidth, frameHeight, params);
		if (!result)
		{
			return result;
		}

		assert(device);

#if defined(PLATFORM_WINDOWS)
		if (device.type == DeviceType::Direct3D11)
		{
			device.as<ID3D11Device>()->AddRef();
		}
		else if (device.type == DeviceType::Direct3D12)
		{
			device.as<ID3D12Device>()->AddRef();
		}

#endif

		m_device = device;

		return Result::OK;
	}

	Result EncoderNV::initializeEncoder(int frameWidth, int frameHeight, const EncoderParams& params)
	{
		GUID requestedEncodeGUID = { 0 };
		switch (params.codec)
		{
		case VideoCodec::H264:
			requestedEncodeGUID = NV_ENC_CODEC_H264_GUID;
			break;
		case VideoCodec::HEVC:
			requestedEncodeGUID = NV_ENC_CODEC_HEVC_GUID;
			break;
		}

		GUID requestedPresetGUID = NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID;
		switch (params.preset)
		{
		case VideoPreset::HighPerformance:
			requestedPresetGUID = NV_ENC_PRESET_LOW_LATENCY_HP_GUID;
			break;
		case VideoPreset::HighQuality:
			requestedPresetGUID = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;
			break;
		}

		NV_ENC_BUFFER_FORMAT requestedFormat = NV_ENC_BUFFER_FORMAT_UNDEFINED;
		switch (params.inputFormat)
		{
		case SurfaceFormat::ABGR:
			requestedFormat = NV_ENC_BUFFER_FORMAT_ABGR;
			break;
		case SurfaceFormat::ARGB:
			requestedFormat = NV_ENC_BUFFER_FORMAT_ARGB;
			break;
		case SurfaceFormat::NV12:
			requestedFormat = NV_ENC_BUFFER_FORMAT_NV12;
			break;
		case SurfaceFormat::ARGB10:
			requestedFormat = NV_ENC_BUFFER_FORMAT_ARGB10;
			break;
		case SurfaceFormat::ABGR10:
			requestedFormat = NV_ENC_BUFFER_FORMAT_ABGR10;
			break;
		}

		uint32_t frameRateDen = 1;

		EncodeConfig config;
		{
			Result result = chooseEncodeConfig(requestedEncodeGUID, requestedPresetGUID, requestedFormat, config);
			if (!result)
			{
				return result;
			}
		}

		// Check the highest encoding level supported
		{
			NV_ENC_CAPS_PARAM capsParam;
			capsParam.version = NV_ENC_CAPS_PARAM_VER;
			capsParam.capsToQuery = NV_ENC_CAPS_LEVEL_MAX;

			// m_encoder not initialized here yet but function just needs the encode GUID.
			if (NVFAILED(g_api.nvEncGetEncodeCaps(m_encoder, config.encodeGUID, &capsParam, (int*)&m_EncodeCapabilities.maxEncodeLevel)))
			{
				shutdown();
				AVSLOG(Error) << "EncoderNV: Failed to check encoder capability";
				return Result::EncoderBackend_CapabilityCheckFailed;
			}
		}

		// Check if async mode is supported
		{
			NV_ENC_CAPS_PARAM capsParam;
			capsParam.version = NV_ENC_CAPS_PARAM_VER;
			capsParam.capsToQuery = NV_ENC_CAPS_ASYNC_ENCODE_SUPPORT;

			// m_encoder not initialized here yet but function just needs the encode GUID.
			if (NVFAILED(g_api.nvEncGetEncodeCaps(m_encoder, config.encodeGUID, &capsParam, (int*)&m_EncodeCapabilities.isAsyncCapable)))
			{
				shutdown();
				AVSLOG(Error) << "EncoderNV: Failed to check encoder capability";
				return Result::EncoderBackend_CapabilityCheckFailed;
			}
		}

		// Check if 10-bit encoding is supported
		{
			NV_ENC_CAPS_PARAM capsParam;
			capsParam.version = NV_ENC_CAPS_PARAM_VER;
			capsParam.capsToQuery = NV_ENC_CAPS_SUPPORT_10BIT_ENCODE;

			// m_encoder not initialized here yet but function just needs the encode GUID.
			if (NVFAILED(g_api.nvEncGetEncodeCaps(m_encoder, config.encodeGUID, &capsParam, (int*)&m_EncodeCapabilities.is10BitCapable)))
			{
				shutdown();
				AVSLOG(Error) << "EncoderNV: Failed to check encoder capability";
				return Result::EncoderBackend_CapabilityCheckFailed;
			}
		}

		// Check if YUV444 encoding is supported
		{
			NV_ENC_CAPS_PARAM capsParam;
			capsParam.version = NV_ENC_CAPS_PARAM_VER;
			capsParam.capsToQuery = NV_ENC_CAPS_SUPPORT_YUV444_ENCODE;

			// m_encoder not initialized here yet but function just needs the encode GUID.
			if (NVFAILED(g_api.nvEncGetEncodeCaps(m_encoder, config.encodeGUID, &capsParam, (int*)&m_EncodeCapabilities.isYUV444Capable)))
			{
				shutdown();
				AVSLOG(Error) << "EncoderNV: Failed to check encoder capability";
				return Result::EncoderBackend_CapabilityCheckFailed;
			}
		}

		// Check if alpha layer encoding is supported
		{
			NV_ENC_CAPS_PARAM capsParam;
			capsParam.version = NV_ENC_CAPS_PARAM_VER;
			capsParam.capsToQuery = NV_ENC_CAPS_SUPPORT_ALPHA_LAYER_ENCODING;

			// m_encoder not initialized here yet but function just needs the encode GUID.
			if (NVFAILED(g_api.nvEncGetEncodeCaps(m_encoder, config.encodeGUID, &capsParam, (int*)&m_EncodeCapabilities.isAlphaLayerSupported)))
			{
				shutdown();
				AVSLOG(Error) << "EncoderNV: Failed to check encoder capability";
				return Result::EncoderBackend_CapabilityCheckFailed;
			}
		}

		{
			// Set GOP length
			if (params.idrInterval == 0)
			{
				// IDR frames will never be inserted automatically by the encoder
				config.config.gopLength = NVENC_INFINITE_GOPLENGTH;
				config.config.frameIntervalP = 1;
			}
			else
			{
				config.config.gopLength = params.idrInterval;
			}

			auto& codecConfig = config.config.encodeCodecConfig;
			if (config.encodeGUID == NV_ENC_CODEC_H264_GUID)
			{
				codecConfig.h264Config.level = NV_ENC_LEVEL_AUTOSELECT;
				codecConfig.h264Config.repeatSPSPPS = 1;
				codecConfig.h264Config.idrPeriod = config.config.gopLength;

				// Only relevant if the input buffer format is a YUV format. Set to 1 for YUV420.
				if (params.useYUV444ChromaFormat && m_EncodeCapabilities.isYUV444Capable)
				{
					codecConfig.h264Config.chromaFormatIDC = 3;
				}
			}
			if (config.encodeGUID == NV_ENC_CODEC_HEVC_GUID)
			{
				codecConfig.hevcConfig.level = NV_ENC_LEVEL_AUTOSELECT;
				codecConfig.hevcConfig.repeatSPSPPS = 1;
				codecConfig.hevcConfig.idrPeriod = config.config.gopLength;

				// 10-bit and YUV-444 are not supported with alpha encoding
				if (params.useAlphaLayerEncoding && m_EncodeCapabilities.isAlphaLayerSupported)
				{
					codecConfig.hevcConfig.enableAlphaLayerEncoding = 1;
					config.config.rcParams.alphaLayerBitrateRatio = 3;
				}
				else
				{
					if (params.use10BitEncoding && m_EncodeCapabilities.is10BitCapable)
					{
						codecConfig.hevcConfig.pixelBitDepthMinus8 = 2;
						config.config.profileGUID = NV_ENC_HEVC_PROFILE_MAIN10_GUID;
					}

					// Only relevant if the inpout buffer format is a YUV format. Set to 1 for YUV420.
					if (params.useYUV444ChromaFormat && m_EncodeCapabilities.isYUV444Capable)
					{
						codecConfig.hevcConfig.chromaFormatIDC = 3;
					}
				}
			}
		}

		switch (params.rateControlMode)
		{
		case RateControlMode::RC_CONSTQP:
			config.config.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CONSTQP;
			break;
		case RateControlMode::RC_VBR:
			config.config.rcParams.rateControlMode = NV_ENC_PARAMS_RC_VBR;
			break;
		case RateControlMode::RC_CBR:
			config.config.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
			break;
		case RateControlMode::RC_CBR_LOWDELAY_HQ:
			config.config.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;
			break;
		case RateControlMode::RC_CBR_HQ:
			config.config.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR_HQ;
			break;
		case RateControlMode::RC_VBR_HQ:
			config.config.rcParams.rateControlMode = NV_ENC_PARAMS_RC_VBR_HQ;
			break;
		}

		if (params.autoBitRate)
		{
			config.config.rcParams.averageBitRate = (static_cast<unsigned int>(frameWidth * frameHeight) / (1280 * 720)) * 20000000;
			config.config.rcParams.averageBitRate = std::min(config.config.rcParams.averageBitRate, uint32_t(60000000));
			config.config.rcParams.maxBitRate = (uint32_t)(config.config.rcParams.averageBitRate * 1.5f);
		}
		else
		{
			if (params.averageBitrate > 0)
			{
				config.config.rcParams.averageBitRate = params.averageBitrate;
				config.config.rcParams.maxBitRate = config.config.rcParams.averageBitRate;
			}
			if (params.maxBitrate > params.averageBitrate)
			{
				config.config.rcParams.maxBitRate = params.maxBitrate;
			}
		}

		if (params.vbvBufferSizeInFrames > 0)
		{
			config.config.rcParams.vbvBufferSize = (config.config.rcParams.maxBitRate * frameRateDen / params.targetFrameRate) * params.vbvBufferSizeInFrames; // bitrate / framerate = one frame
			config.config.rcParams.vbvInitialDelay = config.config.rcParams.vbvBufferSize;
		}

		{
			NV_ENC_INITIALIZE_PARAMS initializeParams = {};
			initializeParams.version = NV_ENC_INITIALIZE_PARAMS_VER;
			initializeParams.encodeGUID = config.encodeGUID;
			initializeParams.presetGUID = config.presetGUID;
			initializeParams.encodeWidth = frameWidth;
			initializeParams.encodeHeight = frameHeight;
			initializeParams.maxEncodeWidth = initializeParams.encodeWidth;
			initializeParams.maxEncodeHeight = initializeParams.encodeHeight;
			initializeParams.darWidth = initializeParams.encodeWidth;
			initializeParams.darHeight = initializeParams.encodeHeight;
			initializeParams.frameRateNum = params.targetFrameRate;
			initializeParams.frameRateDen = frameRateDen;
			initializeParams.enablePTD = true;
			initializeParams.reportSliceOffsets = 0;
			initializeParams.enableSubFrameWrite = 0;
			initializeParams.privDataSize = 0;
			initializeParams.enableExternalMEHints = 0;
			initializeParams.encodeConfig = &config.config;



#if defined(PLATFORM_WINDOWS)
			initializeParams.enableEncodeAsync = params.useAsyncEncoding && m_EncodeCapabilities.isAsyncCapable;
#else
			initializeParams.enableEncodeAsync = false;
#endif

			if (m_initialized)
			{
				NV_ENC_RECONFIGURE_PARAMS reInitEncodeParams;
				reInitEncodeParams.forceIDR = true;
				reInitEncodeParams.reInitEncodeParams = initializeParams;
				reInitEncodeParams.resetEncoder = true;	
				reInitEncodeParams.version = NV_ENC_RECONFIGURE_PARAMS_VER;

				if (NVFAILED(g_api.nvEncReconfigureEncoder(m_encoder, &reInitEncodeParams)))
				{
					shutdown();
					AVSLOG(Error) << "EncoderNV: Failed to reconfigure hardware encoder";
					return Result::EncoderBackend_ReconfigFailed;
				}
			}
			else
			{
				if (NVFAILED(g_api.nvEncInitializeEncoder(m_encoder, &initializeParams)))
				{
					shutdown();
					AVSLOG(Error) << "EncoderNV: Failed to initialize hardware encoder";
					return Result::EncoderBackend_InitFailed;
				}

				if (initializeParams.enableEncodeAsync)
				{
					m_completionEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
					NV_ENC_EVENT_PARAMS eventParams = { NV_ENC_EVENT_PARAMS_VER };
					eventParams.completionEvent = m_completionEvent;
					g_api.nvEncRegisterAsyncEvent(m_encoder, &eventParams);
				}

				{
					NV_ENC_CREATE_BITSTREAM_BUFFER params = {};
					params.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
					if (NVFAILED(g_api.nvEncCreateBitstreamBuffer(m_encoder, &params)))
					{
						shutdown();
						AVSLOG(Error) << "EncoderNV: Failed to create output bitstream buffer";
						return Result::EncoderBackend_OutOfMemory;
					}
					m_outputBuffer.ptr = params.bitstreamBuffer;
				}

				m_initialized = true;
			}		
		}

		m_inputBuffer.format = config.format;

		m_params = params;
		m_params.useAsyncEncoding = params.useAsyncEncoding && m_EncodeCapabilities.isAsyncCapable;
		m_params.use10BitEncoding = params.use10BitEncoding && m_EncodeCapabilities.is10BitCapable;
		m_params.useYUV444ChromaFormat &= (m_EncodeCapabilities.isYUV444Capable != 0);
		m_params.useAlphaLayerEncoding &= (m_EncodeCapabilities.isAlphaLayerSupported != 0);
		m_params.idrInterval = config.config.gopLength;

		return Result::OK;
	}

	Result EncoderNV::reconfigure(int frameWidth, int frameHeight, const EncoderParams& params)
	{
		if (!m_initialized || !m_encoder)
		{
			AVSLOG(Error) << "EncoderNV: Encoder not initialized";
			return Result::EncoderBackend_NotInitialized;
		}

		return initializeEncoder(frameWidth, frameHeight, params);
	}

	Result EncoderNV::shutdown()
	{
		Result result = Result::OK;

		// Not using ContextGuard here since we're about to destroy current context.
		if (m_context)
		{
			cuCtxPushCurrent(m_context);
		}

		if (m_encoder)
		{
			NV_ENC_PIC_PARAMS flushParams = {};
			flushParams.version = NV_ENC_PIC_PARAMS_VER;
			flushParams.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
			NVENCSTATUS status = g_api.nvEncEncodePicture(m_encoder, &flushParams);
			if (NVFAILED(status))
			{
				AVSLOG(Warning) << "EncoderNV: Failed to flush hardware encoder\n";
				result = Result::EncoderBackend_FlushError;
			}
			if (m_registeredSurface.surface)
			{
				result = unregisterSurface();
			}
#if defined(PLATFORM_WINDOWS)
			if (m_completionEvent)
			{
				NV_ENC_EVENT_PARAMS eventParams = { NV_ENC_EVENT_PARAMS_VER };
				eventParams.completionEvent = m_completionEvent;
				g_api.nvEncUnregisterAsyncEvent(m_encoder, &eventParams);
				CloseHandle(m_completionEvent);
			}
#endif
			if (m_outputBuffer.ptr)
			{
				if (NVFAILED(g_api.nvEncDestroyBitstreamBuffer(m_encoder, m_outputBuffer.ptr)))
				{
					AVSLOG(Error) << "EncoderNV: Failed to destroy output bitstream buffer\n";
					result = Result::EncoderBackend_ShutdownFailed;
				}
				m_outputBuffer = {};
			}
			if (NVFAILED(g_api.nvEncDestroyEncoder(m_encoder)))
			{
				AVSLOG(Error) << "EncoderNV: Failed to destroy hardware encoder";
				result = Result::EncoderBackend_ShutdownFailed;
			}
		}

		if (m_module)
		{
			cuModuleUnload(m_module);
			m_module = nullptr;
			m_surfaceRef = nullptr;
			m_kR16toNV12 = nullptr;
		}
		if (m_context)
		{
			cuCtxDestroy_v2(m_context);
			m_context = nullptr;
		}

#if defined(PLATFORM_WINDOWS)
		if (m_device)
		{
			if (m_device.type == DeviceType::Direct3D11)
			{
				m_device.as<ID3D11Device>()->Release();
			}
			else if (m_device.type == DeviceType::Direct3D12)
			{
				m_device.as<ID3D12Device>()->Release();
			}
		}		
#endif

		m_params = {};
		m_device = {};
		m_encoder = nullptr;
		m_initialized = false;

		return result;
	}

	Result EncoderNV::registerSurface(const SurfaceBackendInterface* surface)
	{
		const unsigned int registerFlags = CU_GRAPHICS_REGISTER_FLAGS_SURFACE_LDST;

		if (!m_encoder)
		{
			AVSLOG(Error) << "EncoderNV: Encoder not initialized";
			return Result::EncoderBackend_NotInitialized;
		}
		if (m_registeredSurface.surface)
		{
			AVSLOG(Error) << "EncoderNV: Input surface already registered";
			return Result::EncoderBackend_SurfaceAlreadyRegistered;
		}
		if (!surface)
		{
			AVSLOG(Error) << "EncoderNV: Invalid surface handle";
			return Result::EncoderBackend_InvalidSurface;
		}

		CUDA::ContextGuard ctx(m_context);

		CUresult res;
		switch (m_device.type)
		{
#if PLATFORM_WINDOWS
		case DeviceType::Direct3D11:
			res = cuGraphicsD3D11RegisterResource(&m_registeredSurface.resource, reinterpret_cast<ID3D11Texture2D*>(surface->getResource()), registerFlags);
			if (CUFAILED(res))
			{
				AVSLOG(Error) << "EncoderNV: Failed to register D3D11 surface with CUDA";
				return Result::EncoderBackend_InvalidSurface;
			}
			break;
		case DeviceType::Direct3D12:
			if (CUFAILED(m_dx12Util.LoadGraphicsResource(reinterpret_cast<ID3D12Resource*>(surface->getResource()))))
			{
				AVSLOG(Error) << "EncoderNV: Failed to register D3D12 surface with CUDA";
				return Result::EncoderBackend_InvalidSurface;
			}
			break;
#endif
		case DeviceType::OpenGL:
			AVSLOG(Error) << "EncoderNV: OpenGL surfaces are not yet supported";
			return Result::EncoderBackend_InvalidSurface;
		default:
			AVSLOG(Error) << "EncoderNV: Cannot register surface with invalid device type";
			return Result::EncoderBackend_InvalidDevice;
		}

		
		assert(!m_gResourceSupport || m_registeredSurface.resource);
		assert(m_inputBuffer.ptr == nullptr);
		assert(m_inputBuffer.devicePtr == 0);

		size_t inputBufferWidthInBytes = 0;
		size_t inputBufferHeight = 0;
		const SurfaceFormat surfaceFormat = surface->getFormat();

		uint32_t arbgBytesPerPixel = 4;

		switch (m_inputBuffer.format)
		{
		case NV_ENC_BUFFER_FORMAT_ARGB:
		case NV_ENC_BUFFER_FORMAT_ABGR:
		case NV_ENC_BUFFER_FORMAT_ABGR10:
		case NV_ENC_BUFFER_FORMAT_ARGB10:
			inputBufferWidthInBytes = surface->getWidth() * (size_t)arbgBytesPerPixel;
			inputBufferHeight = surface->getHeight();
			break;
		case NV_ENC_BUFFER_FORMAT_NV12:
			inputBufferWidthInBytes = surface->getWidth();
			inputBufferHeight = (size_t)surface->getHeight() + (surface->getHeight() + 1) / 2;
			break;
		default:
			assert(false);
		}

		CUresult r = cuMemAllocPitch_v2(&m_inputBuffer.devicePtr, &m_inputBuffer.pitch, inputBufferWidthInBytes, inputBufferHeight, 16);
		if (CUFAILED(r))
		{
			if (m_gResourceSupport)
			{
				cuGraphicsUnregisterResource(m_registeredSurface.resource);
				m_registeredSurface.resource = nullptr;
			}		
			AVSLOG(Error) << "EncoderNV: Failed to allocate input buffer in device memory";
			return Result::EncoderBackend_OutOfMemory;
		}

		NV_ENC_REGISTER_RESOURCE resource = {};
		resource.version = NV_ENC_REGISTER_RESOURCE_VER;
		resource.bufferFormat = m_inputBuffer.format;
		resource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR;
		resource.resourceToRegister = reinterpret_cast<void*>(m_inputBuffer.devicePtr);
		resource.width = surface->getWidth();
		resource.height = surface->getHeight();
		resource.pitch = uint32_t(m_inputBuffer.pitch);

		NVENCSTATUS status = g_api.nvEncRegisterResource(m_encoder, &resource);
		if (NVFAILED(status))
		{
			AVSLOG(Error) << "EncoderNV: Failed to register surface";
			return Result::EncoderBackend_InvalidSurface;
		}

		if (m_gResourceSupport)
		{
			cuGraphicsResourceSetMapFlags(m_registeredSurface.resource, CU_GRAPHICS_MAP_RESOURCE_FLAGS_READ_ONLY);
		}

		m_registeredSurface.surface = surface;
		m_inputBuffer.ptr = resource.registeredResource;

		return Result::OK;
	}

	Result EncoderNV::unregisterSurface()
	{
		if (!m_encoder)
		{
			AVSLOG(Error) << "EncoderNV: Encoder not initialized";
			return Result::EncoderBackend_NotInitialized;
		}
		if (!m_registeredSurface.surface)
		{
			AVSLOG(Warning) << "EncoderNV: No registered input surface";
			return Result::EncoderBackend_SurfaceNotRegistered;
		}

		CUDA::ContextGuard ctx(m_context);

		
		if (m_registeredSurface.resource)
		{
			if (CUFAILED(cuGraphicsUnregisterResource(m_registeredSurface.resource)))
			{
				AVSLOG(Warning) << "EncoderNV: Failed to unregister surface with CUDA";
			}
			m_registeredSurface.resource = nullptr;
		}
		else
		{
#if defined(PLATFORM_WINDOWS)
			if (m_device.type == DeviceType::Direct3D12)
			{
				m_dx12Util.UnloadGraphicsResource();
			}
#endif
		}
		if (m_inputBuffer.ptr)
		{
			if (NVFAILED(g_api.nvEncUnregisterResource(m_encoder, m_inputBuffer.ptr)))
			{
				AVSLOG(Error) << "EncoderNV: Failed to unregister surface";
				return Result::EncoderBackend_InvalidSurface;
			}
			m_inputBuffer.ptr = nullptr;
		}
		if (m_inputBuffer.devicePtr)
		{
			cuMemFree_v2(m_inputBuffer.devicePtr);
			m_inputBuffer.devicePtr = 0;
			m_inputBuffer.pitch = 0;
		}

		return Result::OK;
	}

	Result EncoderNV::encodeFrame(uint64_t timestamp, bool forceIDR)
	{
		if (!m_encoder)
		{
			AVSLOG(Error) << "EncoderNV: Encoder not initialized";
			return Result::EncoderBackend_NotInitialized;
		}
		if (!m_registeredSurface.surface)
		{
			AVSLOG(Error) << "EncoderNV: No registered input surface";
			return Result::EncoderBackend_SurfaceNotRegistered;
		}

		Result result = Result::OK;
		if (!(result = processInput()))
		{
			return result;
		}

		m_resource = {};
		m_resource.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
		m_resource.registeredResource = m_inputBuffer.ptr;
		if (NVFAILED(g_api.nvEncMapInputResource(m_encoder, &m_resource)))
		{
			AVSLOG(Error) << "EncoderNV: Failed to map input surface";
			return Result::EncoderBackend_MapFailed;
		}

		NV_ENC_PIC_PARAMS encParams = {};
		encParams.version = NV_ENC_PIC_PARAMS_VER;
		encParams.bufferFmt = m_resource.mappedBufferFmt;
		encParams.inputBuffer = m_resource.mappedResource;
		// TODO: alphaBuffer should contain alpha data for NV12. Contained in inputBuffer for ARGB.
		encParams.alphaBuffer = nullptr;
		encParams.outputBitstream = m_outputBuffer.ptr;
		encParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
		encParams.inputWidth = m_registeredSurface.surface->getWidth();
		encParams.inputHeight = m_registeredSurface.surface->getHeight();
		encParams.inputPitch = uint32_t(m_inputBuffer.pitch);
		encParams.inputTimeStamp = timestamp;
		encParams.completionEvent = m_completionEvent;
		if (forceIDR)
			encParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR | NV_ENC_PIC_FLAG_OUTPUT_SPSPPS;

		if (NVFAILED(g_api.nvEncEncodePicture(m_encoder, &encParams)))
		{
			AVSLOG(Error) << "EncoderNV: Failed to encode frame";
			result = Result::EncoderBackend_EncodeFailed;
		}

		if (!m_completionEvent)
		{
			if (NVFAILED(g_api.nvEncUnmapInputResource(m_encoder, m_resource.mappedResource)))
			{
				AVSLOG(Error) << "EncoderNV: Failed to unmap input surface";
				if (result)
				{
					result = Result::EncoderBackend_UnmapFailed;
				}
			}
		}
		return result;
	}

	Result EncoderNV::processInput()
	{
		CUDA::ContextGuard ctx(m_context);

		assert(m_registeredSurface.surface);
		assert(!m_gResourceSupport || m_registeredSurface.resource);
		assert(m_inputBuffer.devicePtr);

		float gridDimDivisor = 1.0f;
		CUfunction ppKernel = nullptr;

		const SurfaceFormat surfaceFormat = m_registeredSurface.surface->getFormat();
		int width_mult = 1;
		switch (m_inputBuffer.format)
		{
		case NV_ENC_BUFFER_FORMAT_NV12:
			gridDimDivisor = 2.0f;
			if (surfaceFormat == SurfaceFormat::ARGB) ppKernel = m_kRGBAtoNV12;
			if (surfaceFormat == SurfaceFormat::ABGR) ppKernel = m_kBGRAtoNV12;
			if (surfaceFormat == SurfaceFormat::R16)  ppKernel = m_kR16toNV12;
			break;
		case NV_ENC_BUFFER_FORMAT_ABGR: // ABGR corresponds to SurfaceFormat::ARGB
			if (surfaceFormat == SurfaceFormat::ARGB) ppKernel = m_kCopyPixels;
			if (surfaceFormat == SurfaceFormat::ABGR) ppKernel = m_kCopyPixelsSwapRB;
			break;
		case NV_ENC_BUFFER_FORMAT_ABGR10: // ABGR corresponds to SurfaceFormat::ARGB
			if (surfaceFormat == SurfaceFormat::ARGB10)
				ppKernel = m_kCopyPixels;
			else if (surfaceFormat == SurfaceFormat::ARGB16)
				ppKernel = m_kCopyPixels16;
			else if (surfaceFormat == SurfaceFormat::ABGR10)
				ppKernel = m_kCopyPixelsSwapRB;
			else if (surfaceFormat == SurfaceFormat::ABGR16)
				ppKernel = m_kCopyPixels16SwapRB;
			break;
		case NV_ENC_BUFFER_FORMAT_ARGB: // ARGB corresponds to SurfaceFormat::ABGR
			if (surfaceFormat == SurfaceFormat::ARGB) ppKernel = m_kCopyPixelsSwapRB;
			if (surfaceFormat == SurfaceFormat::ABGR) ppKernel = m_kCopyPixels;
			break;
		case NV_ENC_BUFFER_FORMAT_ARGB10: // ARGB corresponds to SurfaceFormat::ABGR
			if (surfaceFormat == SurfaceFormat::ARGB10)
				ppKernel = m_kCopyPixelsSwapRB;
			else if (surfaceFormat == SurfaceFormat::ARGB16)
				ppKernel = m_kCopyPixels16SwapRB;
			else if (surfaceFormat == SurfaceFormat::ABGR10)
				ppKernel = m_kCopyPixels;
			else if (surfaceFormat == SurfaceFormat::ABGR16)
				ppKernel = m_kCopyPixels16;
			break;
		}
		if (!ppKernel)
		{
			AVSLOG(Warning) << "EncoderNV: Unsupported encode input/surface format combination";
			return Result::EncoderBackend_EncodeFailed;
		}

		CUarray surfaceBaseMipLevel = nullptr;
		if (m_gResourceSupport)
		{
			if (CUFAILED(cuGraphicsMapResources(1, &m_registeredSurface.resource, 0)))
			{
				AVSLOG(Warning) << "EncoderNV: Failed to map video frame";
				return Result::EncoderBackend_EncodeFailed;
			}
			cuGraphicsSubResourceGetMappedArray(&surfaceBaseMipLevel, m_registeredSurface.resource, 0, 0);
		}
		else 
		{
#if defined(PLATFORM_WINDOWS)
			if (m_device.type == DeviceType::Direct3D12)
			{
				CUresult r = m_dx12Util.GetMipmappedArrayLevel(&surfaceBaseMipLevel, 0);
				if (CUFAILED(r))
				{
					AVSLOG(Warning) << "EncoderNV: Failed to map video frame";
					return Result::EncoderBackend_EncodeFailed;
				}
			}
#endif
		}
		
		assert(surfaceBaseMipLevel);

		CUDA_ARRAY_DESCRIPTOR desc1;
		CUDA_ARRAY3D_DESCRIPTOR desc2;

		CUresult r = cuArrayGetDescriptor_v2(&desc1, surfaceBaseMipLevel);
		r = cuArray3DGetDescriptor_v2(&desc2, surfaceBaseMipLevel);

		cuSurfRefSetArray(m_surfaceRef, surfaceBaseMipLevel, 0);

		unsigned int frameWidth = m_registeredSurface.surface->getWidth();
		unsigned int frameHeight = m_registeredSurface.surface->getHeight();

		void* ppKernelParams[] = {
			&m_inputBuffer.devicePtr,
			&frameWidth,
			&frameHeight,
			&m_inputBuffer.pitch,
			&m_params.depthRemapNear,
			&m_params.depthRemapFar,
		};

		const unsigned int blockDim = 16;
		const unsigned int gridDimX = (unsigned int)std::ceilf(float(frameWidth) / blockDim / gridDimDivisor);
		const unsigned int gridDimY = (unsigned int)std::ceilf(float(frameHeight) / blockDim / gridDimDivisor);

		Result result = Result::OK;
		if (CUFAILED(cuLaunchKernel(ppKernel, gridDimX, gridDimY, 1, blockDim, blockDim, 1, 0, 0, ppKernelParams, nullptr)))
		{
			AVSLOG(Warning) << "EncoderNV: Failed to launch pre processing kernel"; 
			result = Result::EncoderBackend_EncodeFailed;
		}

		if (m_gResourceSupport)
		{
			cuGraphicsUnmapResources(1, &m_registeredSurface.resource, 0);
		}

		return result;
	}

	Result EncoderNV::mapOutputBuffer(void*& bufferPtr, size_t& bufferSizeInBytes)
	{
		if (!m_encoder)
		{
			AVSLOG(Error) << "EncoderNV: Encoder not initialized";
			return Result::EncoderBackend_NotInitialized;
		}

		assert(m_outputBuffer.ptr);

		Result result = Result::OK;
		if (m_completionEvent)
		{
			result = waitForCompletionEvent();

			if (NVFAILED(g_api.nvEncUnmapInputResource(m_encoder, m_resource.mappedResource)))
			{
				AVSLOG(Error) << "EncoderNV: Failed to unmap input surface";

				if (result)
				{
					result = Result::EncoderBackend_UnmapFailed;
				}
			}
			if (!result)
			{
				return result;
			}
		}

		NV_ENC_LOCK_BITSTREAM params = {};
		params.version = NV_ENC_LOCK_BITSTREAM_VER;
		params.outputBitstream = m_outputBuffer.ptr;
		params.doNotWait = false;
		if (NVFAILED(g_api.nvEncLockBitstream(m_encoder, &params)))
		{
			AVSLOG(Error) << "EncoderNV: Failed to lock output bitstream buffer";
			return Result::EncoderBackend_MapFailed;
		}

		bufferPtr = params.bitstreamBufferPtr;
		bufferSizeInBytes = params.bitstreamSizeInBytes;

		return result;
	}

	Result EncoderNV::waitForCompletionEvent()
	{
#if defined(PLATFORM_WINDOWS)
		if (!m_completionEvent)
		{
			return Result::OK;
		}
#ifdef DEBUG
		WaitForSingleObject(m_completionEvent, INFINITE);
#else
		// wait for 20s which is infinite on terms of gpu time
		if (WaitForSingleObject(m_completionEvent, 20000) == WAIT_FAILED)
		{
			return Result::EncoderBackend_EncodeFailed;
		}
#endif
#endif
		return Result::OK;
	}

	Result EncoderNV::unmapOutputBuffer()
	{
		if (!m_encoder)
		{
			AVSLOG(Error) << "EncoderNV: Encoder not initialized";
			return Result::EncoderBackend_NotInitialized;
		}

		assert(m_outputBuffer.ptr);
		if (NVFAILED(g_api.nvEncUnlockBitstream(m_encoder, m_outputBuffer.ptr)))
		{
			AVSLOG(Error) << "EncoderNV: Failed to unlock output bitstream buffer";
			return Result::EncoderBackend_UnmapFailed;
		}
		return Result::OK;
	}

	SurfaceFormat EncoderNV::getInputFormat() const
	{
		switch (m_inputBuffer.format)
		{
		case NV_ENC_BUFFER_FORMAT_ABGR:
			return SurfaceFormat::ABGR;
		case NV_ENC_BUFFER_FORMAT_ARGB:
			return SurfaceFormat::ARGB;
		case NV_ENC_BUFFER_FORMAT_NV12:
			return SurfaceFormat::NV12;
		case NV_ENC_BUFFER_FORMAT_ARGB10:
			return SurfaceFormat::ARGB10;
		case NV_ENC_BUFFER_FORMAT_ABGR10:
			return SurfaceFormat::ABGR10;
		}
		return SurfaceFormat::Unknown;
	}

	bool EncoderNV::checkSupport()
	{
		constexpr uint32_t apiVersion = NVENCAPI_MAJOR_VERSION << 4 | NVENCAPI_MINOR_VERSION;

		ScopedLibraryHandle hNVENC(m_libNVENC);
		if (!hNVENC)
		{
			return false;
		}

		typedef NVENCSTATUS(NVENCAPI *NvEncodeAPIGetMaxSupportedVersionFUNC)(uint32_t*);
		NvEncodeAPIGetMaxSupportedVersionFUNC NvEncodeAPIGetMaxSupportedVersion =
			(NvEncodeAPIGetMaxSupportedVersionFUNC)Platform::getProcAddress(hNVENC, "NvEncodeAPIGetMaxSupportedVersion");
		if (!NvEncodeAPIGetMaxSupportedVersion)
		{
			return false;
		}

		uint32_t maxSupportedVersion;
		if (NVFAILED(NvEncodeAPIGetMaxSupportedVersion(&maxSupportedVersion)))
		{
			return false;
		}
		return apiVersion <= maxSupportedVersion;
	}

	Result EncoderNV::chooseEncodeConfig(GUID requestedEncodeGUID, GUID requestedPresetGUID, NV_ENC_BUFFER_FORMAT requestedFormat, EncodeConfig& config) const
	{
		enum RankPriority {
			High = 10,
			Medium = 5,
			Low = 1,
		};
		std::multimap<int, EncodeConfig, std::greater<int>> rankedConfigs;

		uint32_t numEncodeGUIDs;
		g_api.nvEncGetEncodeGUIDCount(m_encoder, &numEncodeGUIDs);
		std::vector<GUID> encodeGUIDs(numEncodeGUIDs);
		g_api.nvEncGetEncodeGUIDs(m_encoder, encodeGUIDs.data(), (uint32_t)encodeGUIDs.size(), &numEncodeGUIDs);

		// Filter codecs first.
		if (requestedEncodeGUID != GUID{ 0 })
		{
			auto it = std::remove_if(encodeGUIDs.begin(), encodeGUIDs.end(),
				[requestedEncodeGUID](GUID encodeGUID)
			{ return encodeGUID != requestedEncodeGUID; });
			encodeGUIDs.erase(it, encodeGUIDs.end());
		}

		for (GUID encodeGUID : encodeGUIDs)
		{
			EncodeConfig config = { encodeGUID };
			int rank = 0;

			// Prefer HEVC to H264 (but not higher than preset/format) 
			if (config.encodeGUID == NV_ENC_CODEC_HEVC_GUID)
			{
				rank += RankPriority::Low;
			}

			uint32_t numInputFormats;
			g_api.nvEncGetInputFormatCount(m_encoder, encodeGUID, &numInputFormats);
			std::vector<NV_ENC_BUFFER_FORMAT> inputFormats(numInputFormats);
			g_api.nvEncGetInputFormats(m_encoder, encodeGUID, inputFormats.data(), (uint32_t)inputFormats.size(), &numInputFormats);
			for (const NV_ENC_BUFFER_FORMAT format : inputFormats)
			{
				// If specific format is requested only rank configs supporting said format.
				if (requestedFormat != NV_ENC_BUFFER_FORMAT_UNDEFINED)
				{
					if (format == requestedFormat)
					{
						config.format = format;
						break;
					}
					else {
						continue;
					}
				}

				// Prefer ARGB or ABGR formats.
				if (format == NV_ENC_BUFFER_FORMAT_ARGB || format == NV_ENC_BUFFER_FORMAT_ABGR)
				{
					rank += RankPriority::Medium;
					config.format = format;
					break;
				}
				// YUV 4:2:0 is also supported.
				if (format == NV_ENC_BUFFER_FORMAT_NV12) 
				{
					config.format = format;
				}
			}
			if (config.format == 0)
			{
				// No suitable input format found.
				continue;
			}

			uint32_t numPresets;
			g_api.nvEncGetEncodePresetCount(m_encoder, encodeGUID, &numPresets);
			std::vector<GUID> presetGUIDs(numPresets);
			g_api.nvEncGetEncodePresetGUIDs(m_encoder, encodeGUID, presetGUIDs.data(), (uint32_t)presetGUIDs.size(), &numPresets);
			for (GUID presetGUID : presetGUIDs)
			{
				// Prefer matching preset.
				if (presetGUID == requestedPresetGUID)
				{
					config.presetGUID = presetGUID;
					rank += RankPriority::High;
					break;
				}
				// Otherwise select default preset, if available.
				if (presetGUID == NV_ENC_PRESET_DEFAULT_GUID)
				{
					config.presetGUID = presetGUID;
					rank += RankPriority::Low;
				}
			}
			// If no suitable preset found, just select the first one.
			if (config.presetGUID == GUID{ 0 })
			{
				config.presetGUID = presetGUIDs[0];
			}

			// Retrieve preset config.
			{
				NV_ENC_PRESET_CONFIG presetConfig = {};
				presetConfig.version = NV_ENC_PRESET_CONFIG_VER;
				presetConfig.presetCfg.version = NV_ENC_CONFIG_VER;
				g_api.nvEncGetEncodePresetConfig(m_encoder, encodeGUID, config.presetGUID, &presetConfig);
				config.config = presetConfig.presetCfg;
			}

			rankedConfigs.insert(std::pair<int, EncodeConfig>{rank, config});
		}

		if (rankedConfigs.size() == 0)
		{
			return Result::EncoderBackend_NoSuitableCodecFound;
		}

		config = rankedConfigs.begin()->second;

		return Result::OK;
	}

	bool EncoderNV::initializeEncodeLibrary()
	{
		ScopedLibraryHandle hNVENC(m_libNVENC);
		if (hNVENC)
		{
			typedef NVENCSTATUS(NVENCAPI *NvEncodeAPICreateInstanceFUNC)(NV_ENCODE_API_FUNCTION_LIST*);
			NvEncodeAPICreateInstanceFUNC NvEncodeAPICreateInstance =
				(NvEncodeAPICreateInstanceFUNC)Platform::getProcAddress(hNVENC, "NvEncodeAPICreateInstance");
			if (!NvEncodeAPICreateInstance)
			{
				return false;
			}

			g_api.version = NV_ENCODE_API_FUNCTION_LIST_VER;
			if (NVFAILED(NvEncodeAPICreateInstance(&g_api)))
			{
				return false;
			}
		}
		else {
			AVSLOG(Debug) << "Failed to load " << m_libNVENC.getName();
			return false;
		}

		hNVENC.take();
		return true;
	}

	void EncoderNV::releaseEncodeLibrary()
	{
		m_libNVENC.unload();
	}

} // avs

#endif // !PLATFORM_ANDROID
