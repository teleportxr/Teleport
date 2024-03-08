// libavstream
// (c) Copyright 2018-2024 Simul.co

#include <cstdint>
#include <cuda_fp16.h>

surface<void, cudaSurfaceType2D> inputSurfaceRef;

__device__ float readDepth(int x, int y)
{
	half value;
	surf2Dread(&value, inputSurfaceRef, x << 1, y);
	return __half2float(value);
}

__device__ uint16_t readDepthU16(int x, int y)
{
	half value;
	surf2Dread(&value, inputSurfaceRef, x << 1, y);
	return __half2ushort_rd(value);
}

// Encode 16-bit depth value as YUV triplet in a way that minimizes error due to quantization and sub-sampling.
// See: "Adapting Standard Video Codecs for Depth Streaming", Fabrizo Pece, Jan Kautz, Tim Weyrich.
__device__ float3 encodeDepth(float Ld)
{
	const int np = 512;
	const int w  = 65536;
	const float p = float(np) / float(w);

	const float pDiv2 = p / 2.0f;
	const float pDiv4 = p / 4.0f;

	float Ha = fmod(Ld / pDiv2, 2.0f);
	if(Ha > 1.0f) {
		Ha = 2.0f - Ha;
	}

	float Hb = fmod((Ld - pDiv4) / pDiv2, 2.0f);
	if(Hb > 1.0f) {
		Hb = 2.0f - Hb;
	}

	return make_float3(Ld, Ha, Hb);
}

extern "C" __global__ void CopyPixels(uint32_t* pixels, int width, int height, int pitch)
{
	unsigned int x = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int y = blockDim.y * blockIdx.y + threadIdx.y;

	if(x >= width || y >= height) {
		return;
	}

	surf2Dread(&pixels[y * (pitch >> 2) + x], inputSurfaceRef, x * 4, y);
}

extern "C" __global__ void CopyPixelsSwapRB(uchar4* pixels, int width, int height, int pitch)
{
	unsigned int x = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int y = blockDim.y * blockIdx.y + threadIdx.y;

	if(x >= width || y >= height) {
		return;
	}

	uchar4 pixel;
	surf2Dread(&pixel, inputSurfaceRef, x * 4, y);
	pixels[y * (pitch >> 2) + x] = make_uchar4(pixel.z, pixel.y, pixel.x, pixel.w);
}

extern "C" __global__ void CopyPixels16(uint32_t* pixels, int width, int height, int pitch)
{
	unsigned int x = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int y = blockDim.y * blockIdx.y + threadIdx.y;

	if (x >= width || y >= height) {

		return;
	}

	ushort4 p16;
	surf2Dread(&p16, inputSurfaceRef, x * 8, y);

	/* Aidan: Reduce to scale of 0-1023 (10-bit) from 0-65535 (16-bit) */
	p16.x /= 64;
	p16.y /= 64;
	p16.z /= 64;
	p16.w /= 64;

	uint32_t* pixel = &pixels[y * (pitch >> 2) + x];
	*pixel = uint32_t((p16.w << 30) | (p16.z << 20) | (p16.y << 10) | p16.x);
}

extern "C" __global__ void CopyPixels16SwapRB(uint32_t* pixels, int width, int height, int pitch)
{
	unsigned int x = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int y = blockDim.y * blockIdx.y + threadIdx.y;

	if (x >= width || y >= height) {

		return;
	}

	ushort4 p16;
	surf2Dread(&p16, inputSurfaceRef, x * 8, y);

	/* Aidan: Reduce to scale of 0-1023 (10-bit) from 0-65535 (16-bit) */
	p16.x /= 64;
	p16.y /= 64;
	p16.z /= 64;
	p16.w /= 64; 

	uint32_t* pixel = &pixels[y * (pitch >> 2) + x];
	*pixel = uint32_t((p16.w << 30) | (p16.x << 20) | (p16.y << 10) | p16.z);
}

extern "C" __global__ void RGBAtoNV12(uint8_t* pixels, int width, int height, int pitch)
{
	// TODO: Implement
}

extern "C" __global__ void BGRAtoNV12(uint8_t* pixels, int width, int height, int pitch)
{
	// TODO: Implement
}

extern "C" __global__ void R16toNV12(uint8_t* pixels, int width, int height, int pitch, float remapNear, float remapFar)
{
	unsigned int x = 2 * (blockDim.x * blockIdx.x + threadIdx.x);
	unsigned int y = 2 * (blockDim.y * blockIdx.y + threadIdx.y);

	if(x >= width || y >= height) {
		return;
	}

	float inputD0 = readDepth(x  , y  );
	float inputD1 = readDepth(x+1, y  );
	float inputD2 = readDepth(x  , y+1);
	float inputD3 = readDepth(x+1, y+1);

	float depthRemapRange = remapFar - remapNear;
	if(depthRemapRange > 0.0f) {
		inputD0 = saturate((inputD0 - remapNear) / depthRemapRange);
		inputD1 = saturate((inputD1 - remapNear) / depthRemapRange);
		inputD2 = saturate((inputD2 - remapNear) / depthRemapRange);
		inputD3 = saturate((inputD3 - remapNear) / depthRemapRange);
	}

	float3 D0 = encodeDepth(inputD0);
	float3 D1 = encodeDepth(inputD1);
	float3 D2 = encodeDepth(inputD2);
	float3 D3 = encodeDepth(inputD3);

	float U = 0.25f * (D0.y + D1.y + D2.y + D3.y);
	float V = 0.25f * (D0.z + D1.z + D2.z + D3.z);

	pixels[(y  ) * pitch + x  ] = D0.x * 255;
	pixels[(y  ) * pitch + x+1] = D1.x * 255;
	pixels[(y+1) * pitch + x  ] = D2.x * 255;
	pixels[(y+1) * pitch + x+1] = D3.x * 255;

	const int chromaOffset = pitch * height;
	pixels[chromaOffset + (y >> 1) * pitch + x  ] = U * 255;
	pixels[chromaOffset + (y >> 1) * pitch + x+1] = V * 255;
}

