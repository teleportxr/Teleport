// (C) Copyright 2018-2022 Simul Software Ltd
#include "IndexBuffer.h"
#include "RenderPlatform.h"
#include "Platform/CrossPlatform/Buffer.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
using namespace clientrender;

void IndexBuffer::Destroy()
{
	delete m_SimulBuffer;
}

void IndexBuffer::Create(IndexBufferCreateInfo * pIndexBufferCreateInfo)
{
	m_CI = *pIndexBufferCreateInfo;

	auto *srp = renderPlatform->GetSimulRenderPlatform();

	m_SimulBuffer = srp->CreateBuffer();

	m_SimulBuffer->EnsureIndexBuffer(srp, (int)m_CI.indexCount, (int)m_CI.stride, m_CI.data);
}
