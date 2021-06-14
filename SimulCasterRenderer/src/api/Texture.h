// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once
 
#include "Common.h"
#include "Sampler.h"

namespace scr
{
	//Interface for Texture
	class Texture : public APIObject
	{
	public:
		enum class Slot : uint32_t
		{
			DIFFUSE,	//Slot 0: DIFFUSE	RGBA Colour Texture
			NORMAL,		//Slot 1: NORMAL	R: Tangent, G: Bi-normals and B: Normals
			COMBINED,	//Slot 2: COMBINED	R: Ambient Occlusion, G: Roughness, B: Metallic, A: Specular
			UNKNOWN = 65536,
		};
		enum class Type : uint32_t
		{
			TEXTURE_UNKNOWN = 0,
			TEXTURE_1D,
			TEXTURE_2D,
			TEXTURE_2D_EXTERNAL_OES,	// External video texture for OpenGL.
			TEXTURE_3D,
			TEXTURE_1D_ARRAY,
			TEXTURE_2D_ARRAY,
			TEXTURE_2D_MULTISAMPLE,
			TEXTURE_2D_MULTISAMPLE_ARRAY,
			TEXTURE_CUBE_MAP,
			TEXTURE_CUBE_MAP_ARRAY
		};
		enum class Format : uint32_t
		{
			FORMAT_UNKNOWN = 0,
			RGBA32F,									
			RGBA32UI,									
			RGBA32I,									
			RGBA16F,									
			RGBA16UI,									
			RGBA16I,									
			RGBA16_SNORM,								
			RGBA16,										
			RGBA8UI,									
			RGBA8I,										
			RGBA8_SNORM,								
			RGBA8,
			BGRA8,

			RGB10_A2UI,
			RGB10_A2,
			
			RGB32F,
			R11F_G11F_B10F,										
			
			RG32F,										
			RG32UI,										
			RG32I,										
			RG16F,										
			RG16UI,										
			RG16I,										
			RG16_SNORM,									
			RG16,										
			RG8UI,										
			RG8I,										
			RG8_SNORM,									
			RG8,										
			
			R32F,										
			R32UI,										
			R32I,										
			R16F,										
			R16UI,										
			R16I,										
			R16_SNORM,									
			R16,										
			R8UI,
			R8I,										
			R8_SNORM,	
			R8,											

			DEPTH_COMPONENT32F,						
			DEPTH_COMPONENT32,							
			DEPTH_COMPONENT24,							
			DEPTH_COMPONENT16,							
			DEPTH_STENCIL,								
			DEPTH32F_STENCIL8,							
			DEPTH24_STENCIL8,							

			UNSIGNED_INT_24_8,							
			FLOAT_32_UNSIGNED_INT_24_8_REV, 
		};
		enum class SampleCountBit : uint32_t
		{
			SAMPLE_COUNT_1_BIT	= 0x00000001,
			SAMPLE_COUNT_2_BIT	= 0x00000002,
			SAMPLE_COUNT_4_BIT	= 0x00000004,
			SAMPLE_COUNT_8_BIT	= 0x00000008,
			SAMPLE_COUNT_16_BIT = 0x00000010,
			SAMPLE_COUNT_32_BIT = 0x00000020,
			SAMPLE_COUNT_64_BIT = 0x00000040,
		};
		
		//All compression formats a texture can be compressed in.
		enum class CompressionFormat: uint32_t
		{
			UNCOMPRESSED=0,
			BC1,
			BC3,
			BC4,
			BC5,
			ETC1,
			ETC2,
			PVRTC1_4_OPAQUE_ONLY,
			BC7_M6_OPAQUE_ONLY
		};
		
		struct TextureCreateInfo
		{
			std::string name;

			uint32_t width = 0;
			uint32_t height = 0;
			uint32_t depth = 0;
			uint32_t bytesPerPixel = 0;
			uint32_t arrayCount = 0;
			uint32_t mipCount = 0;

			Slot slot = Slot::UNKNOWN;
			Type type = Type::TEXTURE_UNKNOWN;
			Format format = Format::FORMAT_UNKNOWN;
			SampleCountBit sampleCount = SampleCountBit::SAMPLE_COUNT_1_BIT;
			std::vector<size_t> mipSizes;
			std::vector<const uint8_t*> mips;

			CompressionFormat compression = CompressionFormat::UNCOMPRESSED; //The format the texture is compressed in.

			bool externalResource = false;	// If true, the actual API resource will be created and managed externally on a per-platform basis.
		};

		static const avs::vec3 DUMMY_DIMENSIONS; //X = Width, Y = Height, Z = Depth

	protected:
		TextureCreateInfo m_CI;

		std::shared_ptr<Sampler> m_Sampler = nullptr;

	public:
		Texture(const RenderPlatform* const r)
			: APIObject(r), m_CI()
		{}

		virtual ~Texture()
		{
			/* RK: Purpose of all this unclear..
			m_CI.width = 0;
			m_CI.height = 0; 
			m_CI.depth = 0;
			m_CI.bytesPerPixel = 0;
			m_CI.slot = Slot::UNKNOWN;
			m_CI.type = Type::TEXTURE_UNKNOWN;
			m_CI.format = Format::FORMAT_UNKNOWN;
			m_CI.sampleCount = SampleCountBit::SAMPLE_COUNT_1_BIT;
			m_CI.mipSizes.clear();*/

			//Free mip data.
			for(size_t mipLevel = 0; mipLevel < m_CI.mips.size(); mipLevel++)
			{
				delete[] m_CI.mips[mipLevel];
			}
			m_CI.mips.clear();
		}

		//Returns whether the texture is valid.
		bool IsValid() const
		{
			return m_CI.width != 0 && m_CI.height != 0 && m_CI.depth != 0;
		}

		//Returns whether the passed texture is valid.
		static bool IsValid(scr::Texture* texture)
		{
			return texture && texture->IsValid();
		}

		//Returns whether the texture is a dummy/placeholder texture.
		bool IsDummy() const
		{
			return m_CI.width == DUMMY_DIMENSIONS.x && m_CI.height == DUMMY_DIMENSIONS.y && m_CI.depth == DUMMY_DIMENSIONS.z;
		}

		//Returns whether the passed texture is a dummy/placeholder texture.
		static bool IsDummy(scr::Texture* texture)
		{
			return texture && texture->IsDummy();
		}

		//For cubemaps pass in a uint8_t* to continuous array of data for all 6 sides. Width, height, depth and bytesPerPixel will be the same for all faces.
		virtual void Create(const TextureCreateInfo& pTextureCreateInfo) = 0;
		virtual void Destroy() = 0;

		virtual void UseSampler(const std::shared_ptr<Sampler>& sampler) = 0;
		virtual void GenerateMips() = 0;

		inline std::shared_ptr<Sampler> GetSampler() { return m_Sampler; }
		inline const TextureCreateInfo GetTextureCreateInfo() const { return m_CI;}

		virtual bool ResourceInUse(int timeout) = 0;
		std::function<bool(Texture*, int)> ResourceInUseCallback = &Texture::ResourceInUse;

		friend class FrameBuffer;

	protected:
		virtual void Bind(uint32_t mip,uint32_t layer) const = 0;
		virtual void BindForWrite(uint32_t slot,uint32_t mip,uint32_t layer) const =0;
		virtual void Unbind() const = 0;
	};
}