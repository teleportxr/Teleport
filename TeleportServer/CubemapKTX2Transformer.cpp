#include "CubemapRearrangement.h"
#include "TeleportCore/Logging.h"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include <vkformat_enum.h>
#include <compressonator.h>

namespace
{
	// ---- Cube map face <-> direction convention (matches GPU/Vulkan/KTX sampling) ----
	// u,v are face coordinates in [-1,1]; u increases with the column (rightwards) and v
	// increases with the row (downwards), i.e. v follows the stored top-to-bottom row order.
	void FaceUVToDir(int face, float u, float v, float d[3])
	{
		switch (face)
		{
		case 0: d[0] =  1; d[1] = -v; d[2] = -u; break;	// +X
		case 1: d[0] = -1; d[1] = -v; d[2] =  u; break;	// -X
		case 2: d[0] =  u; d[1] =  1; d[2] =  v; break;	// +Y
		case 3: d[0] =  u; d[1] = -1; d[2] = -v; break;	// -Y
		case 4: d[0] =  u; d[1] = -v; d[2] =  1; break;	// +Z
		default:d[0] = -u; d[1] = -v; d[2] = -1; break;	// -Z
		}
	}

	// Inverse of FaceUVToDir: pick the major axis to choose the face, then recover u,v.
	void DirToFaceUV(const float d[3], int &face, float &u, float &v)
	{
		float ax = std::fabs(d[0]), ay = std::fabs(d[1]), az = std::fabs(d[2]);
		if (ax >= ay && ax >= az)
		{
			float ma = ax;
			if (d[0] > 0) { face = 0; u = -d[2] / ma; v = -d[1] / ma; }	// +X
			else          { face = 1; u =  d[2] / ma; v = -d[1] / ma; }	// -X
		}
		else if (ay >= ax && ay >= az)
		{
			float ma = ay;
			if (d[1] > 0) { face = 2; u =  d[0] / ma; v =  d[2] / ma; }	// +Y
			else          { face = 3; u =  d[0] / ma; v = -d[2] / ma; }	// -Y
		}
		else
		{
			float ma = az;
			if (d[2] > 0) { face = 4; u =  d[0] / ma; v = -d[1] / ma; }	// +Z
			else          { face = 5; u = -d[0] / ma; v = -d[1] / ma; }	// -Z
		}
	}

	// Basis change between coordinate systems, applied to a direction vector.
	// Engineering: +X=right, +Y=forward, +Z=up.  OpenGL: +X=right, +Y=up, +Z=back(towards viewer).
	//   eng->gl: (x, z, -y)      gl->eng: (x, -z, y)
	void DirEngToGl(const float in[3], float out[3]) { out[0] = in[0]; out[1] = in[2]; out[2] = -in[1]; }
	void DirGlToEng(const float in[3], float out[3]) { out[0] = in[0]; out[1] = -in[2]; out[2] = in[1]; }

	int PixelIndexFromCoord(float c, uint32_t N)
	{
		// c in [-1,1] -> nearest texel centre. For the exact 90-degree rotations used here this
		// lands precisely on a texel, so nearest sampling is lossless.
		int i = (int)std::lround((c + 1.0f) * 0.5f * (float)N - 0.5f);
		if (i < 0) i = 0;
		if (i >= (int)N) i = (int)N - 1;
		return i;
	}

	// Decode one BC6H face image (mip level) to RGBA16F (8 bytes/texel) using Compressonator.
	bool DecodeBC6HToRGBA16F(const uint8_t *bc6h, ktx_size_t bc6hSize, uint32_t w, uint32_t h, std::vector<uint8_t> &out)
	{
		CMP_Texture src{}; src.dwSize = sizeof(src); src.dwWidth = w; src.dwHeight = h; src.dwPitch = 0;
		src.format = CMP_FORMAT_BC6H; src.dwDataSize = CMP_CalculateBufferSize(&src);
		if (src.dwDataSize != bc6hSize)
		{
			TELEPORT_WARN("BC6H face size mismatch: expected {0}, got {1}", (uint64_t)src.dwDataSize, (uint64_t)bc6hSize);
			return false;
		}
		src.pData = const_cast<uint8_t *>(bc6h);

		CMP_Texture dst{}; dst.dwSize = sizeof(dst); dst.dwWidth = w; dst.dwHeight = h; dst.dwPitch = 0;
		dst.format = CMP_FORMAT_RGBA_16F; dst.dwDataSize = CMP_CalculateBufferSize(&dst);
		out.resize(dst.dwDataSize); dst.pData = out.data();

		CMP_CompressOptions opt{}; opt.dwSize = sizeof(opt); opt.fquality = 1.0f; opt.dwnumThreads = 0;
		return CMP_ConvertTexture(&src, &dst, &opt, nullptr) == CMP_OK;
	}

