// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_Texture.h"

using namespace pc_client;
using namespace scr;

void PC_Texture::Create(Slot slot, Type type, Format format, SampleCountBit sampleCount, uint32_t width, uint32_t height, uint32_t depth, uint32_t bitsPerPixel, const uint8_t* data)
{
    m_Width = width;
    m_Height = height;
    m_Depth = depth;
    m_BitsPerPixel = bitsPerPixel;

    m_Slot = slot;
    m_Type = type;
    m_Format = format;
    m_SampleCount = sampleCount;

    m_Size = m_Width * m_Height * m_Depth * m_BitsPerPixel;
    m_Data = data;
}
void PC_Texture::Destroy()
{
}

void PC_Texture::Bind() const
{
}
void PC_Texture::Unbind() const
{
}

void PC_Texture::UseSampler(const Sampler* sampler)
{
}
void PC_Texture::GenerateMips()
{
}