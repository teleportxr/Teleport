#include "Texture.h"
#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#include "Platform/CrossPlatform/PixelFormat.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include <magic_enum/magic_enum.hpp>
#include <algorithm>
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
	TELEPORT_INTERNAL_COUT(Resource, "Creating texture {0}",pTextureCreateInfo.name);
	platform::crossplatform::TextureCreate textureCreate;
	textureCreate.w					= pTextureCreateInfo.width;
	textureCreate.l					= pTextureCreateInfo.height;
	textureCreate.d					= pTextureCreateInfo.depth;
	textureCreate.arraysize			= pTextureCreateInfo.arrayCount;
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

// Compressed formats store one block (commonly 4x4 texels) per this many bits; approximate,
// since e.g. RGB-only ETC2 is actually 4 bits/pixel rather than the 8 used here for its combined
// (RGBA) variant - fine for a diagnostic estimate, not for exact VRAM accounting.
static size_t BitsPerPixelForCompression(Texture::CompressionFormat c)
{
	switch (c)
	{
	case Texture::CompressionFormat::BC1:
	case Texture::CompressionFormat::BC4:
	case Texture::CompressionFormat::ETC1:
	case Texture::CompressionFormat::PVRTC1_4_OPAQUE_ONLY:
		return 4;
	case Texture::CompressionFormat::BC3:
	case Texture::CompressionFormat::BC5:
	case Texture::CompressionFormat::ETC2:
	case Texture::CompressionFormat::BC7_M6_OPAQUE_ONLY:
	case Texture::CompressionFormat::BC6H:
		return 8;
	default:
		return 0;
	}
}

static size_t BytesPerPixelForFormat(Texture::Format f)
{
	using Format = Texture::Format;
	switch (f)
	{
	case Format::RGBA32F:
	case Format::RGBA32UI:
	case Format::RGBA32I:
		return 16;
	case Format::RGBA16F:
	case Format::RGBA16UI:
	case Format::RGBA16I:
	case Format::RGBA16_SNORM:
	case Format::RGBA16:
		return 8;
	case Format::RGB32F:
		return 12;
	case Format::RGB8:
		return 3;
	case Format::RG32F:
	case Format::RG32UI:
	case Format::RG32I:
		return 8;
	case Format::RG16F:
	case Format::RG16UI:
	case Format::RG16I:
	case Format::RG16_SNORM:
	case Format::RG16:
		return 4;
	case Format::RG8UI:
	case Format::RG8I:
	case Format::RG8_SNORM:
	case Format::RG8:
		return 2;
	case Format::R32F:
	case Format::R32UI:
	case Format::R32I:
		return 4;
	case Format::R16F:
	case Format::R16UI:
	case Format::R16I:
	case Format::R16_SNORM:
	case Format::R16:
		return 2;
	case Format::R8UI:
	case Format::R8I:
	case Format::R8_SNORM:
	case Format::R8:
		return 1;
	case Format::DEPTH_COMPONENT16:
		return 2;
	case Format::DEPTH_COMPONENT24:
		return 3;
	case Format::DEPTH_COMPONENT32F:
	case Format::DEPTH_COMPONENT32:
	case Format::DEPTH24_STENCIL8:
	case Format::UNSIGNED_INT_24_8:
		return 4;
	case Format::DEPTH32F_STENCIL8:
	case Format::FLOAT_32_UNSIGNED_INT_24_8_REV:
		return 8;
	case Format::FORMAT_UNKNOWN:
		return 0;
	// RGBA8/BGRA8/RGB10_A2*/R11F_G11F_B10F/DEPTH_STENCIL and anything else uncompressed: 4 bytes
	// covers every remaining format this client actually creates.
	default:
		return 4;
	}
}

size_t Texture::GetMemoryBytes() const
{
	// Computed from dimensions/format rather than summing a retained buffer: the CPU-side copy
	// (m_CI.images) is expected to be released once uploaded, and this estimate stays correct -
	// and needs no held memory of its own - whether or not that copy is still around.
	if (m_CI.width == 0 || m_CI.height == 0 || m_CI.format == Format::FORMAT_UNKNOWN)
	{
		return 0;
	}

	const size_t depth		 = std::max<uint32_t>(m_CI.depth, 1);
	const size_t arrayCount = std::max<uint32_t>(m_CI.arrayCount, 1);
	const size_t faces		 = ((m_CI.type & Type::TEXTURE_CUBE_MAP) == Type::TEXTURE_CUBE_MAP) ? 6 : 1;
	const size_t mipCount	 = std::max<uint32_t>(m_CI.mipCount, 1);

	const size_t bitsPerBlock = BitsPerPixelForCompression(m_CI.compression) * 16; // 4x4 texels/block
	const size_t blockDim	  = (m_CI.compression != CompressionFormat::UNCOMPRESSED) ? 4 : 1;
	const size_t bytesPerPixel = (blockDim == 1) ? BytesPerPixelForFormat(m_CI.format) : 0;

	size_t total = 0;
	for (size_t mip = 0; mip < mipCount; ++mip)
	{
		const uint32_t mw = std::max<uint32_t>(m_CI.width >> mip, 1);
		const uint32_t mh = std::max<uint32_t>(m_CI.height >> mip, 1);
		size_t mipBytes;
		if (blockDim > 1)
		{
			const size_t blocksWide = (mw + blockDim - 1) / blockDim;
			const size_t blocksHigh = (mh + blockDim - 1) / blockDim;
			mipBytes				= blocksWide * blocksHigh * (bitsPerBlock / 8);
		}
		else
		{
			mipBytes = (size_t)mw * mh * bytesPerPixel;
		}
		total += mipBytes * depth;
	}
	return total * arrayCount * faces;
}
