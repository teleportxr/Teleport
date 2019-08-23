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
		struct TextureCreateInfo
		{
			uint32_t width;
			uint32_t height;
			uint32_t depth;
			uint32_t bytesPerPixel;
			uint32_t arrayCount;
			uint32_t mipCount;

			Slot slot;
			Type type;
			Format format;
			SampleCountBit sampleCount;
			size_t size;
			const uint8_t* data;
		};

	protected:
		TextureCreateInfo m_CI;

		const Sampler* m_Sampler = nullptr;

	public:
		Texture(RenderPlatform *r) : APIObject(r) {}
		virtual ~Texture()
		{
			m_CI.width = 0;
			m_CI.height = 0; 
			m_CI.depth = 0;
			m_CI.bytesPerPixel = 0;
			m_CI.slot = Slot::UNKNOWN;
			m_CI.type = Type::TEXTURE_UNKNOWN;
			m_CI.format = Format::FORMAT_UNKNOWN;
			m_CI.sampleCount = SampleCountBit::SAMPLE_COUNT_1_BIT;
			m_CI.size = 0;
			m_CI.data = nullptr;

			m_Sampler = nullptr;
		}

		//For cubemaps pass in a uint8_t* to continuous array of data for all 6 sides. Width, height, depth and bytesPerPixel will be the same for all faces.
		virtual void Create(TextureCreateInfo* pTextureCreateInfo) = 0;
		virtual void Destroy() = 0;

		virtual void UseSampler(const Sampler* sampler) = 0;
		virtual void GenerateMips() = 0;

		inline const Sampler* GetSampler() const { return m_Sampler; }

		virtual bool ResourceInUse(int timeout) = 0;
		std::function<bool(Texture*, int)> ResourceInUseCallback = &Texture::ResourceInUse;

		friend class FrameBuffer;

	protected:
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
	};
}