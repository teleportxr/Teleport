// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_FrameBuffer.h"

using namespace scr;
using namespace OVR;

void GL_FrameBuffer::Create(Texture::Format format, Texture::SampleCount sampleCount, uint32_t width, uint32_t height)
{
   m_Width = width;
   m_Height = height;
   m_SampleCount = sampleCount;
   m_Format = format;

   colorFormat_t colourFormat = COLOR_8888;
   depthFormat_t depthFormat = DEPTH_24;

   m_EyeBuffersParms.resolutionWidth = width;
   m_EyeBuffersParms.resolutionHeight = height;
   m_EyeBuffersParms.multisamples = (int)sampleCount;
   m_EyeBuffersParms.colorFormat = colourFormat;
   m_EyeBuffersParms.depthFormat = depthFormat;

   m_EyeBuffers = std::make_unique<ovrEyeBuffers>();
   m_EyeBuffers->Initialize(m_EyeBuffersParms, true);
}
void GL_FrameBuffer::Destroy()
{
   m_EyeBuffers->~ovrEyeBuffers();
}

void GL_FrameBuffer::Bind() const
{
   m_EyeBuffers->BeginFrame();
   m_EyeBuffers->BeginRenderingEye(eyeNum);
}
void GL_FrameBuffer::Unbind() const
{
}

void GL_FrameBuffer::Resolve()
{
   m_EyeBuffers->EndRenderingEye(eyeNum);
   m_EyeBuffers->EndFrame();

   eyeNum = (eyeNum + 1) % 2;

}
void GL_FrameBuffer::UpdateFrameBufferSize(uint32_t width, uint32_t height)
{}
void GL_FrameBuffer::Clear(float colour_r, float colour_g, float colour_b, float colour_a, float depth, uint32_t stencil)
{
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
   glClearColor(colour_r, colour_g, colour_b, colour_a);
   glClearDepthf(depth);
   glClearStencil(stencil);
}
