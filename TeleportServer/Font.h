#pragma once
#include "libavstream/common.hpp"

#include <string>
#include <map>
#include "UnityPlugin/InteropStructures.h"
#include "TeleportCore/FontAtlas.h"

namespace avs
{
	struct Texture;
}
namespace teleport
{
	namespace server
	{
		class Font
		{
			std::map<std::string, InteropFontAtlas> interopFontAtlases;
		public:
			static Font& GetInstance();
			~Font();
			//! Extract and save the font at ttf_path_utf8 to the asset path
			//! at asset_path_utf8 with the given sizes. Also write the avsTexture with the atlas
			//! bitmap.
			static bool ExtractFont(core::FontAtlas& fontAtlas, std::string ttf_path_utf8
				, std::string asset_path_utf8
				, std::string atlas_chars
				, avs::Texture& avsTexture
				, std::vector<int> sizes);
			//! Free the memory that was allocated.
			static void Free(avs::Texture& avsTexture);
			//! Interop
			bool GetInteropFontAtlas(std::string path, InteropFontAtlas* interopFontAtlas);
		};
	}
}