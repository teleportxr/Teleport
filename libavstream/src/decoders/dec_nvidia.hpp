// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#if !defined(PLATFORM_ANDROID)

#include "libavstream/decoders/dec_interface.h"
#include <libraryloader.hpp>

#include <api/cuda.hpp>
#include <dynlink_nvcuvid.h>
#if(LIBAV_USE_D3D12)
#include <api/cuda_dx12.hpp>
#else
#include <api/cuda.hpp>
#endif

namespace avs
{
	/*!
	 * A DecoderNV.
	 *
	 * A DecoderNV uses the NVidia nvcuvid video decoder api to decode a video frame 
	 * from a compressed bitstream and writes the output to a surface texture using CUDA.
	 *
	 */
	class DecoderNV final : public DecoderBackendInterface
	{
	public:
		~DecoderNV();

		/* Begin DecoderBackendInterface */
		Result initialize(const DeviceHandle& device, int frameWidth, int frameHeight, const DecoderParams& params) override;
		Result reconfigure(int frameWidth, int frameHeight, const DecoderParams& params) override;
		Result shutdown() override;
		Result registerSurface(const SurfaceBackendInterface* surface, const SurfaceBackendInterface* alphaSurface = nullptr) override;
		Result unregisterSurface() override;

		Result decode(const void* buffer, size_t bufferSizeInBytes, const void* alphaBuffer, size_t alphaBufferSizeInBytes, VideoPayloadType payloadType, bool lastPayload) override;
		Result display(bool showAlphaAsColor = false) override;
		/* End DecoderBackendInterface */

	private:

		CUdevice m_device = 0;
		DeviceType m_deviceType = DeviceType::Invalid;

		DecoderParams m_params = {};

		CUcontext m_context = nullptr;
		CUvideoparser m_parser = nullptr;
		CUvideodecoder m_decoder = nullptr;
		CUfunction m_colorKernel = nullptr;
		CUfunction m_alphaKernel = nullptr;
		unsigned int m_frameWidth = 0;
		unsigned int m_frameHeight = 0;
		int m_displayPictureIndex = -1;

#if(LIBAV_USE_D3D12)
		CUDA::DX12Util m_dx12Util;
#endif
		// Variable signifying if the GraphicsResource type is supported by CUDA for the graphics API in use
		// Currently not supported for Vulkan and D3D12
		bool m_gResourceSupport = true;
		bool m_surfaceRegistered = false;

		CUmodule m_module = nullptr;
		CUsurfref m_surfaceRef = nullptr;

		CUfunction m_kNV12toRGBA = nullptr;
		CUfunction m_kNV12toABGR = nullptr;
		CUfunction m_kAlphaNV12toRGBA = nullptr;
		CUfunction m_kAlphaNV12toABGR = nullptr;
		CUfunction m_kNV12toR16 = nullptr;
		CUfunction m_kP016toRGBA = nullptr;
		CUfunction m_kP016toABGR = nullptr;
		CUfunction m_kYUV444toRGBA = nullptr;
		CUfunction m_kYUV444toABGR = nullptr;
		CUfunction m_kYUV444P16toRGBA = nullptr;
		CUfunction m_kYUV444P16toABGR = nullptr;

		CUgraphicsResource m_registeredSurface = nullptr;
		SurfaceFormat m_registeredSurfaceFormat = SurfaceFormat::Unknown;

		static int CUDAAPI onSequence(void*, CUVIDEOFORMAT*);
		static int CUDAAPI onDisplayPicture(void*, CUVIDPARSERDISPINFO*);
		static int CUDAAPI onDecodePicture(void*, CUVIDPICPARAMS*);

		static LibraryLoader m_libCUVID;
		static bool initializeCUVID();
	};

} // avs

#endif // !PLATFORM_ANDROID
