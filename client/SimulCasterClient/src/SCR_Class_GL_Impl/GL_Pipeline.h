// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/Pipeline.h>
#include <GlProgram.h>

namespace scr
{
	class GL_Pipeline final : public Pipeline
	{
	private:
		OVR::GlProgram m_Program;

	public:
		GL_Pipeline() {}

		void Create(const std::vector<Shader*>& shaders,
			const VertexBufferLayout& layout,
			const TopologyType& topology,
			const ViewportAndScissor& viewportAndScissor,
			const RasterizationState& rasterization,
			const MultisamplingState& multisample,
			const DepthStencilingState& depthStenciling,
			const ColourBlendingState& colourBlending) override;

		void LinkShaders() override;
		
		void Bind() const override;
		void Unbind() const override;

		void BindDescriptorSets(const std::vector<DescriptorSet>& descriptorSets) override;

		void Draw(size_t indexBufferCount) override;

	private:
		GLenum ToGLTopology(TopologyType topology) const;
        GLenum ToGLCullMode(CullMode cullMode) const;
        GLenum ToGLCompareOp(CompareOp op) const;
        GLenum ToGLStencilCompareOp(StencilCompareOp op) const;
        GLenum ToGLBlendFactor(BlendFactor factor) const;
        GLenum ToGLBlendOp(BlendOp op) const;
	};
}