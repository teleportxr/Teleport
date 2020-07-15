// libavstream
// (c) Copyright 2018 Simul.co

#include <cstdint>
#include <cuda_fp16.h>

surface<void, cudaSurfaceType2D> outputSurfaceRef;

__device__ const float YUVtoRGB_ITU709[3][3] = {
	{ 1.0f,  0.0f,       1.5748f   },
	{ 1.0f, -0.187324f, -0.468124f },
	{ 1.0f,  1.8556f  ,  0.0f      },
};

__device__ const float YUVtoRGB_BT709[3][3] = {
	{ 1.16438f,  0.0f,  1.83367f   },
	{ 1.16438f, -0.218117f, -0.545076f },
	{ 1.16438f,  2.16063f,  0.0f      },
};

__device__ const float YUVtoRGB_BT2020[3][3] = {
	{ 1.16893f,  0.0f,       1.72371f   },
	{ 1.16893f, -0.192351f, -0.667873f },
	{ 1.16893f,  2.19923f  ,  0.0f      },
};

template<class T>
__device__ static T Clamp(T x, T lower, T upper) {
	return x < lower ? lower : (x > upper ? upper : x);
}

template<class YuvUnit>
__device__ inline uchar3 YuvToRgbForPixel(YuvUnit y, YuvUnit u, YuvUnit v, int matrixType) 
{
	const int
		low = 1 << (sizeof(YuvUnit) * 8 - 4),
		mid = 1 << (sizeof(YuvUnit) * 8 - 1);
	float fy = (int)y - low, fu = (int)u - mid, fv = (int)v - mid;
	const float maxf = (1 << sizeof(YuvUnit) * 8) - 1.0f;
	YuvUnit r, g, b = 0;
	if (matrixType == 0)
	{
		r = (YuvUnit)Clamp(YUVtoRGB_BT709[0][0] * fy + YUVtoRGB_BT709[0][1] * fu + YUVtoRGB_BT709[0][2] * fv, 0.0f, maxf);
		g = (YuvUnit)Clamp(YUVtoRGB_BT709[1][0] * fy + YUVtoRGB_BT709[1][1] * fu + YUVtoRGB_BT709[1][2] * fv, 0.0f, maxf);
		b = (YuvUnit)Clamp(YUVtoRGB_BT709[2][0] * fy + YUVtoRGB_BT709[2][1] * fu + YUVtoRGB_BT709[2][2] * fv, 0.0f, maxf);
	}
	else if (matrixType == 4)
	{
		r = (YuvUnit)Clamp(YUVtoRGB_BT2020[0][0] * fy + YUVtoRGB_BT2020[0][1] * fu + YUVtoRGB_BT2020[0][2] * fv, 0.0f, maxf);
		g = (YuvUnit)Clamp(YUVtoRGB_BT2020[1][0] * fy + YUVtoRGB_BT2020[1][1] * fu + YUVtoRGB_BT2020[1][2] * fv, 0.0f, maxf);
		b = (YuvUnit)Clamp(YUVtoRGB_BT2020[2][0] * fy + YUVtoRGB_BT2020[2][1] * fu + YUVtoRGB_BT2020[2][2] * fv, 0.0f, maxf);
	}

	uchar3 rgb;
	const int nShift = abs((int)sizeof(YuvUnit) - (int)sizeof(rgb.x)) * 8;
	if (sizeof(YuvUnit) >= sizeof(rgb.x)) 
	{
		rgb.x = r >> nShift;
		rgb.y = g >> nShift;
		rgb.z = b >> nShift;
	}
	else 
	{
		rgb.x = r << nShift;
		rgb.y = g << nShift;
		rgb.z = b << nShift;
	}
	
	return make_uchar3(rgb.x, rgb.y, rgb.z);
}

