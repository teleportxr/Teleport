// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/Effect.h>

namespace pc_client
{
	class PC_Effect final : public scr::Effect
	{
	private:

	public:
		PC_Effect(scr::RenderPlatform *r):scr::Pipeline(r) {}

		void Create(const std::vector<scr::Shader*>& shaders,
			const scr::VertexBufferLayout& layout,
			const scr::Pipeline::TopologyType& topology,
			const scr::Pipeline::ViewportAndScissor& viewportAndScissor,
			const scr::Pipeline::RasterizationState& rasterization,
			const scr::Pipeline::MultisamplingState& multisample,
			const scr::Pipeline::DepthStencilingState& depthStenciling,
			const scr::Pipeline::ColourBlendingState& colourBlending) override;

		void LinkShaders() override;
		
		void Bind() const override;
		void Unbind() const override;

		void BindDescriptorSets(const std::vector<scr::DescriptorSet>& descriptorSets) override;

		void Draw(size_t indexBufferCount) override;

	private:
	};
}