// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once

#if (LIBAV_USE_D3D12)

#include "api/cuda.hpp"
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <stdexcept>
#include <cuda_runtime.h>

namespace avs::CUDA
{
	#define SAFE_RELEASE(p) if (p) (p)->Release()

	class WindowsSecurityAttributes {
	protected:
		SECURITY_ATTRIBUTES m_winSecurityAttributes;
		PSECURITY_DESCRIPTOR m_winPSecurityDescriptor;

	public:
		WindowsSecurityAttributes();
		~WindowsSecurityAttributes();
		SECURITY_ATTRIBUTES * operator&();
	};

	class DX12Util
	{
	public:
		DX12Util();
		~DX12Util();
		void Initialize();
		CUresult GetCudaDevice(unsigned int *pCudaDeviceCount, CUdevice *pCudaDevice, ID3D12Device* pD3D12Device);
		CUresult LoadGraphicsResource(ID3D12Resource *pD3D12Resource);
		CUresult GetMipmappedArrayLevel(CUarray *pLevelArray, unsigned int level);
		CUresult UnloadGraphicsResource();

	private:
		CUresult GetMappedMipmappedArrayFromResource();
		CUresult FreeMappedMipmappedArrayFromResource();
		CUresult DestroyExternalMemory();

		static inline std::string HrToString(HRESULT hr) 
		{
			char s_str[64] = {};
			sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
			return std::string(s_str);
		}

		class HrException : public std::runtime_error
		{
		public:
			HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
			HRESULT Error() const { return m_hr; }
		private:
			const HRESULT m_hr;
		};

		static inline void ThrowIfFailed(HRESULT hr)
		{
			if (FAILED(hr))
			{
				throw HrException(hr);
			}
		}

		static void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter);

		UINT m_nodeMask;
		ID3D12Device* m_device;
		ID3D12Resource* m_resource;
		CUexternalMemory m_externalMemory;
		CUmipmappedArray m_mipmapArray;
	};
} // avs::CUDA
#endif