// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_IndexBuffer.h"

using namespace scr;

void PC_IndexBuffer::Create(size_t size, const uint32_t* data)
{
    m_Size = size;
    m_Data = data;
    
    assert(size % 4 == 0);
    m_Count = size / sizeof(uint32_t);
}
void PC_IndexBuffer::Destroy()
{
}

void PC_IndexBuffer::Bind() const
{
}
void PC_IndexBuffer::Unbind() const
{
}
