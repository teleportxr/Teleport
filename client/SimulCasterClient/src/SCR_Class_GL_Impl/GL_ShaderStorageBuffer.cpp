#include "GL_ShaderStorageBuffer.h"

using namespace scc;
using namespace scr;
using namespace OVR;

void GL_ShaderStorageBuffer::Create(ShaderStorageBufferCreateInfo* pShaderStorageBufferCreateInfo)
{
    m_CI = *pShaderStorageBufferCreateInfo;

    m_SSBO = GlBuffer();
    m_SSBO.Create(GLBUFFER_TYPE_STORAGE, m_CI.size, m_CI.data);
}

void GL_ShaderStorageBuffer::Update(size_t size, const void* data, uint32_t offset)
{
    m_SSBO.Update(size, data, offset);
}

void GL_ShaderStorageBuffer::Destroy()
{
    m_SSBO.Destroy();
}

void GL_ShaderStorageBuffer::Bind() const
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO.GetBuffer());
}
void GL_ShaderStorageBuffer::Unbind() const
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void GL_ShaderStorageBuffer::Access()
{
    Bind();
    /*void* gpuData = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, m_CI.size, (GLbitfield)m_CI.access);

    if (((uint32_t)m_CI.access & (uint32_t)ShaderStorageBuffer::Access::READ_BIT) != 0)
        memcpy(m_CI.data, gpuData, m_CI.size);
    if (((uint32_t)m_CI.access & (uint32_t)ShaderStorageBuffer::Access::WRITE_BIT) != 0)
        memcpy(gpuData, m_CI.data, m_CI.size);

    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);*/
    Unbind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_CI.bindingLocation, m_SSBO.GetBuffer());
}

