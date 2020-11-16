// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/UniformBuffer.h>

namespace pc_client
{
class PC_UniformBuffer final : public scr::UniformBuffer
{
private:

public:
	PC_UniformBuffer(const scr::RenderPlatform* const r) :scr::UniformBuffer(r) {}

	//Binding Locations for UBOs
	//Camera = 0;
	//Model = 1;
	//Light = 2;
	// Inherited via UniformBuffer
	void Create(UniformBufferCreateInfo* pUniformBuffer) override;
	void Destroy() override;

	void Submit() const override;
	void Update() const override;

	bool ResourceInUse(int timeout) override { return true; }

	void Bind() const override;
	void Unbind() const override;
};
}