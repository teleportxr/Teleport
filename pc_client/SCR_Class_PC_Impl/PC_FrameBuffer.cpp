// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_FrameBuffer.h"

using namespace pc_client;
using namespace clientrender;

void PC_FrameBuffer::Destroy()
{
}

void PC_FrameBuffer::Bind() const
{
}
void PC_FrameBuffer::Unbind() const
{
}

void PC_FrameBuffer::Resolve()
{
}
void PC_FrameBuffer::UpdateFrameBufferSize(uint32_t width, uint32_t height)
{
}

void pc_client::PC_FrameBuffer::Create(FrameBufferCreateInfo * pFrameBufferCreateInfo)
{
	m_CI = *pFrameBufferCreateInfo;
}

void pc_client::PC_FrameBuffer::SetClear(ClearColous * pClearColours)
{
}
