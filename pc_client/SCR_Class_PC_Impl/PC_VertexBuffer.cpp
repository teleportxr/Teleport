// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_VertexBuffer.h"

using namespace pc_client;
using namespace scr;

void PC_VertexBuffer::Create(size_t size, const void* data)
{
    m_Size = size;
    m_Data = data;
}

void PC_VertexBuffer::Destroy()
{
}

void PC_VertexBuffer::Bind() const
{
}

void PC_VertexBuffer::Unbind() const
{
}