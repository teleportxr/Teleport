// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_UniformBuffer.h"

using namespace clientrender;

namespace pc_client
{
void PC_UniformBuffer::Create(UniformBufferCreateInfo* pUniformBuffer)
{
	m_CI = *pUniformBuffer;
}

void PC_UniformBuffer::Destroy()
{}

void PC_UniformBuffer::Submit() const
{}

void PC_UniformBuffer::Update() const
{}

void PC_UniformBuffer::Bind() const
{}

void PC_UniformBuffer::Unbind() const
{}
}