	// Encode one RGBA16F face image (mip level) to BC6H using Compressonator.
	bool EncodeRGBA16FToBC6H(const uint8_t *rgba16f, uint32_t w, uint32_t h, std::vector<uint8_t> &out)
	{
		CMP_Texture src{}; src.dwSize = sizeof(src); src.dwWidth = w; src.dwHeight = h; src.dwPitch = 0;
		src.format = CMP_FORMAT_RGBA_16F; src.dwDataSize = CMP_CalculateBufferSize(&src);
		src.pData = const_cast<uint8_t *>(rgba16f);

		CMP_Texture dst{}; dst.dwSize = sizeof(dst); dst.dwWidth = w; dst.dwHeight = h; dst.dwPitch = 0;
		dst.format = CMP_FORMAT_BC6H; dst.dwDataSize = CMP_CalculateBufferSize(&dst);
		out.resize(dst.dwDataSize); dst.pData = out.data();

		CMP_CompressOptions opt{}; opt.dwSize = sizeof(opt); opt.fquality = 1.0f; opt.dwnumThreads = 0;
		return CMP_ConvertTexture(&src, &dst, &opt, nullptr) == CMP_OK;
	}

	// Decode all six BC6H faces of one mip level to RGBA16F.
	bool DecodeMipFaces(const ktxTexture2 *source, uint32_t mip, uint32_t N, std::vector<std::vector<uint8_t>> &faces)
	{
		faces.resize(6);
		for (uint32_t f = 0; f < 6; ++f)
		{
			ktx_size_t offset = 0;
			if (ktxTexture_GetImageOffset(ktxTexture(const_cast<ktxTexture2 *>(source)), mip, 0, f, &offset) != KTX_SUCCESS)
				return false;
			ktx_size_t imageSize = ktxTexture_GetImageSize(ktxTexture(const_cast<ktxTexture2 *>(source)), mip);
			if (!DecodeBC6HToRGBA16F(source->pData + offset, imageSize, N, N, faces[f]))
				return false;
		}
		return true;
	}
}

using namespace teleport::server;

ktxTexture2* CubemapKTX2Transformer::LoadKTX2File(const std::string& path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file.is_open())
	{
		TELEPORT_WARN("Failed to open KTX2 file: {0}", path);
		return nullptr;
	}

	size_t fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<uint8_t> data(fileSize);
	file.read(reinterpret_cast<char*>(data.data()), fileSize);
	file.close();

	return LoadKTX2Memory(data.data(), data.size());
}

ktxTexture2* CubemapKTX2Transformer::LoadKTX2Memory(const uint8_t* data, size_t size)
{
	ktxTexture *ktxt = nullptr;
	KTX_error_code result = ktxTexture_CreateFromMemory(data, size,
		KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxt);
	
	if (result != KTX_SUCCESS)
	{
		TELEPORT_WARN("Failed to load KTX2 from memory: {0}", static_cast<int>(result));
		return nullptr;
	}

	ktxTexture2* ktx2 = reinterpret_cast<ktxTexture2*>(ktxt);
	if (!IsCubemap(ktx2))
	{
		TELEPORT_WARN("Loaded texture is not a cubemap");
		DestroyTexture(ktx2);
		return nullptr;
	}

	return ktx2;
}

