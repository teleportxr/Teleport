// (C) Copyright 2018-2019 Simul Software Ltd

#include "DescriptorSet.h"

using namespace scr;

DescriptorSet::DescriptorSet(const std::vector<DescriptorSetLayout>& descriptorSetLayouts)
{
	uint32_t descriptorSetKey = 0;
	for (auto& descSetLayout : descriptorSetLayouts)
	{
		m_Sets[descriptorSetKey] = descSetLayout;
		descriptorSetKey++;
	}
}

DescriptorSet::~DescriptorSet()
{
	m_Sets.clear();
}

void DescriptorSet::AddBuffer(uint32_t descriptorSetIndex, DescriptorSetLayout::DescriptorType descriptorType, uint32_t bindingIndex, const DescriptorBufferInfo& bufferInfo, uint32_t dstArrayElement)
{
	WriteDescriptorSet wds;
	wds.dstSet = descriptorSetIndex;
	wds.dstBinding = bindingIndex;
	wds.dstArrayElement = 0;
	wds.descriptorCount = m_Sets[descriptorSetIndex].FindDescriptorSetLayout(bindingIndex).count;
	wds.descriptorType = descriptorType;
	wds.pImageInfo = nullptr;
	wds.pBufferInfo = &bufferInfo;

	m_WriteDescriptorSets.push_back(wds);
}
void DescriptorSet::AddImage(uint32_t descriptorSetIndex, DescriptorSetLayout::DescriptorType descriptorType, uint32_t bindingIndex, const DescriptorImageInfo& imageInfo, uint32_t dstArrayElement)
{
	WriteDescriptorSet wds;
	wds.dstSet = descriptorSetIndex;
	wds.dstBinding = bindingIndex;
	wds.dstArrayElement = 0;
	wds.descriptorCount = m_Sets[descriptorSetIndex].FindDescriptorSetLayout(bindingIndex).count;
	wds.descriptorType = descriptorType;
	wds.pImageInfo = &imageInfo;
	wds.pBufferInfo = nullptr;

	m_WriteDescriptorSets.push_back(wds);
}

/*void DescriptorSet::Update()
{
	for (auto& wds : m_WriteDescriptorSets)
	{
		if (wds.pBufferInfo != nullptr)
		{
			wds.pBufferInfo->buffer->Update(wds.pBufferInfo->offset, wds.pBufferInfo->range, nullptr);
		}
		else if (wds.pImageInfo != nullptr)
		{
			wds.pImageInfo->texture->UseSampler(wds.pImageInfo->sampler);
			wds.pImageInfo->texture->Bind();
		}
		else
			SCR_COUT_BREAK("Invalid WriteDescriptorSet.")
	}
}*/