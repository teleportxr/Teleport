// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_FrameBuffer.h"

using namespace pc_client;
using namespace scr;

void PC_FrameBuffer::Create(Texture::Format format, Texture::SampleCount sampleCount, uint32_t width, uint32_t height)
{
   m_Width = width;
   m_Height = height;
   m_SampleCount = sampleCount;
   m_Format = format;
}

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
void PC_FrameBuffer::Clear(float colour_r, float colour_g, float colour_b, float colour_a, float depth, uint32_t stencil)
{
}
