// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_IndexBuffer.h"
#include "PC_RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Buffer.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"

using namespace pc_client;
using namespace scr;

void PC_IndexBuffer::Create(size_t numIndices, size_t stride, const unsigned char* data)
{
    m_Data = data;
    m_Count = numIndices;

	auto *rp = static_cast<PC_RenderPlatform*> (renderPlatform);
	auto *srp = rp->GetSimulRenderPlatform();

	m_SimulBuffer=srp->CreateBuffer();

	m_SimulBuffer->EnsureIndexBuffer(srp,(int) m_Count, (int)stride, data);
}
void PC_IndexBuffer::Destroy()
{
	delete m_SimulBuffer;
}

void PC_IndexBuffer::Bind() const
{
}
void PC_IndexBuffer::Unbind() const
{
}
