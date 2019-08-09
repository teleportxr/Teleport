// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "api/Shader.h"
#include "api/Texture.h"
#include "api/UniformBuffer.h"
#include "Common.h"

namespace scr
{
	class DescriptorSetLayout
	{
	public:
		enum class DescriptorType : uint32_t
		{
			SAMPLER,
			COMBINED_IMAGE_SAMPLER,
			SAMPLED_IMAGE,
			STORAGE_IMAGE,
			UNIFORM_TEXEL_BUFFER,
			STORAGE_TEXEL_BUFFER,
			UNIFORM_BUFFER,
			STORAGE_BUFFER,
			UNIFORM_BUFFER_DYNAMIC,
			STORAGE_BUFFER_DYNAMIC,
			INPUT_ATTACHMENT = 10
		};
		struct DescriptorSetLayoutBinding
		{
			uint32_t binding;
			DescriptorType type;
			uint32_t count;		//Number of item in a potential array. Default = 1.
			Shader::Stage stage;
		};

		std::vector<DescriptorSetLayoutBinding> m_SetLayoutBindings;

		DescriptorSetLayout() {};

		inline void AddBinding(uint32_t binding, DescriptorType type, Shader::Stage stage, uint32_t count = 1)
		{
			m_SetLayoutBindings.push_back({ binding, type, count, stage });
		}
		inline void AddBinding(const DescriptorSetLayoutBinding& layout)
		{
			m_SetLayoutBindings.push_back(layout);
		}
		inline DescriptorSetLayoutBinding& FindDescriptorSetLayout(uint32_t bindingIndex)
		{
			size_t index = (size_t)-1;
			for (size_t i = 0; i < m_SetLayoutBindings.size(); i++)
			{
				if (m_SetLayoutBindings[i].binding == bindingIndex)
				{
					index = i;
					break;
				}
			}
			if (index == (size_t)-1)
			{
				SCR_COUT_BREAK("Could not find DescriptorSetLayoutBinding at binding index: " << bindingIndex << ".");
				throw;
			}
			return m_SetLayoutBindings[index];
		}
	};

	class DescriptorSet
	{
	private:
		struct DescriptorImageInfo 
		{
			const Sampler*	sampler;
			const Texture*	texture;
		};
		struct DescriptorBufferInfo 
		{
			const UniformBuffer*	buffer;
			size_t					offset;
			size_t					range;
		};
		struct WriteDescriptorSet
		{
			const char* descriptorName;
			uint32_t dstSet;
			uint32_t dstBinding;
			uint32_t dstArrayElement;
			uint32_t descriptorCount;
			DescriptorSetLayout::DescriptorType descriptorType;
			const DescriptorImageInfo* pImageInfo;
			const DescriptorBufferInfo* pBufferInfo;

		};

		std::map<uint32_t, DescriptorSetLayout> m_Sets;
		std::vector<WriteDescriptorSet> m_WriteDescriptorSets;

	public:
		DescriptorSet() {};
		DescriptorSet(const std::vector<DescriptorSetLayout>& descriptorSetLayouts);
		~DescriptorSet();

		void AddBuffer(uint32_t descriptorSetIndex, DescriptorSetLayout::DescriptorType descriptorType, uint32_t bindingIndex, const char* descriptorName, const DescriptorBufferInfo& bufferInfo, uint32_t dstArrayElement = 0);
		void AddImage(uint32_t descriptorSetIndex, DescriptorSetLayout::DescriptorType descriptorType, uint32_t bindingIndex, const char* descriptorName, const DescriptorImageInfo& imageInfo, uint32_t dstArrayElement = 0);

		const std::vector<WriteDescriptorSet>& GetWriteDescriptorSet() const {return m_WriteDescriptorSets;}
	};
}