#pragma once

// (C) Copyright 2018-2023 Simul Software Ltd
#pragma once
 
#include "Common.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "Platform/CrossPlatform/Shaders/text_constants.sl"
#include "Platform/CrossPlatform/Shaders/camera_constants.sl"
#include "ClientRender/FontAtlas.h"

namespace platform
{
	namespace crossplatform
	{
		class RenderPlatform;
	}
}
namespace clientrender
{
	struct TextCanvasCreateInfo
	{
		avs::uid server_uid=0;
		avs::uid uid=0;
		avs::uid font=0;
		int size=0;
		float lineHeight=0.0f;
		float width=0;
		float height=0;
		vec4 colour={0,0,0,0};
		std::string text;
	};
	//! A text canvas rendering component.
	class TextCanvas : public IncompleteTextCanvas
	{
		static void RestoreCommonDeviceObjects(platform::crossplatform::RenderPlatform *r);
		static void InvalidateCommonDeviceObjects();
	public:
		TextCanvas(const TextCanvasCreateInfo &t);
		~TextCanvas();
		std::string getName() const
		{
			return "TextCanvas";
		}
		TextCanvasCreateInfo textCanvasCreateInfo;
		
		void Render(platform::crossplatform::GraphicsDeviceContext &deviceContext
				, platform::crossplatform::ConstantBuffer<CameraConstants, platform::crossplatform::ResourceUsageFrequency::FEW_PER_FRAME> &cameraConstants
				, platform::crossplatform::ConstantBuffer<StereoCameraConstants, platform::crossplatform::ResourceUsageFrequency::FEW_PER_FRAME> &stereoCameraConstants
				,platform::crossplatform::Texture *fontTexture);
		void RestoreDeviceObjects(platform::crossplatform::RenderPlatform *r);
		void InvalidateDeviceObjects();

		void SetFontAtlas(std::shared_ptr<clientrender::FontAtlas> f);
		static void RecompileShaders()
		{
			recompile=true;
		}
	protected:
		static void Recompile();

		static size_t count;
		static platform::crossplatform::RenderPlatform *renderPlatform;
		std::shared_ptr<clientrender::FontAtlas> fontAtlas;
		static platform::crossplatform::Effect						*effect;
		static platform::crossplatform::EffectTechnique				*tech;
		static platform::crossplatform::EffectPass					*singleViewPass;
		static platform::crossplatform::EffectPass					*multiViewPass;
		static platform::crossplatform::ShaderResource				textureResource;
		static platform::crossplatform::ShaderResource				_fontChars;
		static platform::crossplatform::ConstantBuffer<TextConstants>	textConstants;
	
		platform::crossplatform::StructuredBuffer<FontChar> fontChars;
		static bool recompile;
	};
}