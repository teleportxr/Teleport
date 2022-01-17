#pragma once

#include "ClientRender/Skin.h"

#include "GL_RenderPlatform.h"

namespace scc
{
class GL_Skin : public clientrender::Skin
{
public:
	GL_Skin(const scc::GL_RenderPlatform *renderPlatform, const std::string& name);
	GL_Skin(const scc::GL_RenderPlatform *renderPlatform, const std::string& name, const std::vector<clientrender::mat4>& inverseBindMatrices, size_t boneAmount, const clientrender::Transform& skinTransform);

	virtual ~GL_Skin() = default;

	virtual void UpdateBoneMatrices(const clientrender::mat4 &rootTransform) override;

	inline const clientrender::ShaderResource &GetShaderResource() const
	{ return shaderResource; }

private:
	clientrender::ShaderResource shaderResource;
	clientrender::ShaderResourceLayout shaderResourceLayout;
	std::shared_ptr<clientrender::UniformBuffer> uniformBuffer;

	void CreateUniformBuffer(const scc::GL_RenderPlatform *renderPlatform);
};
}