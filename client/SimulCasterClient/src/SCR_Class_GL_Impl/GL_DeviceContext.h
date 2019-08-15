// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/DeviceContext.h>
#include <crossplatform/DescriptorSet.h>

namespace scc
{
class GL_DeviceContext final : public scr::DeviceContext
    {
	public:
    GL_DeviceContext(scr::RenderPlatform *r)
        :scr::DeviceContext(r) {}

    void Create(DeviceContextCreateInfo* pDeviceContextCreateInfo) override;

    void Draw(scr::InputCommand* pInputCommand) override;
    void DispatchCompute(scr::InputCommand* pInputCommand) override;

    void BeginFrame() override;
    void EndFrame() override;

    private:
        void BindDescriptorSets(const std::vector<scr::DescriptorSet>& descriptorSets);
	};
}