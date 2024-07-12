/*
 * We save and load the IDs from disk, but then we need to re-assign them.
 * Otherwise we will end up with a lot of unused ID slots, and need to manually change the next unused ID to a safe value.
 */
#pragma once

#include <vector>
#include <string>
#include <memory>

namespace teleport
{
	namespace server
	{
		struct ExtractedTexture;
		struct PrecompressedTexture;
		extern bool ApplyBasisCompression(ExtractedTexture &extractedTexture,std::string path
											,std::shared_ptr<PrecompressedTexture> compressionData
											,uint8_t compressionStrength
											,uint8_t compressionQuality);
		extern void LoadAsPng(ExtractedTexture &textureData, const std::vector<char> &data,const std::string &filename);
		extern void LoadAsBasisFile(struct ExtractedTexture &textureData, const std::vector<char> &data, const std::string &filename);
	}
}