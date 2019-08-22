// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_Texture.h"
#include "PC_RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"

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

void PC_Texture::Create(TextureCreateInfo * pTextureCreateInfo)
{
	m_CI = *pTextureCreateInfo;
	//m_CI.size = pTextureCreateInfo->width * pTextureCreateInfo->height * pTextureCreateInfo->depth *pTextureCreateInfo->bitsPerPixel;
	//m_Data = data;
	/*auto* rp = static_cast<PC_RenderPlatform*> (renderPlatform);
	auto* srp = rp->GetSimulRenderPlatform();
	m_SimulTexture = srp->CreateTexture();

	m_SimulTexture->ensureTexture2DSizeAndFormat(srp, pTextureCreateInfo->width, pTextureCreateInfo->height, pixelFormat, computable, rt, ds, num_samp);*/
}

void PC_Texture::GenerateMips()
{
}