ktxTexture2* CubemapKTX2Transformer::TransformFaceOrder(const ktxTexture2* source,
													  const Face faceMapping[6])
{
	if (!source)
		return nullptr;

	// Create new texture with same specifications
	ktxTextureCreateInfo info = {};
	info.vkFormat = source->vkFormat;
	info.baseWidth = source->baseWidth;
	info.baseHeight = source->baseHeight;
	info.baseDepth = source->baseDepth;
	info.numDimensions = source->numDimensions;
	info.numLevels = source->numLevels;
	info.numLayers = source->numLayers;
	info.numFaces = 6;
	info.isArray = KTX_FALSE;
	info.generateMipmaps = KTX_FALSE;

	ktxTexture2* dst = nullptr;
	KTX_error_code result = ktxTexture2_Create(&info,
		KTX_TEXTURE_CREATE_ALLOC_STORAGE, &dst);
	
	if (result != KTX_SUCCESS)
	{
		TELEPORT_WARN("Failed to create destination KTX2 texture");
		return nullptr;
	}

	// Copy face data according to mapping
	for (uint32_t m = 0; m < source->numLevels; ++m)
	{
		for (uint32_t l = 0; l < source->numLayers; ++l)
		{
			for (uint32_t dstFace = 0; dstFace < 6; ++dstFace)
			{
				uint32_t srcFace = (uint32_t)faceMapping[dstFace];
				if (srcFace >= 6)
				{
					TELEPORT_WARN("Invalid face mapping: {0}", srcFace);
					DestroyTexture(dst);
					return nullptr;
				}

				ktx_size_t srcOffset = 0;
				KTX_error_code offsetResult = ktxTexture_GetImageOffset(
					ktxTexture(source), m, l, srcFace, &srcOffset);

				if (offsetResult == KTX_SUCCESS)
				{
					// Size of a single cubemap face image at this mip level. Using the span to the
					// next mip level here would wrongly include every face from srcFace onwards.
					ktx_size_t imageSize = ktxTexture_GetImageSize(ktxTexture(source), m);
					const uint8_t* srcImage = source->pData + srcOffset;

					result = ktxTexture_SetImageFromMemory(ktxTexture(dst),
						m, l, dstFace, srcImage, imageSize);

					if (result != KTX_SUCCESS)
					{
						TELEPORT_WARN("Failed to set image data for face {0}", dstFace);
						DestroyTexture(dst);
						return nullptr;
					}
				}
			}
		}
	}

	return dst;
}

// Resample a BC6H cubemap into a different coordinate system. dstToSrcDir maps a direction
// expressed in the DESTINATION frame to the same physical direction in the SOURCE frame.
// Each destination texel's direction is rotated into the source frame and looked up there, so
// both the face reordering AND the per-face rotation are handled in one pass. Because the axis
// changes are exact 90-degree rotations, the lookup lands on exact source texels (lossless apart
// from one BC6H re-encode).
static ktxTexture2* ConvertAxes(const ktxTexture2* source, void(*dstToSrcDir)(const float[3], float[3]))
{
	if (!source)
		return nullptr;
	if (source->vkFormat != VK_FORMAT_BC6H_UFLOAT_BLOCK)
	{
		TELEPORT_WARN("ConvertAxes only supports BC6H cubemaps (vkFormat {0}); cannot rotate faces.",
			(uint32_t)source->vkFormat);
		return nullptr;
	}

	ktxTextureCreateInfo info = {};
	info.vkFormat = source->vkFormat;
	info.baseWidth = source->baseWidth;
	info.baseHeight = source->baseHeight;
	info.baseDepth = source->baseDepth;
	info.numDimensions = source->numDimensions;
	info.numLevels = source->numLevels;
	info.numLayers = source->numLayers;
	info.numFaces = 6;
	info.isArray = KTX_FALSE;
	info.generateMipmaps = KTX_FALSE;

	ktxTexture2* dst = nullptr;
	if (ktxTexture2_Create(&info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &dst) != KTX_SUCCESS)
	{
		TELEPORT_WARN("Failed to create destination KTX2 texture");
		return nullptr;
	}

	for (uint32_t m = 0; m < source->numLevels; ++m)
	{
		uint32_t N = std::max(1u, source->baseWidth >> m);
		std::vector<std::vector<uint8_t>> faces;
		if (!DecodeMipFaces(source, m, N, faces))
		{
			TELEPORT_WARN("Failed to decode source faces at mip {0}", m);
			CubemapKTX2Transformer::DestroyTexture(dst);
			return nullptr;
		}

		std::vector<uint8_t> dstFace(N * N * 8);	// RGBA16F = 8 bytes/texel
		std::vector<uint8_t> encoded;
		for (uint32_t dstF = 0; dstF < 6; ++dstF)
		{
			for (uint32_t y = 0; y < N; ++y)
			{
				float v = ((float)y + 0.5f) / (float)N * 2.0f - 1.0f;
				for (uint32_t x = 0; x < N; ++x)
				{
					float u = ((float)x + 0.5f) / (float)N * 2.0f - 1.0f;
					float dd[3], sd[3];
					FaceUVToDir((int)dstF, u, v, dd);
					dstToSrcDir(dd, sd);
					int srcF; float su, sv;
					DirToFaceUV(sd, srcF, su, sv);
					int sx = PixelIndexFromCoord(su, N);
					int sy = PixelIndexFromCoord(sv, N);
					memcpy(&dstFace[((size_t)y * N + x) * 8],
						   &faces[srcF][((size_t)sy * N + sx) * 8], 8);
				}
			}

			if (!EncodeRGBA16FToBC6H(dstFace.data(), N, N, encoded))
			{
				TELEPORT_WARN("Failed to encode face {0} at mip {1}", dstF, m);
				CubemapKTX2Transformer::DestroyTexture(dst);
				return nullptr;
			}
			if (ktxTexture_SetImageFromMemory(ktxTexture(dst), m, 0, dstF, encoded.data(), encoded.size()) != KTX_SUCCESS)
			{
				TELEPORT_WARN("Failed to set image data for face {0} at mip {1}", dstF, m);
				CubemapKTX2Transformer::DestroyTexture(dst);
				return nullptr;
			}
		}
	}
	return dst;
}

