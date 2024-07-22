// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#include <platform.hpp>

#include <string>
#include <stdexcept>

#include <dynlink_cuda.h>
#if defined(PLATFORM_WINDOWS)
#include <dynlink_cudaD3D11.h>
#endif

#define CUFAILED(x) \
	((x) != CUDA_SUCCESS)
#define CUGETPROC(proc) \
	proc = (t##proc*)Platform::getProcAddress(hLibrary, #proc)
#define CUGETPROC_EX(proc, name) \
	proc = (t##proc*)Platform::getProcAddress(hLibrary, name)

extern tcuCtxCreate* cuCtxCreate;
extern tcuCtxDestroy* cuCtxDestroy;
extern tcuCtxPushCurrent* cuCtxPushCurrent;
extern tcuCtxPopCurrent* cuCtxPopCurrent;
extern tcuCtxSynchronize* cuCtxSynchronize;
extern tcuModuleLoadFatBinary* cuModuleLoadFatBinary;
extern tcuModuleUnload* cuModuleUnload;
extern tcuModuleGetFunction* cuModuleGetFunction;
extern tcuModuleGetSurfRef* cuModuleGetSurfRef;
extern tcuSurfRefSetArray* cuSurfRefSetArray;
extern tcuMemAlloc* cuMemAlloc;
extern tcuMemAllocPitch* cuMemAllocPitch;
extern tcuMemFree* cuMemFree;
extern tcuMemsetD8* cuMemsetD8;
extern tcuMemcpyDtoH* cuMemcpyDtoH;
extern tcuMemcpy2D* cuMemcpy2D;
extern tcuLaunchKernel* cuLaunchKernel;
extern tcuStreamSynchronize* cuStreamSynchronize;
extern tcuGraphicsMapResources* cuGraphicsMapResources;
extern tcuGraphicsUnmapResources* cuGraphicsUnmapResources;
extern tcuGraphicsResourceSetMapFlags* cuGraphicsResourceSetMapFlags;
extern tcuGraphicsSubResourceGetMappedArray* cuGraphicsSubResourceGetMappedArray;
extern tcuGraphicsUnregisterResource* cuGraphicsUnregisterResource;
extern tcuDriverGetVersion* cuDriverGetVersion;
extern tcuDeviceGetCount*   cuDeviceGetCount;
// Simul additions to dynamic_cuda_cuda.h
extern tcuCtxCreate_v2* cuCtxCreate_v2;
extern tcuCtxDestroy_v2* cuCtxDestroy_v2;
extern tcuCtxPushCurrent_v2* cuCtxPushCurrent_v2;
extern tcuCtxPopCurrent_v2* cuCtxPopCurrent_v2;
extern tcuDeviceGetUuid* cuDeviceGetUuid;
extern tcuDeviceGetLuid* cuDeviceGetLuid;
extern tcuImportExternalMemory* cuImportExternalMemory;
extern tcuExternalMemoryGetMappedMipmappedArray* cuExternalMemoryGetMappedMipmappedArray;
extern tcuDestroyExternalMemory* cuDestroyExternalMemory;
extern tcuMipmappedArrayGetLevel* cuMipmappedArrayGetLevel;
extern tcuArrayGetDescriptor* cuArrayGetDescriptor;
extern tcuArray3DGetDescriptor* cuArray3DGetDescriptor;
extern tcuArrayGetDescriptor_v2* cuArrayGetDescriptor_v2;
extern tcuArray3DGetDescriptor_v2* cuArray3DGetDescriptor_v2;
extern tcuMemAllocPitch_v2* cuMemAllocPitch_v2;
extern tcuMemFree_v2* cuMemFree_v2;
extern tcuMipmappedArrayDestroy* cuMipmappedArrayDestroy; 

#if defined(PLATFORM_WINDOWS)
extern tcuD3D11GetDevices* cuD3D11GetDevices;
extern tcuGraphicsD3D11RegisterResource* cuGraphicsD3D11RegisterResource;
#endif

namespace avs::CUDA {

struct ContextGuard
{
	explicit ContextGuard(CUcontext ctx, bool pushContext=true)
		: m_context(ctx)
	{
		if(pushContext)
{
			cuCtxPushCurrent(m_context);
		}
	}
	~ContextGuard()
	{
		if(m_context)
{
			CUcontext dummyContext;
			cuCtxPopCurrent(&dummyContext);
		}
	}
private:
	CUcontext const m_context;
};

bool initialize();

} // avs::CUDA
