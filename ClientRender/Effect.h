// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "Common.h"
#include "VertexBufferLayout.h"
#include "Texture.h"
#include "Shader.h"
#include "ClientRender/ShaderResource.h"
#include "ClientRender/ShaderSystem.h"

namespace clientrender
{
	//Interface for Effect
	class Effect : APIObject
	{
	public:
		enum class TopologyType : uint32_t
		{
			POINT_LIST,
			LINE_LIST,
			LINE_STRIP,
			TRIANGLE_LIST,
			TRIANGLE_STRIP,
			TRIANGLE_FAN
		};
		struct ViewportAndScissor
		{
			float x, y, width, height;
			float minDepth, maxDepth;

			uint32_t offsetX, offsetY;
			uint32_t extentX, extentY;
		};
		enum class PolygonMode :uint32_t
		{
			FILL,
			LINE,
			POINT,
		};
		enum class CullMode : uint32_t
		{
			NONE,
			FRONT_BIT,
			BACK_BIT,
			FRONT_AND_BACK,
		};
		enum class FrontFace : uint32_t
		{
			COUNTER_CLOCKWISE,
			CLOCKWISE
		};
		struct RasterizationState
		{
			bool depthClampEnable;			//Controls whether to clamp the fragment's depth values as described in Depth Test.	    Vulkan Specific: Are these available in other Graphics APIs? 
			bool rasterizerDiscardEnable;	//Controls whether primitives are discarded immediately before the rasterization stage. Vulkan Specific: Are these available in other Graphics APIs? 
			PolygonMode polygonMode;
			CullMode cullMode;
			FrontFace frontFace;
		};
		struct MultisamplingState
		{
			bool samplerShadingEnable;
			Texture::SampleCountBit rasterizationSamples;
		};
		enum class CompareOp : uint32_t
		{
			NEVER,
			LESS,
			EQUAL,
			LESS_OR_EQUAL,
			GREATER,
			NOT_EQUAL,
			GREATER_OR_EQUAL,
			ALWAYS
		};
		enum class StencilCompareOp : uint32_t
		{
			KEEP,
			ZERO,
			REPLACE,
			INCREMENT_AND_CLAMP,
			DECREMENT_AND_CLAMP,
			INVERT,
			INCREMENT_AND_WRAP,
			DECREMENT_AND_WRAP
		};
		struct StencilCompareOpState
		{
			StencilCompareOp stencilFailOp;
			StencilCompareOp stencilPassDepthFailOp;
			StencilCompareOp passOp;
			CompareOp compareOp;
		};
		struct DepthStencilingState
		{
			bool depthTestEnable;
			bool depthWriteEnable;
			CompareOp depthCompareOp;

			bool stencilTestEnable;
			StencilCompareOpState frontCompareOp;
			StencilCompareOpState backCompareOp;

			bool depthBoundTestEnable;
			float minDepthBounds;
			float maxDepthBounds;

		};
		enum class BlendFactor : uint32_t
		{
			ZERO,
			ONE,
			SRC_COLOR,
			ONE_MINUS_SRC_COLOR,
			DST_COLOR,
			ONE_MINUS_DST_COLOR,
			SRC_ALPHA,
			ONE_MINUS_SRC_ALPHA,
			DST_ALPHA,
			ONE_MINUS_DST_ALPHA
		};
		enum class BlendOp : uint32_t
		{
			ADD,
			SUBTRACT,
			REVERSE_SUBTRACT,
			MIN,
			MAX,
		};
		struct ColourBlendingState
		{
			bool blendEnable;
			BlendFactor srcColorBlendFactor;
			BlendFactor dstColorBlendFactor;
			BlendOp colorBlendOp;
			BlendFactor srcAlphaBlendFactor;
			BlendFactor dstAlphaBlendFactor;
			BlendOp alphaBlendOp;
		};

		struct EffectCreateInfo
		{
			const char* effectName; //Opaque, Transparent, Emissive
		};
		struct EffectPassCreateInfo
		{
			std::string effectPassName;
			ShaderSystem::PassVariables passVariables;
			ShaderSystem::Pipeline pipeline;
			VertexBufferLayout vertexLayout;
			TopologyType topology;
			ViewportAndScissor viewportAndScissor;
			RasterizationState rasterizationState;
			MultisamplingState multisamplingState;
			DepthStencilingState depthStencilingState;
			ColourBlendingState colourBlendingState;
		};

	protected:
		EffectCreateInfo m_CI;
		std::map<std::string, EffectPassCreateInfo> m_EffectPasses;

	public:
		Effect(const RenderPlatform* const r)
			: APIObject(r), m_CI()
		{}

		virtual ~Effect() = default;

		virtual void Create(EffectCreateInfo* pEffectCreateInfo) = 0;
		virtual void CreatePass(EffectPassCreateInfo* pEffectPassCreateInfo) = 0;

		inline const EffectCreateInfo& GetEffectCreateInfo() const { return m_CI; }
		const EffectPassCreateInfo* GetEffectPassCreateInfo(const char* effectPassName) const
		{
			for(const auto& passPair : m_EffectPasses)
			{
				if(passPair.first== effectPassName)
				{
					return &passPair.second;
				}
			}

			return nullptr;
		}

		bool HasEffectPass(const char* effectPassName)
		{
			//Null pointer returns false, otherwise true.
			return GetEffectPassCreateInfo(effectPassName);
		}

		virtual void LinkShaders(const char* effectPassName, const std::vector<ShaderResource>& shaderResources) = 0;

	protected:
		virtual void Bind(const char* effectPassName) const = 0;
		virtual void Unbind(const char* effectPassName) const = 0;
	};
}