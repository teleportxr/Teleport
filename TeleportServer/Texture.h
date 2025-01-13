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
		extern float GetCompressionProgress();
		extern bool CompressToKtx2(ExtractedTexture &extractedTexture,std::string assetPath, std::shared_ptr<PrecompressedTexture> compressionData);
		extern void LoadAsKtxFile( ExtractedTexture &textureData, const std::vector<char> &data,const std::string &filename);
		extern void LoadAsTeleportTexture(ExtractedTexture &textureData, const std::vector<char> &data,const std::string &filename);
		extern void LoadAsPng(ExtractedTexture &textureData, const std::vector<char> &data,const std::string &filename);
	}
}