// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_Texture.h"
#include "PC_RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Texture.h"

using namespace pc_client;
using namespace scr;

simul::crossplatform::PixelFormat ToSimulPixelFormat(scr::Texture::Format f)
{
	using namespace simul::crossplatform;
	using namespace scr;
	switch (f)
	{
	case scr::Texture::Format::RGBA32F:						return RGBA_32_FLOAT;
	case scr::Texture::Format::RGBA32UI:						return RGBA_32_UINT;
	case scr::Texture::Format::RGBA32I:						return RGBA_32_INT;
	case scr::Texture::Format::RGBA16F:						return RGBA_16_FLOAT;
	case scr::Texture::Format::RGBA16UI:						return RGBA_16_UINT;
	case scr::Texture::Format::RGBA16I:						return RGBA_16_INT;
	case scr::Texture::Format::RGBA16_SNORM:					return RGBA_16_SNORM;
	case scr::Texture::Format::RGBA16:						return RGBA_16_UNORM;
	case scr::Texture::Format::RGBA8UI:						return RGBA_8_UINT;
	case scr::Texture::Format::RGBA8I:						return RGBA_8_INT;
	case scr::Texture::Format::RGBA8_SNORM:					return RGBA_8_SNORM;
	case scr::Texture::Format::RGBA8:							return RGBA_8_UNORM;
	case scr::Texture::Format::RGB10_A2UI:					return RGB_10_A2_UINT;
	case scr::Texture::Format::RGB10_A2:						return RGB_10_A2_INT;
	case scr::Texture::Format::RGB32F:						return RGB_32_FLOAT;
	case scr::Texture::Format::R11F_G11F_B10F:				return RGB_11_11_10_FLOAT;
	case scr::Texture::Format::RG32F:							return RG_32_FLOAT;
	case scr::Texture::Format::RG32UI:						return RG_32_UINT;
	case scr::Texture::Format::RG32I:							 
	case scr::Texture::Format::RG16F:							return RG_16_FLOAT;
	case scr::Texture::Format::RG16UI:						return RG_16_UINT;
	case scr::Texture::Format::RG16I:
	case scr::Texture::Format::RG16_SNORM:					
	case scr::Texture::Format::RG16:							
	case scr::Texture::Format::RG8UI:							
	case scr::Texture::Format::RG8I:
	case scr::Texture::Format::RG8:							return RG_8_UNORM; 
	case scr::Texture::Format::R32F:							return R_32_FLOAT;
	case scr::Texture::Format::R32UI:							return R_32_UINT;
	case scr::Texture::Format::R32I:							return R_32_INT;
	case scr::Texture::Format::R16F:							return R_16_FLOAT;
	case scr::Texture::Format::R16UI:								
	case scr::Texture::Format::R16I:								
	case scr::Texture::Format::R16_SNORM :

	case scr::Texture::Format::R8UI:								
	case scr::Texture::Format::R8I:								
	case scr::Texture::Format::R8_SNORM:							
	case scr::Texture::Format::R8:
	case scr::Texture::Format::DEPTH_COMPONENT32F:			return D_32_FLOAT;
	case scr::Texture::Format::DEPTH_COMPONENT32:				return D_32_UINT;
	case scr::Texture::Format::DEPTH_COMPONENT24:					 
	case scr::Texture::Format::DEPTH_COMPONENT16:					
	case scr::Texture::Format::DEPTH_STENCIL:						
	case scr::Texture::Format::DEPTH32F_STENCIL8:				return D_32_FLOAT_S_8_UINT;
	case scr::Texture::Format::DEPTH24_STENCIL8:				return D_24_UNORM_S_8_UINT;
//	case scr::Texture::Format::UNSIGNED_INT_24_8:				return D_24_UINT_S_8_UINT;
	case scr::Texture::Format::FLOAT_32_UNSIGNED_INT_24_8_REV:		
	default:
		return UNKNOWN;
	};
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

void PC_Texture::Create(TextureCreateInfo * pTextureCreateInfo)
{
	m_CI = *pTextureCreateInfo;
	//m_CI.size = pTextureCreateInfo->width * pTextureCreateInfo->height * pTextureCreateInfo->depth *pTextureCreateInfo->bitsPerPixel;
	//m_Data = data;
	auto* rp = static_cast<PC_RenderPlatform*> (renderPlatform);
	auto* srp = rp->GetSimulRenderPlatform();
	m_SimulTexture = srp->CreateTexture();
	auto pixelFormat = ToSimulPixelFormat(pTextureCreateInfo->format);
	bool computable = false;
	bool rt = false;
	bool ds = false;
	int num_samp = 1;
	m_SimulTexture->ensureTexture2DSizeAndFormat(srp, pTextureCreateInfo->width, pTextureCreateInfo->height, pixelFormat, computable, rt, ds, num_samp);
	m_SimulTexture->setTexels(srp->GetImmediateContext(), pTextureCreateInfo->data, 0, pTextureCreateInfo->size/pTextureCreateInfo->bytesPerPixel);
}

void PC_Texture::GenerateMips()
{
}
