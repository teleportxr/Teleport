// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd
#if defined(LIBAV_USE_CUDA)

#include <api/cuda.hpp>
#include <libraryloader.hpp>
#include <logger.hpp>
#include <iostream>


tcuCtxCreate* cuCtxCreate;
tcuCtxDestroy* cuCtxDestroy;
tcuCtxPushCurrent* cuCtxPushCurrent;
tcuCtxPopCurrent* cuCtxPopCurrent;
tcuCtxSynchronize* cuCtxSynchronize;
tcuModuleLoadFatBinary* cuModuleLoadFatBinary;
tcuModuleUnload* cuModuleUnload;
tcuModuleGetFunction* cuModuleGetFunction;
tcuModuleGetSurfRef* cuModuleGetSurfRef;
tcuSurfRefSetArray* cuSurfRefSetArray;
tcuMemAlloc* cuMemAlloc;
tcuMemAllocPitch* cuMemAllocPitch;
tcuMemFree* cuMemFree;
tcuMemsetD8* cuMemsetD8;
tcuMemcpy2D* cuMemcpy2D;
tcuMemcpyDtoH* cuMemcpyDtoH;
tcuLaunchKernel* cuLaunchKernel;
tcuStreamSynchronize* cuStreamSynchronize;
tcuGraphicsMapResources* cuGraphicsMapResources;
tcuGraphicsUnmapResources* cuGraphicsUnmapResources;
tcuGraphicsResourceSetMapFlags* cuGraphicsResourceSetMapFlags;
tcuGraphicsSubResourceGetMappedArray* cuGraphicsSubResourceGetMappedArray;
tcuGraphicsUnregisterResource* cuGraphicsUnregisterResource;
tcuDriverGetVersion* cuDriverGetVersion;
tcuDeviceGetCount*   cuDeviceGetCount;
tcuCtxCreate_v2* cuCtxCreate_v2;
tcuCtxDestroy_v2* cuCtxDestroy_v2;
tcuCtxPushCurrent_v2* cuCtxPushCurrent_v2;
tcuCtxPopCurrent_v2* cuCtxPopCurrent_v2;
tcuDeviceGetUuid* cuDeviceGetUuid;
tcuDeviceGetLuid* cuDeviceGetLuid;
tcuImportExternalMemory* cuImportExternalMemory;
tcuExternalMemoryGetMappedMipmappedArray* cuExternalMemoryGetMappedMipmappedArray;
tcuDestroyExternalMemory* cuDestroyExternalMemory;
tcuMipmappedArrayGetLevel* cuMipmappedArrayGetLevel;
tcuArrayGetDescriptor* cuArrayGetDescriptor;
tcuArray3DGetDescriptor* cuArray3DGetDescriptor;
tcuArrayGetDescriptor_v2* cuArrayGetDescriptor_v2;
tcuArray3DGetDescriptor_v2* cuArray3DGetDescriptor_v2;
tcuMemAllocPitch_v2* cuMemAllocPitch_v2;
tcuMemFree_v2* cuMemFree_v2;
tcuMipmappedArrayDestroy* cuMipmappedArrayDestroy;

#if defined(PLATFORM_WINDOWS)
tcuD3D11GetDevices* cuD3D11GetDevices;
tcuGraphicsD3D11RegisterResource* cuGraphicsD3D11RegisterResource;
#endif


void __checkCudaErrors(CUresult err, const char *file, const int line)
{
	if (CUDA_SUCCESS != err)
	{
		std::cerr << "checkCudaErrors() Driver API error = " << err << "\""
			//			<< getCudaDrvErrorString(err) << "\" from file <" << file
			<< ", line " << line << "\n";
		exit(-1);
	}
}

namespace avs::CUDA {

	bool initialize()
	{
		static bool initialized = false;

		if (initialized)
			return true;

		static LibraryLoader libCUDA("nvcuda");

		ScopedLibraryHandle hLibrary(libCUDA);
		if (!hLibrary)
		{
			AVSLOG(Error) << "DecoderNV: CUDA runtime not found";
			return false;
		}

		auto cuInit = (tcuInit*)Platform::getProcAddress(hLibrary, "cuInit");
		if (!cuInit || CUFAILED(cuInit(0)))
		{
			AVSLOG(Error) << "DecoderNV: Failed to initialize CUDA driver API";
			return false;
		}

		CUGETPROC(cuCtxCreate);
		CUGETPROC(cuCtxDestroy);
		CUGETPROC(cuCtxPushCurrent);
		CUGETPROC(cuCtxPopCurrent);
		CUGETPROC(cuCtxSynchronize);
		CUGETPROC(cuModuleLoadFatBinary);
		CUGETPROC(cuModuleUnload);
		CUGETPROC(cuModuleGetFunction);
		CUGETPROC(cuModuleGetSurfRef);
		CUGETPROC(cuSurfRefSetArray);
		CUGETPROC(cuMemAlloc);
		CUGETPROC(cuMemAllocPitch);
		CUGETPROC(cuMemFree);
		CUGETPROC(cuMemsetD8);
		CUGETPROC(cuMemcpy2D);
		CUGETPROC(cuMemcpyDtoH);
		CUGETPROC(cuLaunchKernel);
		CUGETPROC(cuStreamSynchronize);
		CUGETPROC(cuGraphicsMapResources);
		CUGETPROC(cuGraphicsUnmapResources);
		CUGETPROC(cuGraphicsResourceSetMapFlags);
		CUGETPROC(cuGraphicsSubResourceGetMappedArray);
		CUGETPROC(cuGraphicsUnregisterResource);
		CUGETPROC(cuDriverGetVersion);
		CUGETPROC(cuDeviceGetCount);
		CUGETPROC(cuCtxCreate_v2);
		CUGETPROC(cuCtxDestroy_v2);
		CUGETPROC(cuCtxPushCurrent_v2);
		CUGETPROC(cuCtxPopCurrent_v2);
		CUGETPROC(cuDeviceGetUuid);
		CUGETPROC(cuDeviceGetLuid);
		CUGETPROC(cuImportExternalMemory);
		CUGETPROC(cuExternalMemoryGetMappedMipmappedArray); 
		CUGETPROC(cuDestroyExternalMemory);
		CUGETPROC(cuMipmappedArrayGetLevel);
		CUGETPROC(cuArrayGetDescriptor);
		CUGETPROC(cuArray3DGetDescriptor);
		CUGETPROC(cuArrayGetDescriptor_v2);
		CUGETPROC(cuArray3DGetDescriptor_v2);
		CUGETPROC(cuMemAllocPitch_v2);
		CUGETPROC(cuMemFree_v2);
		CUGETPROC(cuMipmappedArrayDestroy);

#if defined(PLATFORM_WINDOWS)
		CUGETPROC(cuD3D11GetDevices);
		CUGETPROC(cuGraphicsD3D11RegisterResource);
#endif

		hLibrary.take();

		initialized = true;

		return true;
	}
} // avs
#endif