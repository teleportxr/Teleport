// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "Common.h"

#include "ClientRender/API.h"
#include "TeleportClient/basic_linear_algebra.h"

namespace platform
{
	namespace crossplatform
	{
		class RenderPlatform;
	}
}

namespace clientrender
{
	class FrameBuffer;
	class IndexBuffer;
	class Skin;
	class Texture;
	class Transform;
	class UniformBuffer;
	class VertexBuffer;
	class Material;

	class RenderPlatform
	{
	public:
		virtual ~RenderPlatform() {}

		virtual std::shared_ptr<IndexBuffer>			InstantiateIndexBuffer() const;
		virtual std::shared_ptr<Skin>					InstantiateSkin(const std::string& name) const;
		virtual std::shared_ptr<Skin>					InstantiateSkin(const std::string& name, const std::vector<mat4>& inverseBindMatrices, size_t numBones, const Transform& skinTransform) const;
		virtual std::shared_ptr<Texture>				InstantiateTexture() const;
		virtual std::shared_ptr<UniformBuffer>			InstantiateUniformBuffer() const;
		virtual std::shared_ptr<VertexBuffer>			InstantiateVertexBuffer() const;

		std::shared_ptr<clientrender::Material> placeholderMaterial;

		void SetSimulRenderPlatform(platform::crossplatform::RenderPlatform *r)
		{
			renderPlatform = r;
		}
		platform::crossplatform::RenderPlatform *GetSimulRenderPlatform() const
		{
			return renderPlatform;
		}
	protected:
		platform::crossplatform::RenderPlatform* renderPlatform = nullptr;
	};
}