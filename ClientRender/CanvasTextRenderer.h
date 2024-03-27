// (C) Copyright 2018-2024 Simul Software Ltd
#pragma once
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "Platform/CrossPlatform/Shaders/camera_constants.sl"
#include "Platform/CrossPlatform/Shaders/text_constants.sl"
#include "client/Shaders/pbr_constants.sl"

namespace teleport
{
	namespace clientrender
	{
		class FontAtlas;
		struct CanvasRender;
		//! Renderer that draws a Portal CanvasText.
		//! There will be one instance of this.
		class CanvasTextRenderer 
		{
		protected:
			platform::crossplatform::Effect *effect = nullptr;
			platform::crossplatform::RenderPlatform *renderPlatform = nullptr;
			bool reload_shaders = true;
			platform::crossplatform::EffectTechnique *tech = nullptr;
			platform::crossplatform::EffectTechnique *link_tech = nullptr;
			platform::crossplatform::EffectPass *singleViewPass = nullptr;
			platform::crossplatform::EffectPass *multiViewPass = nullptr;
			platform::crossplatform::EffectPass *link_singleViewPass = nullptr;
			platform::crossplatform::EffectPass *link_multiViewPass = nullptr;
			platform::crossplatform::ShaderResource textureResource;
			platform::crossplatform::ShaderResource _fontChars;
			platform::crossplatform::ConstantBuffer<TextConstants> textConstants;
			void Recompile();
			size_t count=0;
		public:
			CanvasTextRenderer();
			virtual ~CanvasTextRenderer();
			void RestoreDeviceObjects(platform::crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			void Render(platform::crossplatform::GraphicsDeviceContext &deviceContext, const CanvasRender *canvasRender);
			void Render(platform::crossplatform::GraphicsDeviceContext &deviceContext, const clientrender::FontAtlas *fontAtlas, int size, const std::string &text, vec4 colour, vec4 canvas, float lineHeight, platform::crossplatform::StructuredBuffer<FontChar> &fontChars,bool link=false);
		};
	}
}