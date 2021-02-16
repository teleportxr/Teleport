// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "api/Shader.h"
#include "api/Texture.h"
#include "api/UniformBuffer.h"
#include "api/ShaderStorageBuffer.h"
#include "Common.h"

namespace scr
{
	class ShaderResourceLayout
	{
	public:
		enum class ShaderResourceType : uint32_t
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
			INPUT_ATTACHMENT
		};
		struct ShaderResourceLayoutBinding
		{
			uint32_t binding;
			ShaderResourceType type;
			uint32_t count;		//Number of item in a potential array. Default = 1.
			Shader::Stage stage;
		};

		std::vector<ShaderResourceLayoutBinding> m_LayoutBindings;

		ShaderResourceLayout() {};

		inline void AddBinding(uint32_t binding, ShaderResourceType type, Shader::Stage stage, uint32_t count = 1)
		{
			m_LayoutBindings.push_back({ binding, type, count, stage });
		}
		inline void AddBinding(const ShaderResourceLayoutBinding& layout)
		{
			m_LayoutBindings.push_back(layout);
		}
		inline ShaderResourceLayoutBinding& FindShaderResourceLayout(uint32_t bindingIndex)
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
				SCR_CERR_BREAK("Could not find DescriptorSetLayoutBinding at binding index: " << bindingIndex << ".", -1);
				throw;
			}
			return m_LayoutBindings[index];
		}
	};

	class ShaderResource
	{
	public:
		struct ShaderResourceImageInfo
		{
			std::shared_ptr<Sampler> sampler;
			std::shared_ptr<Texture> texture;
			uint32_t mip=(uint32_t)-1;
			uint32_t layer=(uint32_t)-1;
		};
		struct ShaderResourceBufferInfo
		{
			void* buffer; //Used for both UB and SSB.
			size_t offset;
			size_t range;
		};
		struct WriteShaderResource
		{
			const char* shaderResourceName;
			uint32_t dstBinding;
			uint32_t dstArrayElement;
			uint32_t shaderResourceCount;
			ShaderResourceLayout::ShaderResourceType shaderResourceType;
			ShaderResourceImageInfo imageInfo;
			ShaderResourceBufferInfo bufferInfo;
		};
	private:

		ShaderResourceLayout m_ShaderResourceLayout;
		std::vector<WriteShaderResource> m_WriteShaderResources;

	public:
		ShaderResource() {};
		ShaderResource(const ShaderResourceLayout& shaderResourceLayout);
		~ShaderResource();

		void SetLayout(const ShaderResourceLayout& shaderResourceLayout);
		void AddBuffer( ShaderResourceLayout::ShaderResourceType shaderResourceType, uint32_t bindingIndex, const char* shaderResourceName, const ShaderResourceBufferInfo& bufferInfo, uint32_t dstArrayElement = 0);
		void AddImage( ShaderResourceLayout::ShaderResourceType shaderResourceType, uint32_t bindingIndex, const char* shaderResourceName, const ShaderResourceImageInfo& imageInfo, uint32_t dstArrayElement = 0);
		void SetImageInfo( size_t index, const ShaderResourceImageInfo& imageInfo);

		const std::vector<WriteShaderResource>& GetWriteShaderResources() const {return m_WriteShaderResources;}
		std::vector<WriteShaderResource>& GetWriteShaderResources() {return m_WriteShaderResources;}
	};
}