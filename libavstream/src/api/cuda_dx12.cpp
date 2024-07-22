// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#if (LIBAV_USE_D3D12)

#include <api/cuda_dx12.hpp>
#include <libraryloader.hpp>
#include <logger.hpp>
#include <iostream>
#include <aclapi.h>

namespace avs::CUDA {

	using Microsoft::WRL::ComPtr;

	WindowsSecurityAttributes::WindowsSecurityAttributes()
	{
		m_winPSecurityDescriptor = (PSECURITY_DESCRIPTOR)calloc(1, SECURITY_DESCRIPTOR_MIN_LENGTH + 2 * sizeof(void**));
		assert(m_winPSecurityDescriptor != (PSECURITY_DESCRIPTOR)NULL);

		PSID *ppSID = (PSID *)((PBYTE)m_winPSecurityDescriptor + SECURITY_DESCRIPTOR_MIN_LENGTH);
		PACL *ppACL = (PACL *)((PBYTE)ppSID + sizeof(PSID *));

		InitializeSecurityDescriptor(m_winPSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);

		SID_IDENTIFIER_AUTHORITY sidIdentifierAuthority = SECURITY_WORLD_SID_AUTHORITY;
		AllocateAndInitializeSid(&sidIdentifierAuthority, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, ppSID);

		EXPLICIT_ACCESS explicitAccess;
		ZeroMemory(&explicitAccess, sizeof(EXPLICIT_ACCESS));
		explicitAccess.grfAccessPermissions = STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL;
		explicitAccess.grfAccessMode = SET_ACCESS;
		explicitAccess.grfInheritance = INHERIT_ONLY;
		explicitAccess.Trustee.TrusteeForm = TRUSTEE_IS_SID;
		explicitAccess.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
		explicitAccess.Trustee.ptstrName = (LPTSTR)*ppSID;

		SetEntriesInAcl(1, &explicitAccess, NULL, ppACL);

		SetSecurityDescriptorDacl(m_winPSecurityDescriptor, TRUE, *ppACL, FALSE);

		m_winSecurityAttributes.nLength = sizeof(m_winSecurityAttributes);
		m_winSecurityAttributes.lpSecurityDescriptor = m_winPSecurityDescriptor;
		m_winSecurityAttributes.bInheritHandle = TRUE;
	}

	WindowsSecurityAttributes::~WindowsSecurityAttributes()
	{
		PSID* ppSID = (PSID*)((PBYTE)m_winPSecurityDescriptor + SECURITY_DESCRIPTOR_MIN_LENGTH);
		PACL* ppACL = (PACL*)((PBYTE)ppSID + sizeof(PSID*));

		if (*ppSID) {
			FreeSid(*ppSID);
		}
		if (*ppACL) {
			LocalFree(*ppACL);
		}
		free(m_winPSecurityDescriptor);
	}

	SECURITY_ATTRIBUTES* WindowsSecurityAttributes::operator&()
	{
		return &m_winSecurityAttributes;
	}

	DX12Util::DX12Util()
	{
		Initialize();
	}

	DX12Util::~DX12Util() {}

	void DX12Util::Initialize()
	{
		m_nodeMask = 0;
		m_device = nullptr;
		m_resource = nullptr;
		m_externalMemory = nullptr;
		m_mipmapArray = nullptr;
	}

	// Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
	// If no such adapter can be found, *ppAdapter will be set to nullptr.
	_Use_decl_annotations_
	void DX12Util::GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter)
	{
		ComPtr<IDXGIAdapter1> adapter;

		*ppAdapter = nullptr;

		for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				// Don't select the Basic Render Driver adapter.
				continue;
			}

			// Check to see if the adapter supports Direct3D 12, but don't create the
			// actual device yet.
			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}

		*ppAdapter = adapter.Detach();
	}

	CUresult DX12Util::GetCudaDevice(unsigned int *pCudaDeviceCount, CUdevice *pCudaDevice, ID3D12Device* pD3D12Device)
	{
		CUresult result = CUresult::CUDA_SUCCESS;

		int num_cuda_devices;

		result = cuDeviceGetCount(&num_cuda_devices);
		if (CUFAILED(result))
		{
			return result;
		}

		if (num_cuda_devices <= 0)
		{
			return CUresult::CUDA_ERROR_NO_DEVICE;
		}

		*pCudaDeviceCount = num_cuda_devices;

		LUID dx12deviceluid = pD3D12Device->GetAdapterLuid();

		for (UINT devId = 0; devId < *pCudaDeviceCount; devId++)
		{
			char value;
			unsigned int deviceNodeMask;
			result = cuDeviceGetLuid(&value, &deviceNodeMask, devId);
			const char* luid = &value;
			if (CUFAILED(result))
			{
				return result;
			}

			if ((memcmp(&dx12deviceluid.LowPart, luid, sizeof(dx12deviceluid.LowPart)) == 0) && (memcmp(&dx12deviceluid.HighPart, luid + sizeof(dx12deviceluid.LowPart), sizeof(dx12deviceluid.HighPart)) == 0))
			{
				//cudaSetDevice(devId);
				m_nodeMask = deviceNodeMask;
				*pCudaDevice = devId;
				m_device = pD3D12Device;
				return CUresult::CUDA_SUCCESS;
			}
		}
		return result;
	}

	CUresult DX12Util::LoadGraphicsResource(ID3D12Resource *pD3D12Resource)
	{
		assert(m_device);

		CUresult result = CUresult::CUDA_SUCCESS;

		HANDLE sharedHandle;
		WindowsSecurityAttributes windowsSecurityAttributes;
		LPCWSTR name = NULL;
		
		ThrowIfFailed(m_device->CreateSharedHandle(pD3D12Resource, &windowsSecurityAttributes, GENERIC_ALL, name, &sharedHandle));

		D3D12_RESOURCE_ALLOCATION_INFO d3d12ResourceAllocationInfo;
		
		D3D12_RESOURCE_DESC desc = pD3D12Resource->GetDesc();

		d3d12ResourceAllocationInfo = m_device->GetResourceAllocationInfo(m_nodeMask, 1, &desc);
		size_t actualSize = d3d12ResourceAllocationInfo.SizeInBytes;
		size_t alignment = d3d12ResourceAllocationInfo.Alignment;

		CUDA_EXTERNAL_MEMORY_HANDLE_DESC externalMemoryHandleDesc;
		memset(&externalMemoryHandleDesc, 0, sizeof(externalMemoryHandleDesc));
		externalMemoryHandleDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE;
		externalMemoryHandleDesc.handle.win32.handle = sharedHandle;
		externalMemoryHandleDesc.size = actualSize;
		externalMemoryHandleDesc.flags = 0x1; // external memory dedicated

		result = cuImportExternalMemory(&m_externalMemory, &externalMemoryHandleDesc);

		if (CUFAILED(result))
		{
			return result;
		}
		
		m_resource = pD3D12Resource;

		result = GetMappedMipmappedArrayFromResource();

		return result;
	}

	CUresult DX12Util::GetMappedMipmappedArrayFromResource()
	{
		assert(m_externalMemory != nullptr);
		assert(m_resource);

		CUresult result = CUresult::CUDA_SUCCESS;

		D3D12_RESOURCE_DESC desc = m_resource->GetDesc();

		// Aidan: For some unknown reason the driver api cuExternalMemoryGetMappedMipmappedArray call gets invalid value error result
		// This forces us to use the runtime api cudaExternalMemoryGetMappedMipmappedArray instead for the moment

		//CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC mipmapDesc;
		//mipmapDesc.offset = 0;
		//mipmapDesc.arrayDesc.Depth = 0;
		//mipmapDesc.arrayDesc.Height = desc.Height;
		//mipmapDesc.arrayDesc.Width = desc.Width;
		//mipmapDesc.arrayDesc.Flags = CUDA_ARRAY3D_SURFACE_LDST; //0x20
		//mipmapDesc.arrayDesc.Format = CU_AD_FORMAT_UNSIGNED_INT8;
		//mipmapDesc.arrayDesc.NumChannels = 4;
		//mipmapDesc.numLevels = desc.MipLevels;
		//
		//result = cuExternalMemoryGetMappedMipmappedArray(&m_mipmapArray, m_externalMemory, &mipmapDesc);

		cudaExternalMemoryMipmappedArrayDesc mipmapDesc;
		mipmapDesc.extent.depth = 0;
		mipmapDesc.extent.height = desc.Height;
		mipmapDesc.extent.width = desc.Width;
		mipmapDesc.flags = cudaArraySurfaceLoadStore;
		switch (desc.Format)
		{
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_R10G10B10A2_UNORM:
			mipmapDesc.formatDesc = cudaCreateChannelDesc(8, 8, 8, 8, cudaChannelFormatKindUnsigned);
			break;
		case DXGI_FORMAT_R16G16B16A16_UNORM:
			mipmapDesc.formatDesc = cudaCreateChannelDesc(16, 16, 16, 16, cudaChannelFormatKindUnsigned);
			break;
		default:
			return CUresult::CUDA_ERROR_INVALID_VALUE;
		}
		mipmapDesc.numLevels = desc.MipLevels;
		mipmapDesc.offset = 0;

		// We need to cast to the CUDA runtime api types and back to the driver api types
		cudaExternalMemory_t externalMemory = reinterpret_cast<cudaExternalMemory_t>(m_externalMemory);
		cudaMipmappedArray_t mipmapArray;
		cudaError_t e = cudaExternalMemoryGetMappedMipmappedArray(&mipmapArray, externalMemory, &mipmapDesc);

		if (e != cudaSuccess)
		{
			return CUresult::CUDA_ERROR_UNKNOWN;
		}

		m_mipmapArray = reinterpret_cast<CUmipmappedArray>(mipmapArray);

		return result;
	}

	CUresult DX12Util::GetMipmappedArrayLevel(CUarray *pLevelArray, unsigned int level)
	{
		assert(m_mipmapArray);
		
		return cuMipmappedArrayGetLevel(pLevelArray, m_mipmapArray, level);
	}

	CUresult DX12Util::FreeMappedMipmappedArrayFromResource()
	{
		CUresult result = CUresult::CUDA_SUCCESS;

		if (m_mipmapArray != nullptr)
		{
			//result = cuMipmappedArrayDestroy(m_mipmapArray);
		}

		return result;
	}

	CUresult DX12Util::DestroyExternalMemory()
	{
		CUresult result = CUresult::CUDA_SUCCESS;

		if (m_externalMemory != nullptr)
		{
			//result = cuDestroyExternalMemory(m_externalMemory);
		}

		return result;
	}

	CUresult DX12Util::UnloadGraphicsResource()
	{
		CUresult result = FreeMappedMipmappedArrayFromResource();
		if (result == CUDA_SUCCESS)
		{
			//result = DestroyExternalMemory();
		}
		return result;
	}

} // avs::CUDA_N
#endif
