#include "GL_Skin.h"

namespace scc
{
GL_Skin::GL_Skin(const scc::GL_RenderPlatform *renderPlatform, const std::string& name)
	:clientrender::Skin(name)
{
	CreateUniformBuffer(renderPlatform);
}

GL_Skin::GL_Skin(const scc::GL_RenderPlatform *renderPlatform, const std::string& name, const std::vector<clientrender::mat4>& inverseBindMatrices, size_t boneAmount, const clientrender::Transform& skinTransform)
	:clientrender::Skin(name, inverseBindMatrices, boneAmount, skinTransform)
{
	CreateUniformBuffer(renderPlatform);
}

void GL_Skin::UpdateBoneMatrices(const clientrender::mat4 &rootTransform)
{
	clientrender::Skin::UpdateBoneMatrices(rootTransform);
	uniformBuffer->Update();
}

void GL_Skin::CreateUniformBuffer(const scc::GL_RenderPlatform *renderPlatform)
{
	//return;
	size_t boneMatricesSize = sizeof(clientrender::mat4) * MAX_BONES;
	uint32_t bindingIndex = 4;

	//Set up uniform buffer.
	clientrender::UniformBuffer::UniformBufferCreateInfo bufferCreateInfo;
	bufferCreateInfo.name="u_BoneData";
	bufferCreateInfo.bindingLocation = bindingIndex;
	bufferCreateInfo.size = boneMatricesSize;
	bufferCreateInfo.data = GetBoneMatrices();

	uniformBuffer = renderPlatform->InstantiateUniformBuffer();
	uniformBuffer->Create(&bufferCreateInfo);

	shaderResourceLayout.AddBinding(bindingIndex, clientrender::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, clientrender::Shader::Stage::SHADER_STAGE_VERTEX);

	shaderResource = clientrender::ShaderResource({shaderResourceLayout});
	shaderResource.AddBuffer( clientrender::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, bindingIndex, "u_BoneData", {uniformBuffer.get(), 0, boneMatricesSize});
}
}