// (C) Copyright 2018-2024 Simul Software Ltd
#pragma once
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "Platform/CrossPlatform/Shaders/camera_constants.sl"
#include "client/Shaders/pbr_constants.sl"
#include "Platform/CrossPlatform/Shaders/text_constants.sl"

namespace avs
{
	typedef uint64_t uid;
}
namespace teleport
{
	namespace clientrender
	{
		struct LinkRender
		{
			avs::uid uid;
			mat4 *model;
			vec3 position;
			std::string url;
			mutable float time=0.f;
			platform::crossplatform::StructuredBuffer<FontChar> fontChars;
		};
		//! Renderer that draws a Portal Link.
		//! There will be one instance of this.
		class LinkRenderer 
		{
		protected:
			std::shared_ptr<platform::crossplatform::Effect> linkEffect = nullptr;
			platform::crossplatform::EffectTechnique *link_tech=nullptr;
			platform::crossplatform::RenderPlatform *renderPlatform = nullptr;
			bool reload_shaders=true;

			platform::crossplatform::ConstantBuffer<LinkConstants> linkConstants;
		public:
			LinkRenderer();
			virtual ~LinkRenderer();
			void RestoreDeviceObjects(platform::crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			void RenderLink(platform::crossplatform::GraphicsDeviceContext &deviceContext, const LinkRender &linkRender,bool highlight);
		};
	}
}