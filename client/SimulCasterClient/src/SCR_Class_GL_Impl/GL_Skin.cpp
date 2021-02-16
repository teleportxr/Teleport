#include "GL_Skin.h"

namespace scc
{
GL_Skin::GL_Skin(const scc::GL_RenderPlatform *renderPlatform, const std::string& name)
	:scr::Skin(name)
{
	CreateUniformBuffer(renderPlatform);
}

GL_Skin::GL_Skin(const scc::GL_RenderPlatform *renderPlatform, const std::string& name, const std::vector<scr::Transform>& inverseBindMatrices, size_t boneAmount, const scr::Transform& skinTransform)
	:scr::Skin(name, inverseBindMatrices, boneAmount, skinTransform)
{
	CreateUniformBuffer(renderPlatform);
}

void GL_Skin::UpdateBoneMatrices(const scr::mat4 &rootTransform)
{
	scr::Skin::UpdateBoneMatrices(rootTransform);

	uniformBuffer->Update();
}

void GL_Skin::CreateUniformBuffer(const scc::GL_RenderPlatform *renderPlatform)
{
	size_t boneMatricesSize = sizeof(scr::mat4) * MAX_BONES;
	uint32_t bindingIndex = 4;

	//Set up uniform buffer.
	scr::UniformBuffer::UniformBufferCreateInfo bufferCreateInfo;
	bufferCreateInfo.bindingLocation = bindingIndex;
	bufferCreateInfo.size = boneMatricesSize;
	bufferCreateInfo.data = GetBoneMatrices();

	uniformBuffer = renderPlatform->InstantiateUniformBuffer();
	uniformBuffer->Create(&bufferCreateInfo);

	shaderResourceLayout.AddBinding(bindingIndex, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, scr::Shader::Stage::SHADER_STAGE_VERTEX);

	shaderResource = scr::ShaderResource({shaderResourceLayout});
	shaderResource.AddBuffer( scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, bindingIndex, "u_BoneData", {uniformBuffer.get(), 0, boneMatricesSize});
}
}