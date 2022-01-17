// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#if !defined(PLATFORM_ANDROID)

#include <mutex>

#include <platform.hpp>
#include <common_p.hpp>
#include <libraryloader.hpp>

#include <libavstream/encoders/enc_interface.hpp>
#if defined(PLATFORM_WINDOWS)
#include <api/cuda_dx12.hpp>
#else
#include <ClientRender/cuda.hpp>
#endif

#include <nvEncodeAPI.h>

namespace avs
{
	class EncoderNV final : public EncoderBackendInterface
	{
	public:
		~EncoderNV();

		static bool checkSupport();

		/* Begin EncoderInterface */
		Result initialize(const DeviceHandle& device, int frameWidth, int frameHeight, const EncoderParams& params) override;
		Result reconfigure(int frameWidth, int frameHeight, const EncoderParams& params) override;
		Result shutdown() override;
		Result registerSurface(const SurfaceBackendInterface* surface) override;
		Result unregisterSurface() override;

		Result encodeFrame(uint64_t timestamp, bool forceIDR = false) override;

		Result mapOutputBuffer(void*& bufferPtr, size_t& bufferSizeInBytes) override;
		Result unmapOutputBuffer() override;

		SurfaceFormat getInputFormat() const override;

		Result waitForEncodingCompletion() override;

		/* End EncoderInterface */

		struct EncodeConfig
		{
			GUID encodeGUID;
			GUID presetGUID;
			NV_ENC_BUFFER_FORMAT format;
			NV_ENC_CONFIG config;
		};
		Result chooseEncodeConfig(GUID requestedEncodeGUID, GUID requestedPresetGUID, NV_ENC_TUNING_INFO tuningInfo, NV_ENC_BUFFER_FORMAT requestedFormat, EncodeConfig& config) const;
		Result processInput();

	private:
		Result initializeEncoder(int frameWidth, int frameHeight, const EncoderParams& params);
		static bool initializeEncodeLibrary();
		static void releaseEncodeLibrary();

		// Variable signifying if the GraphicsResource type is supported by CUDA for the graphics API in use
		// Currently not supported for Vulkan and D3D12
		bool m_gResourceSupport = true;
		EncoderParams m_params = {};
		DeviceHandle m_device = {};
		void* m_encoder = nullptr;

#if defined(PLATFORM_WINDOWS)
		CUDA::DX12Util m_dx12Util;
#endif

		CUcontext m_context = nullptr;
		CUmodule m_module = nullptr;
		CUsurfref m_surfaceRef = nullptr;

		CUfunction m_kCopyPixels = nullptr;
		CUfunction m_kCopyPixelsSwapRB = nullptr;
		CUfunction m_kCopyPixels16 = nullptr;
		CUfunction m_kCopyPixels16SwapRB = nullptr;

		CUfunction m_kRGBAtoNV12 = nullptr;
		CUfunction m_kBGRAtoNV12 = nullptr;
		CUfunction m_kR16toNV12 = nullptr;

		struct EncoderInputData
		{
			NV_ENC_BUFFER_FORMAT format = NV_ENC_BUFFER_FORMAT_UNDEFINED;
			size_t pitch = 0;
		};
		EncoderInputData m_inputData;

		static constexpr uint32_t MAX_BUFFERS = 4;

		struct InputBuffer
		{
			CUdeviceptr devicePtr = 0;
			NV_ENC_REGISTERED_PTR regPtr = nullptr;
			NV_ENC_MAP_INPUT_RESOURCE resource;
		};

		struct OutputBuffer
		{
			NV_ENC_OUTPUT_PTR ptr = nullptr;
		};

		struct EncoderBufferData
		{
			InputBuffer inputBuffer;
			OutputBuffer outputBuffer;
			void* completionEvent = nullptr;
		};
		EncoderBufferData m_bufferData[MAX_BUFFERS];

		struct RegisteredSurface
		{
			const SurfaceBackendInterface* surface = nullptr;
			CUgraphicsResource cuResource = nullptr;
		};
		RegisteredSurface m_registeredSurface;
		EncodeCapabilities m_EncodeCapabilities;
		static LibraryLoader m_libNVENC;
		uint32_t m_numBuffers = 1;
		int m_bufferIndex = 0;
		bool m_initialized = false;

		ThreadSafeQueue<int> m_bufferIndexQueue;
	};

} // avs

#endif // !PLATFORM_ANDROID
