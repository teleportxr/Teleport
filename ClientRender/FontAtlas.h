// (C) Copyright 2018-2023 Simul Software Ltd
#pragma once

#include "Common.h"
#include "ResourceManager.h"
#include "TeleportCore/FontAtlas.h"
namespace platform
{
	namespace crossplatform
	{
		class Texture;
	}
}

namespace teleport
{
	namespace clientrender
	{
		class Texture;
		//! A font atlas.
		class FontAtlas : public teleport::core::FontAtlas, public IncompleteFontAtlas
		{
		public:
			FontAtlas(avs::uid u = 0, const std::string &url = "") : teleport::core::FontAtlas(u), IncompleteFontAtlas(u, url) {}
			std::shared_ptr<clientrender::Texture> fontTexture;
			std::string getName() const
			{
				return "FontAtlas";
			}
			static const char *getTypeName()
			{
				return "FontAtlas";
			}
			/// Save the Font atlas to the local file cache.
			virtual void Save(std::ostream &) const override;
			virtual const char *GetFileExtension() const override
			{
				return "atlas";
			}
		};
	}
}