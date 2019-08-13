// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/Pipeline.h>

namespace scr
{
	class PC_Pipeline final : public Pipeline
	{
	private:

	public:
		PC_Pipeline() {}

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
	};
}