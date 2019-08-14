// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_Texture.h"

using namespace pc_client;
using namespace scr;


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
void pc_client::PC_Texture::Create(TextureCreateInfo * pTextureCreateInfo, size_t size, const uint8_t * data)
{
	m_CI = *pTextureCreateInfo;
	m_Size = pTextureCreateInfo->width * pTextureCreateInfo->height * pTextureCreateInfo->depth *pTextureCreateInfo->bitsPerPixel;
	m_Data = data;
}
void PC_Texture::GenerateMips()
{
}