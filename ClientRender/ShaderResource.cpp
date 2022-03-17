// (C) Copyright 2018-2022 Simul Software Ltd

#include "ShaderResource.h"
#include "TeleportClient/Log.h"

using namespace clientrender;

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

ShaderResourceLayout::ShaderResourceLayoutBinding& ShaderResourceLayout::FindShaderResourceLayout(uint32_t bindingIndex)
{
	size_t index = (size_t)-1;
	for (size_t i = 0; i < m_LayoutBindings.size(); i++)
	{
		if (m_LayoutBindings[i].binding == bindingIndex)
		{
			index = i;
			break;
		}
	}
	if (index == (size_t)-1)
	{
		SCR_LOG("Warning: Layout %d not found in the %d layout bindings.",bindingIndex,(int)m_LayoutBindings.size());
		TELEPORT_CERR_BREAK("Could not find DescriptorSetLayoutBinding at binding index: " << bindingIndex << ".", -1);
		throw;
	}
	return m_LayoutBindings[index];
}