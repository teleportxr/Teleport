// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_FrameBuffer.h"

using namespace scc;
using namespace scr;
using namespace OVR;

void GL_FrameBuffer::Create(FrameBufferCreateInfo* pFrameBufferCreateInfo)
{
   m_CI = *pFrameBufferCreateInfo;

   colorFormat_t colourFormat = COLOR_8888;
   depthFormat_t depthFormat = DEPTH_24;

   m_EyeBuffersParms.resolutionWidth = m_CI.width;
   m_EyeBuffersParms.resolutionHeight = m_CI.height;
   m_EyeBuffersParms.multisamples = (int)m_CI.sampleCount;
   m_EyeBuffersParms.colorFormat = colourFormat;
   m_EyeBuffersParms.depthFormat = depthFormat;

   m_EyeBuffers.Initialize(m_EyeBuffersParms, true);
}
void GL_FrameBuffer::Destroy()
{
   m_EyeBuffers.~ovrEyeBuffers();
}

void GL_FrameBuffer::Bind() const
{
   //NULL
}
void GL_FrameBuffer::Unbind() const
{
   //NULL
}
void GL_FrameBuffer::Resolve()
{
   //NULL
}

void GL_FrameBuffer::BeginFrame()
{
   m_EyeBuffers.BeginFrame();
   m_EyeBuffers.BeginRenderingEye(eyeNum);
}

void GL_FrameBuffer::EndFrame()
{
   m_EyeBuffers.EndRenderingEye(eyeNum);
   m_EyeBuffers.EndFrame();

   eyeNum = (eyeNum + 1) % 2;

}
void GL_FrameBuffer::UpdateFrameBufferSize(uint32_t width, uint32_t height)
{}
void GL_FrameBuffer::SetClear(ClearColous* pClearColours)
{
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
   glClearColor(pClearColours->colour_r, pClearColours->colour_g, pClearColours->colour_b, pClearColours->colour_a);
   glClearDepthf(pClearColours->depth);
   glClearStencil(pClearColours->stencil);
}