template<class YuvUnitx2>
__device__ void inline YuvToRgb(const uint8_t* frame, int width, int height, int pitch, int matrixType, bool isABGR)
{
	unsigned int x = 2 * (blockDim.x * blockIdx.x + threadIdx.x);
	unsigned int y = 2 * (blockDim.y * blockIdx.y + threadIdx.y);

	if (x + 1 >= width || y + 1>= height) {
		return;
	}

	uint8_t* pSrc = (uint8_t *)&frame[x * sizeof(YuvUnitx2) / 2 + y * pitch];
	// 4 Ys
	YuvUnitx2 l0 = *(YuvUnitx2 *)pSrc;
	YuvUnitx2 l1 = *(YuvUnitx2 *)(pSrc + pitch);
	// U and V components
	YuvUnitx2 ch = *(YuvUnitx2 *)(pSrc + (height - y / 2) * pitch);

	uchar4 p0, p1, p2, p3;

	if (isABGR)
	{
		p0 = packABGR8(YuvToRgbForPixel(l0.x, ch.x, ch.y, matrixType));
		p1 = packABGR8(YuvToRgbForPixel(l0.y, ch.x, ch.y, matrixType));
		p2 = packABGR8(YuvToRgbForPixel(l1.x, ch.x, ch.y, matrixType));
		p3 = packABGR8(YuvToRgbForPixel(l1.y, ch.x, ch.y, matrixType));
	}
	else
	{
		p0 = packRGBA8(YuvToRgbForPixel(l0.x, ch.x, ch.y, matrixType));
		p1 = packRGBA8(YuvToRgbForPixel(l0.y, ch.x, ch.y, matrixType));
		p2 = packRGBA8(YuvToRgbForPixel(l1.x, ch.x, ch.y, matrixType));
		p3 = packRGBA8(YuvToRgbForPixel(l1.y, ch.x, ch.y, matrixType));
	}

	surf2Dwrite(p0, outputSurfaceRef, (x) * 4, (y), cudaBoundaryModeZero);
	surf2Dwrite(p1, outputSurfaceRef, (x + 1) * 4, (y), cudaBoundaryModeZero);
	surf2Dwrite(p2, outputSurfaceRef, (x) * 4, (y + 1), cudaBoundaryModeZero);
	surf2Dwrite(p3, outputSurfaceRef, (x + 1) * 4, (y + 1), cudaBoundaryModeZero);
}

template<class YuvUnitx2>
__device__ void inline Yuv444ToRgb(const uint8_t* frame, int width, int height, int pitch, int matrixType, bool isABGR)
{
	unsigned int x = 2 * (blockDim.x * blockIdx.x + threadIdx.x);
	unsigned int y = (blockDim.y * blockIdx.y + threadIdx.y);

	if (x + 1 >= width || y >= height) {
		return;
	}

	uint8_t* pSrc = (uint8_t *)&frame[x * sizeof(YuvUnitx2) / 2 + y * pitch];
	// 4 Ys
	YuvUnitx2 l0 = *(YuvUnitx2 *)pSrc;
	// U and V components
	YuvUnitx2 ch1 = *(YuvUnitx2 *)(pSrc + (height * pitch));
	YuvUnitx2 ch2 = *(YuvUnitx2 *)(pSrc + (2 * height * pitch));

	uchar4 p0, p1;

	if (isABGR)
	{
		p0 = packABGR8(YuvToRgbForPixel(l0.x, ch1.x, ch2.x, matrixType));
		p1 = packABGR8(YuvToRgbForPixel(l0.y, ch1.y, ch2.y, matrixType));
	}
	else
	{
		p0 = packRGBA8(YuvToRgbForPixel(l0.x, ch1.x, ch2.x, matrixType));
		p1 = packRGBA8(YuvToRgbForPixel(l0.y, ch1.y, ch2.y, matrixType));
	}

	surf2Dwrite(p0, outputSurfaceRef, (x) * 4, (y), cudaBoundaryModeZero);
	surf2Dwrite(p1, outputSurfaceRef, (x + 1) * 4, (y), cudaBoundaryModeZero);
}

__device__ uchar4 packRGBA8(uchar3 value)
{
	return make_uchar4(value.x, value.y, value.z, 255);
}

__device__ uchar4 packABGR8(uchar3 value)
{
	return make_uchar4(255, value.z, value.y, value.x);
}

