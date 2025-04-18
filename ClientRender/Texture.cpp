#include "Texture.h"
#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#include "Platform/CrossPlatform/PixelFormat.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include <magic_enum/magic_enum.hpp>
using namespace teleport;
using namespace clientrender;

const vec3 Texture::DUMMY_DIMENSIONS = {1, 1, 1};


platform::crossplatform::PixelFormat ToSimulPixelFormat(clientrender::Texture::Format f)
{
	using namespace platform::crossplatform;
	switch (f)
	{
	case clientrender::Texture::Format::RGBA32F:					return RGBA_32_FLOAT;
	case clientrender::Texture::Format::RGBA32UI:					return RGBA_32_UINT;
	case clientrender::Texture::Format::RGBA32I:					return RGBA_32_INT;
	case clientrender::Texture::Format::RGBA16F:					return RGBA_16_FLOAT;
	case clientrender::Texture::Format::RGBA16UI:					return RGBA_16_UINT;
	case clientrender::Texture::Format::RGBA16I:					return RGBA_16_INT;
	case clientrender::Texture::Format::RGBA16_SNORM:				return RGBA_16_SNORM;
	case clientrender::Texture::Format::RGBA16:						return RGBA_16_UNORM;
	case clientrender::Texture::Format::RGBA8UI:					return RGBA_8_UINT;
	case clientrender::Texture::Format::RGBA8I:						return RGBA_8_INT;
	case clientrender::Texture::Format::RGBA8_SNORM:				return RGBA_8_SNORM;
	case clientrender::Texture::Format::RGBA8:						return RGBA_8_UNORM;
	case clientrender::Texture::Format::BGRA8:						return RGBA_8_UNORM;	// Because GL doesn't support  BGRA!!!
	case clientrender::Texture::Format::RGB10_A2UI:					return RGB_10_A2_UINT;
	case clientrender::Texture::Format::RGB10_A2:					return RGB_10_A2_INT;
	case clientrender::Texture::Format::RGB32F:						return RGB_32_FLOAT;
	case clientrender::Texture::Format::R11F_G11F_B10F:				return RGB_11_11_10_FLOAT;
	case clientrender::Texture::Format::RGB8:						return RGB_8_UNORM;
	case clientrender::Texture::Format::RG32F:						return RG_32_FLOAT;
	case clientrender::Texture::Format::RG32UI:						return RG_32_UINT;
	case clientrender::Texture::Format::RG32I:							 
	case clientrender::Texture::Format::RG16F:						return RG_16_FLOAT;
	case clientrender::Texture::Format::RG16UI:						return RG_16_UINT;
	case clientrender::Texture::Format::RG16I:
	case clientrender::Texture::Format::RG16_SNORM:					
	case clientrender::Texture::Format::RG16:							
	case clientrender::Texture::Format::RG8UI:							
	case clientrender::Texture::Format::RG8I:
	case clientrender::Texture::Format::RG8:						return RG_8_UNORM; 
	case clientrender::Texture::Format::R32F:						return R_32_FLOAT;
	case clientrender::Texture::Format::R32UI:						return R_32_UINT;
	case clientrender::Texture::Format::R32I:						return R_32_INT;
	case clientrender::Texture::Format::R16F:						return R_16_FLOAT;
	case clientrender::Texture::Format::R16UI:								
	case clientrender::Texture::Format::R16I:								
	case clientrender::Texture::Format::R16_SNORM :

	case clientrender::Texture::Format::R8UI:								
	case clientrender::Texture::Format::R8I:								
	case clientrender::Texture::Format::R8_SNORM:						return R_8_SNORM;
	case clientrender::Texture::Format::R8:								return R_8_UNORM;
	case clientrender::Texture::Format::DEPTH_COMPONENT32F:				return D_32_FLOAT;
	case clientrender::Texture::Format::DEPTH_COMPONENT32:				return D_32_UINT;
	case clientrender::Texture::Format::DEPTH_COMPONENT24:					 
	case clientrender::Texture::Format::DEPTH_COMPONENT16:					
	case clientrender::Texture::Format::DEPTH_STENCIL:						
	case clientrender::Texture::Format::DEPTH32F_STENCIL8:				return D_32_FLOAT_S_8_UINT;
	case clientrender::Texture::Format::DEPTH24_STENCIL8:				return D_24_UNORM_S_8_UINT;
//	case clientrender::Texture::Format::UNSIGNED_INT_24_8:				return D_24_UINT_S_8_UINT;
	case clientrender::Texture::Format::FLOAT_32_UNSIGNED_INT_24_8_REV:		
	default:
		return UNKNOWN;
	};
}

Texture::~Texture()
{
	Destroy();
}

void Texture::Destroy()
{
	delete m_SimulTexture;
	m_SimulTexture = nullptr;
}
#pragma optimize("",off)
void Texture::Create(const TextureCreateInfo& pTextureCreateInfo)
{
	if(pTextureCreateInfo.format==Format::FORMAT_UNKNOWN)
		return;
	if(pTextureCreateInfo.width==0||pTextureCreateInfo.height==0)
		return;
	m_CI = pTextureCreateInfo;
	//m_CI.size = pTextureCreateInfo->width * pTextureCreateInfo->height * pTextureCreateInfo->depth *pTextureCreateInfo->bitsPerPixel;
	//m_Data = data;
	m_SimulTexture = renderPlatform->CreateTexture();
	auto pixelFormat = ToSimulPixelFormat(pTextureCreateInfo.format);
	bool computable = false;
	bool rt = false;
	bool ds = false;
	int num_samp = 1;
	TELEPORT_LOG("Creating texture {0}",pTextureCreateInfo.name);
	platform::crossplatform::TextureCreate textureCreate;
	textureCreate.w					= pTextureCreateInfo.width;
	textureCreate.l					= pTextureCreateInfo.height;
	textureCreate.f					= pixelFormat;
	textureCreate.computable		= computable;
	textureCreate.cubemap			=((pTextureCreateInfo.type&Type::TEXTURE_CUBE_MAP)==Type::TEXTURE_CUBE_MAP);
	textureCreate.make_rt			= rt;
	textureCreate.setDepthStencil	= ds;
	textureCreate.numOfSamples		= num_samp;
	textureCreate.compressionFormat = (platform::crossplatform::CompressionFormat)pTextureCreateInfo.compression;
	textureCreate.mips				=pTextureCreateInfo.mipCount;
	while((1<<(textureCreate.mips-1))>textureCreate.w)
	{
		textureCreate.mips--;
	}
	while((1<<(textureCreate.mips-1))>textureCreate.l)
	{
		textureCreate.mips--;
	}
	textureCreate.initialData		= pTextureCreateInfo.images;
	textureCreate.name				= m_CI.name.c_str();
	if(!m_SimulTexture->EnsureTexture(renderPlatform, &textureCreate))
	{
		TELEPORT_WARN("\tFailed to create texture: {0}",pTextureCreateInfo.name);
	}
}

void Texture::GenerateMips()
{
}
