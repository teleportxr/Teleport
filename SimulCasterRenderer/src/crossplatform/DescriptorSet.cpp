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

void DescriptorSet::AddBuffer(uint32_t descriptorSetIndex, DescriptorSetLayout::DescriptorType descriptorType, uint32_t bindingIndex, const char* descriptorName, const DescriptorBufferInfo& bufferInfo, uint32_t dstArrayElement)
{
	WriteDescriptorSet wds;
	wds.descriptorName = descriptorName;
	wds.dstSet = descriptorSetIndex;
	wds.dstBinding = bindingIndex;
	wds.dstArrayElement = 0;
	wds.descriptorCount = m_Sets[descriptorSetIndex].FindDescriptorSetLayout(bindingIndex).count;
	wds.descriptorType = descriptorType;
	wds.imageInfo = { nullptr, nullptr };
	wds.bufferInfo = bufferInfo;

	m_WriteDescriptorSets.push_back(wds);
}
void DescriptorSet::AddImage(uint32_t descriptorSetIndex, DescriptorSetLayout::DescriptorType descriptorType, uint32_t bindingIndex, const char* descriptorName, const DescriptorImageInfo& imageInfo, uint32_t dstArrayElement)
{
	WriteDescriptorSet wds;
	wds.descriptorName = descriptorName;
	wds.dstSet = descriptorSetIndex;
	wds.dstBinding = bindingIndex;
	wds.dstArrayElement = 0;
	wds.descriptorCount = m_Sets[descriptorSetIndex].FindDescriptorSetLayout(bindingIndex).count;
	wds.descriptorType = descriptorType;
	wds.imageInfo = imageInfo;
	wds.bufferInfo = { nullptr, 0, 0 };

	m_WriteDescriptorSets.push_back(wds);
}