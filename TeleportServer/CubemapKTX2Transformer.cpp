#include "CubemapRearrangement.h"
#include "TeleportCore/Logging.h"
#include <fstream>
#include <algorithm>

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

				ktx_size_t imageSize = 0;
				ktx_size_t levelIndex = 0;
				KTX_error_code offsetResult = ktxTexture_GetImageOffset(
					ktxTexture(source), m, l, srcFace, &levelIndex);

				if (offsetResult == KTX_SUCCESS)
				{
					// Get the image size from source texture levelIndex
					ktx_size_t nextLevelIndex = 0;
					if (m + 1 < source->numLevels)
					{
						ktxTexture_GetImageOffset(ktxTexture(source), m + 1, 0, 0, &nextLevelIndex);
					}
					else
					{
						nextLevelIndex = source->dataSize;
					}
					imageSize = nextLevelIndex - levelIndex;
					const uint8_t* srcImage = source->pData + levelIndex;

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

ktxTexture2* CubemapKTX2Transformer::ConvertEngineeringToOpenGL(const ktxTexture2* source)
{
	const Face engineeringToOpenGL[6] = {
		Face::POS_X,	// dst[0] ← src[0] (+X)
		Face::NEG_X,	// dst[1] ← src[1] (-X)
		Face::POS_Z,	// dst[2] ← src[4] (+Y ← +Z)
		Face::NEG_Z,	// dst[3] ← src[5] (-Y ← -Z)
		Face::NEG_Y,	// dst[4] ← src[3] (+Z ← -Y)
		Face::POS_Y		// dst[5] ← src[2] (-Z ← +Y)
	};
	return TransformFaceOrder(source, engineeringToOpenGL);
}

ktxTexture2* CubemapKTX2Transformer::ConvertOpenGLToEngineering(const ktxTexture2* source)
{
	const Face openGLToEngineering[6] = {
		Face::POS_X,	// dst[0] ← src[0] (+X)
		Face::NEG_X,	// dst[1] ← src[1] (-X)
		Face::NEG_Y,	// dst[2] ← src[5] (+Y ← -Z)
		Face::POS_Y,	// dst[3] ← src[3] (-Y ← +Z)
		Face::POS_Z,	// dst[4] ← src[2] (+Z ← +Y)
		Face::NEG_Z		// dst[5] ← src[4] (-Z ← -Y)
	};
	return TransformFaceOrder(source, openGLToEngineering);
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