// Decode 16-bit depth value from quantized & sub-sampled YUV triplet.
// See: "Adapting Standard Video Codecs for Depth Streaming", Fabrizo Pece, Jan Kautz, Tim Weyrich.
__device__ float decodeDepth(float Ld, float Ha, float Hb)
{
	const int np = 512;
	const int w  = 65536;
	const float p = float(np) / float(w);

	const float pDiv2 = p / 2.0f;
	const float pDiv4 = p / 4.0f;
	const float pDiv8 = p / 8.0f;
	
	int   mL = __float2int_rd(4.0f * (Ld / p) - 0.5f) % 4;
	float L0 = Ld - fmod(Ld - pDiv8, p) + pDiv4 * mL - pDiv8;

	float deltaH;
	switch(mL) {
	case 0: deltaH = pDiv2 * Ha; break;
	case 1: deltaH = pDiv2 * Hb; break;
	case 2: deltaH = pDiv2 * (1.0f - Ha); break;
	case 3: deltaH = pDiv2 * (1.0f - Hb); break;
	}

	return L0 + deltaH;
}

extern "C" __global__ void NV12toRGBA(const uint8_t* frame, int width, int height, int pitch)
{
	YuvToRgb<uchar2>(frame, width, height, pitch, 0, false);
}

extern "C" __global__ void NV12toABGR(const uint8_t* frame, int width, int height, int pitch)
{
	YuvToRgb<uchar2>(frame, width, height, pitch, 0, true);
}

extern "C" __global__ void P016toRGBA(const uint8_t* frame, int width, int height, int pitch)
{
	YuvToRgb<ushort2>(frame, width, height, pitch, 4, false);
}

extern "C" __global__ void P016toABGR(const uint8_t* frame, int width, int height, int pitch)
{
	YuvToRgb<ushort2>(frame, width, height, pitch, 4, true);
}

extern "C" __global__ void YUV444toRGBA(const uint8_t* frame, int width, int height, int pitch)
{
	Yuv444ToRgb<uchar2>(frame, width, height, pitch, 0, false);
}

extern "C" __global__ void YUV444toABGR(const uint8_t* frame, int width, int height, int pitch)
{
	Yuv444ToRgb<uchar2>(frame, width, height, pitch, 0, true);
}

extern "C" __global__ void YUV444P16toRGBA(const uint8_t* frame, int width, int height, int pitch)
{
	Yuv444ToRgb<ushort2>(frame, width, height, pitch, 4, false);
}

extern "C" __global__ void YUV444P16toABGR(const uint8_t* frame, int width, int height, int pitch)
{
	Yuv444ToRgb<ushort2>(frame, width, height, pitch, 4, true);
}

extern "C" __global__ void NV12toR16(const uint8_t* frame, int width, int height, int pitch)
{
	unsigned int x = 2 * (blockDim.x * blockIdx.x + threadIdx.x);
	unsigned int y = 2 * (blockDim.y * blockIdx.y + threadIdx.y);

	if (x >= width || y >= height) {
		return;
	}

	float Y0 = frame[(y)* pitch + x] / 255.0f;
	float Y1 = frame[(y)* pitch + x + 1] / 255.0f;
	float Y2 = frame[(y + 1) * pitch + x] / 255.0f;
	float Y3 = frame[(y + 1) * pitch + x + 1] / 255.0f;

	const int chromaOffset = pitch * height;
	float U = frame[chromaOffset + (y >> 1) * pitch + x] / 255.0f;
	float V = frame[chromaOffset + (y >> 1) * pitch + x + 1] / 255.0f;

	surf2Dwrite(__float2half(decodeDepth(Y0, U, V)), outputSurfaceRef, (x) << 1, (y), cudaBoundaryModeZero);
	surf2Dwrite(__float2half(decodeDepth(Y1, U, V)), outputSurfaceRef, (x + 1) << 1, (y), cudaBoundaryModeZero);
	surf2Dwrite(__float2half(decodeDepth(Y2, U, V)), outputSurfaceRef, (x) << 1, (y + 1), cudaBoundaryModeZero);
	surf2Dwrite(__float2half(decodeDepth(Y3, U, V)), outputSurfaceRef, (x + 1) << 1, (y + 1), cudaBoundaryModeZero);
}
