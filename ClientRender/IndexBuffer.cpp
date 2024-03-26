// (C) Copyright 2018-2022 Simul Software Ltd
#include "IndexBuffer.h"
#include "Platform/CrossPlatform/Buffer.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
using namespace teleport;
using namespace clientrender;

void IndexBuffer::Destroy()
{
	delete m_SimulBuffer;
}

void IndexBuffer::Create(IndexBufferCreateInfo * pIndexBufferCreateInfo)
{
	m_CI = *pIndexBufferCreateInfo;

	m_SimulBuffer = renderPlatform->CreateBuffer();

	m_SimulBuffer->EnsureIndexBuffer(renderPlatform, (int)m_CI.indexCount, (int)m_CI.stride, m_CI.data);
}
