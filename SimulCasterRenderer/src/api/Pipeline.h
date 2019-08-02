// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../Common.h"
#include "Shader.h"
#include "VertexBufferLayout.h"
#include "Texture.h"

namespace scr
{
	//Interface for Pipeline
	class Pipeline
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
			uint32_t ententX, ententY;
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
			bool depthClampEnable;			//Controls whether to clamp the fragment’s depth values as described in Depth Test.	    Vulkan Specific: Are these available in other Graphics APIs? 
			bool rasterizerDiscardEnable;	//Controls whether primitives are discarded immediately before the rasterization stage. Vulkan Specific: Are these available in other Graphics APIs? 
			PolygonMode polygonMode;
			CullMode cullMode;
			FrontFace frontFace;
		};
		struct MultisamplingState
		{
			bool samplerShadingEnable;
			Texture::SampleCount rasterizationSamples;
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
			MAXs,
		};
		struct ColourBlendingState
		{
			bool blendEnable;
			BlendFactor srcColorBlendFactor;
			BlendFactor dstColorBlendFactor;
			BlendOp colorBlendOp;
			BlendOp srcAlphaBlendFactor;
			BlendOp dstAlphaBlendFactor;
			BlendOp alphaBlendOp;
		};

	protected:
		const std::vector<Shader&>& m_Shaders;
		const VertexBufferLayout& m_VertexLayout;
		const TopologyType& m_Topology;
		const ViewportAndScissor& m_ViewportAndScissor;
		const RasterizationState& m_RasterizationState;
		const MultisamplingState& m_MultisamplingState;
		const DepthStencilingState& m_DepthStencilingState;
		const ColourBlendingState& m_ColourBlendingState;

	public:
		virtual ~Pipeline()	{};

		virtual void Create(const std::vector<Shader&>& shaders,
			const VertexBufferLayout& layout,
			const TopologyType& topology,
			const ViewportAndScissor& viewportAndScissor,
			const RasterizationState& rasterization,
			const MultisamplingState& multisample,
			const DepthStencilingState& depthStenciling,
			const ColourBlendingState& colourBlending) = 0;
			/*:m_Shaders(shaders), m_VertexLayout(layout), m_Topology(topology),
			m_ViewportAndScissor(viewportAndScissor), m_RasterizationState(rasterization),
			m_MultisamplingState(multisample), m_DepthStencilingState(depthStenciling),
			m_ColourBlendingState(colourBlending)*/ 

		virtual void LinkShaders(const std::vector<Shader&>& shaders) = 0;
		
		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		virtual void Draw(TopologyType topology, size_t indexBufferCount) = 0;
	};
}