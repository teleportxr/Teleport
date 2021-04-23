#pragma once

#include "crossplatform/Skin.h"

#include "GL_RenderPlatform.h"

namespace scc
{
class GL_Skin : public scr::Skin
{
public:
	GL_Skin(const scc::GL_RenderPlatform *renderPlatform, const std::string& name);
	GL_Skin(const scc::GL_RenderPlatform *renderPlatform, const std::string& name, const std::vector<scr::mat4>& inverseBindMatrices, size_t boneAmount, const scr::Transform& skinTransform);

	virtual ~GL_Skin() = default;

	virtual void UpdateBoneMatrices(const scr::mat4 &rootTransform) override;

	inline const scr::ShaderResource &GetShaderResource() const
	{ return shaderResource; }

private:
	scr::ShaderResource shaderResource;
	scr::ShaderResourceLayout shaderResourceLayout;
	std::shared_ptr<scr::UniformBuffer> uniformBuffer;

	void CreateUniformBuffer(const scc::GL_RenderPlatform *renderPlatform);
};
}