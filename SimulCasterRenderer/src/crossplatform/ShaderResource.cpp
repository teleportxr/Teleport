// (C) Copyright 2018-2019 Simul Software Ltd

#include "ShaderResource.h"

using namespace scr;

ShaderResource::ShaderResource(const std::vector<ShaderResourceLayout>& shaderResourceLayouts)
{
	SetLayouts(shaderResourceLayouts);
}

void ShaderResource::SetLayouts(const std::vector<ShaderResourceLayout>& shaderResourceLayouts)
{
	uint32_t shaderResourceKey = 0;
	for (auto& shaderResourceLayout : shaderResourceLayouts)
	{
		m_ShaderResourceLayouts[shaderResourceKey] = shaderResourceLayout;
		shaderResourceKey++;
	}
}

ShaderResource::~ShaderResource()
{
	m_ShaderResourceLayouts.clear();
}

void ShaderResource::AddBuffer(uint32_t shaderResourceLayoutIndex, ShaderResourceLayout::ShaderResourceType shaderResourceType, uint32_t bindingIndex, const char* shaderResourceName, const ShaderResourceBufferInfo& bufferInfo, uint32_t dstArrayElement)
{
	WriteShaderResource wsr;
	wsr.shaderResourceName = shaderResourceName;
	wsr.dstSet = shaderResourceLayoutIndex;
	wsr.dstBinding = bindingIndex;
	wsr.dstArrayElement = 0;
	wsr.shaderResourceCount = m_ShaderResourceLayouts[shaderResourceLayoutIndex].FindShaderResourceLayout(bindingIndex).count;
	wsr.shaderResourceType = shaderResourceType;
	wsr.imageInfo = { nullptr, nullptr };
	wsr.bufferInfo = bufferInfo;

	m_WriteShaderResources.push_back(wsr);
}

void ShaderResource::AddImage(uint32_t shaderResourceLayoutIndex, ShaderResourceLayout::ShaderResourceType shaderResourceType, uint32_t bindingIndex, const char* shaderResourceName, const ShaderResourceImageInfo& imageInfo, uint32_t dstArrayElement)
{
	WriteShaderResource wsr;
	wsr.shaderResourceName = shaderResourceName;
	wsr.dstSet = shaderResourceLayoutIndex;
	wsr.dstBinding = bindingIndex;
	wsr.dstArrayElement = 0;
	wsr.shaderResourceCount = m_ShaderResourceLayouts[shaderResourceLayoutIndex].FindShaderResourceLayout(bindingIndex).count;
	wsr.shaderResourceType = shaderResourceType;
	wsr.imageInfo = imageInfo;
	wsr.bufferInfo = { nullptr, 0, 0 };

	m_WriteShaderResources.push_back(wsr);
}

ShaderResource ShaderResource::GetShaderResourcesBySet(size_t shaderResourceSetIndex)
{
	ShaderResourceLayout& shaderResourceLayout = m_ShaderResourceLayouts.find(shaderResourceSetIndex)->second;
	const std::vector<WriteShaderResource>& this_wsrs= this->GetWriteShaderResources();

	ShaderResource result({shaderResourceLayout});
	result.GetWriteShaderResources().clear();
	for (auto& this_wsr : this_wsrs)
	{
		if (this_wsr.dstSet == shaderResourceSetIndex)
		{
			result.GetWriteShaderResources().push_back(this_wsr);
		}
		else
			continue;
	}
	return result;
}
