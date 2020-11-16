#pragma once

#include "crossplatform/Skin.h"

#include "GL_RenderPlatform.h"

namespace scc
{
class GL_Skin : public scr::Skin
{
public:
	GL_Skin(const scc::GL_RenderPlatform *renderPlatform);

	virtual ~GL_Skin() = default;

	virtual void UpdateBoneMatrices(const scr::mat4 &rootTransform) override;

	inline const scr::ShaderResource &GetShaderResource() const
	{ return shaderResource; }

private:
	scr::ShaderResource shaderResource;
	scr::ShaderResourceLayout shaderResourceLayout;
	std::shared_ptr<scr::UniformBuffer> uniformBuffer;
};
}