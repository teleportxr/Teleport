#pragma once

// (C) Copyright 2018-2023 Simul Software Ltd
#pragma once

#include "ClientRender/FontAtlas.h"
#include "Common.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "Platform/CrossPlatform/Shaders/camera_constants.sl"
#include "Platform/CrossPlatform/Shaders/text_constants.sl"
#include "Platform/CrossPlatform/Texture.h"

namespace platform
{
	namespace crossplatform
	{
		class RenderPlatform;
	}
}
namespace teleport
{
	namespace clientrender
	{
		class TextCanvas;
		struct CanvasRender
		{
			mat4 *model;
			std::shared_ptr<TextCanvas> textCanvas;
		};
		struct TextCanvasCreateInfo
		{
			avs::uid server_uid = 0;
			avs::uid uid = 0;
			avs::uid font = 0;
			int size = 0;
			float lineHeight = 0.0f;
			float width = 0;
			float height = 0;
			vec4 colour = {0, 0, 0, 0};
			std::string text;
		};
		//! A text canvas rendering component.
		class TextCanvas : public IncompleteTextCanvas
		{

		public:
			TextCanvas(const TextCanvasCreateInfo &t);
			~TextCanvas();
			std::string getName() const
			{
				return "TextCanvas";
			}

			static const char *getTypeName()
			{
				return "TextCanvas";
			}
			TextCanvasCreateInfo textCanvasCreateInfo;
			void RestoreDeviceObjects(platform::crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();

			void SetFontAtlas(std::shared_ptr<clientrender::FontAtlas> f);

			std::shared_ptr<clientrender::FontAtlas> fontAtlas;
			platform::crossplatform::StructuredBuffer<FontChar> fontChars;
		};
	}
}