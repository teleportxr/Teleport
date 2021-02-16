// (C) Copyright 2018-2019 Simul Software Ltd

#include "ShaderResource.h"

using namespace scr;

ShaderResource::ShaderResource(const ShaderResourceLayout& shaderResourceLayout)
{
	SetLayout(shaderResourceLayout);
}

void ShaderResource::SetLayout(const ShaderResourceLayout& shaderResourceLayout)
{
	m_ShaderResourceLayout = shaderResourceLayout;
}

ShaderResource::~ShaderResource()
{
}

void ShaderResource::AddBuffer( ShaderResourceLayout::ShaderResourceType shaderResourceType, uint32_t bindingIndex, const char* shaderResourceName, const ShaderResourceBufferInfo& bufferInfo, uint32_t dstArrayElement)
{
	WriteShaderResource wsr;
	wsr.shaderResourceName = shaderResourceName;
	//wsr.dstSet = shaderResourceLayoutIndex;
	wsr.dstBinding = bindingIndex;
	wsr.dstArrayElement = 0;
	wsr.shaderResourceCount = m_ShaderResourceLayout.FindShaderResourceLayout(bindingIndex).count;
	wsr.shaderResourceType = shaderResourceType;
	wsr.imageInfo = { nullptr, nullptr };
	wsr.bufferInfo = bufferInfo;

	m_WriteShaderResources.push_back(wsr);
}

void ShaderResource::AddImage( ShaderResourceLayout::ShaderResourceType shaderResourceType, uint32_t bindingIndex, const char* shaderResourceName, const ShaderResourceImageInfo& imageInfo, uint32_t dstArrayElement)
{
	WriteShaderResource wsr;
	wsr.shaderResourceName = shaderResourceName;
	//wsr.dstSet = shaderResourceLayoutIndex;
	wsr.dstBinding = bindingIndex;
	wsr.dstArrayElement = 0;
	wsr.shaderResourceCount = m_ShaderResourceLayout.FindShaderResourceLayout(bindingIndex).count;
	wsr.shaderResourceType = shaderResourceType;
	wsr.imageInfo = imageInfo;
	wsr.bufferInfo = { nullptr, 0, 0 };

	m_WriteShaderResources.push_back(wsr);
}

void ShaderResource::SetImageInfo( size_t index, const ShaderResourceImageInfo& imageInfo)
{
	size_t idx=0;
	for (auto& this_wsr : m_WriteShaderResources)
	{
		if(idx==index)
		{
			this_wsr.imageInfo=imageInfo;
			return;
		}
		idx++;
	}
}
