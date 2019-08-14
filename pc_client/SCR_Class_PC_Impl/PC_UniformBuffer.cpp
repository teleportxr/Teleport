// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_UniformBuffer.h"

using namespace pc_client;
using namespace scr;

void PC_UniformBuffer::Create(size_t size, const void* data, uint32_t bindingLocation)
{
    m_BindingLocation = bindingLocation;
    m_Size = size;
    m_Data = data;

}
void PC_UniformBuffer::Destroy()
{
}

void PC_UniformBuffer::Bind() const
{
}
void PC_UniformBuffer::Unbind() const
{
}

void PC_UniformBuffer::Submit() const
{
}
