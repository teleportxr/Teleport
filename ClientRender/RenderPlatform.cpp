#include "RenderPlatform.h"
#include "IndexBuffer.h"

#include "Texture.h"
#include "UniformBuffer.h"
#include "VertexBuffer.h"
#include "Skin.h"
using namespace clientrender;

std::shared_ptr<clientrender::IndexBuffer> RenderPlatform::InstantiateIndexBuffer() const
{
	return std::make_shared<clientrender::IndexBuffer>(this);
}

std::shared_ptr<clientrender::Skin> RenderPlatform::InstantiateSkin(const std::string& name) const
{
	return std::make_shared<clientrender::Skin>(name);
}

std::shared_ptr<clientrender::Skin> RenderPlatform::InstantiateSkin(const std::string& name, const std::vector<mat4>& inverseBindMatrices, size_t numBones, const Transform& skinTransform) const
{
	return std::make_shared<clientrender::Skin>(name, inverseBindMatrices, numBones, skinTransform);
}

std::shared_ptr<clientrender::Texture> RenderPlatform::InstantiateTexture() const
{
	return std::make_shared<clientrender::Texture>(this);
}

std::shared_ptr<clientrender::UniformBuffer> RenderPlatform::InstantiateUniformBuffer() const
{
	return std::make_shared<clientrender::UniformBuffer>(this);
}

std::shared_ptr<clientrender::VertexBuffer> RenderPlatform::InstantiateVertexBuffer() const
{
	return std::make_shared<clientrender::VertexBuffer>(this);
}