ktxTexture2* CubemapKTX2Transformer::ConvertEngineeringToOpenGL(const ktxTexture2* source)
{
	// Destination is OpenGL; map each OpenGL direction back to Engineering to look it up.
	return ConvertAxes(source, DirGlToEng);
}

ktxTexture2* CubemapKTX2Transformer::ConvertOpenGLToEngineering(const ktxTexture2* source)
{
	// Destination is Engineering; map each Engineering direction to OpenGL to look it up.
	return ConvertAxes(source, DirEngToGl);
}

bool CubemapKTX2Transformer::DecodeFacesRGBA16F(const ktxTexture2* source, uint32_t mip,
												uint32_t& faceWidth, std::vector<std::vector<uint8_t>>& faces)
{
	if (!source || source->vkFormat != VK_FORMAT_BC6H_UFLOAT_BLOCK)
	{
		TELEPORT_WARN("DecodeFacesRGBA16F: source is not a BC6H cubemap");
		return false;
	}
	faceWidth = std::max(1u, source->baseWidth >> mip);
	return DecodeMipFaces(source, mip, faceWidth, faces);
}

bool CubemapKTX2Transformer::SaveKTX2File(const ktxTexture2* texture, const std::string& path)
{
	std::vector<uint8_t> buffer;
	if (!SaveKTX2Memory(texture, buffer))
		return false;

	std::ofstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		TELEPORT_WARN("Failed to open output file: {0}", path);
		return false;
	}

	file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
	file.close();
	return true;
}

bool CubemapKTX2Transformer::SaveKTX2Memory(const ktxTexture2* texture, 
										   std::vector<uint8_t>& outBuffer)
{
	uint8_t* data = nullptr;
	ktx_size_t size = 0;
	
	KTX_error_code result = ktxTexture_WriteToMemory(ktxTexture(texture), &data, &size);
	if (result != KTX_SUCCESS || !data)
	{
		TELEPORT_WARN("Failed to serialize KTX2 texture");
		return false;
	}

	outBuffer.resize(size);
	memcpy(outBuffer.data(), data, size);
	free(data);
	return true;
}

void CubemapKTX2Transformer::DestroyTexture(ktxTexture2* texture)
{
	if (texture)
		ktxTexture_Destroy(ktxTexture(texture));
}

bool CubemapKTX2Transformer::IsCubemap(const ktxTexture2* texture)
{
	return texture && texture->isCubemap;
}

void CubemapKTX2Transformer::GetDimensions(const ktxTexture2* texture,
										   uint32_t& width, uint32_t& height, uint32_t& mipCount)
{
	if (texture)
	{
		width = texture->baseWidth;
		height = texture->baseHeight;
		mipCount = texture->numLevels;
	}
}
