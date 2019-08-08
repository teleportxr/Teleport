// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once
 
#include "../Common.h"
#include "Sampler.h"

namespace scr
{
	//Interface for Texture
	class Texture
	{
	public:
		enum class Slot : uint32_t
		{
			DIFFUSE,	//Slot 0: DIFFUSE	RGBA Colour Texture
			NORMAL,		//Slot 1: NORMAL	R: Tangent, G: Bi-normals and B: Normals
			COMBINED	//Slot 2: COMBINED	R: Ambient Occlusion, G: Roughness, B: Metallic, A: Specular
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
			
			R11F_G11F_B10F,								
			RGB10_A2UI,									
			RGB10_A2,									
			
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
		enum class SampleCount : uint32_t
		{
			SAMPLE_COUNT_1_BIT = 1,
			SAMPLE_COUNT_2_BIT = 2,
			SAMPLE_COUNT_4_BIT = 4,
			SAMPLE_COUNT_8_BIT = 8,
			SAMPLE_COUNT_16_BIT = 16,
			SAMPLE_COUNT_32_BIT = 32,
			SAMPLE_COUNT_64_BIT = 64,
		};

	protected:
		uint32_t m_Width, m_Height, m_Depth, m_BitsPerPixel;

		Slot m_Slot;
		Type m_Type;
		Format m_Format;
		SampleCount m_SampleCount;

		size_t m_Size;
		const uint8_t* m_Data;

		const Sampler* m_Sampler;

	public:
			
		virtual ~Texture()
		{
			m_Width = 0;
			m_Height = 0; 
			m_Depth = 0;
			m_BitsPerPixel = 0;

			m_Type = Type::TEXTURE_UNKNOWN;
			m_Format = Format::FORMAT_UNKNOWN;

			m_Size = 0;
			m_Data = nullptr;

			m_Sampler = nullptr;
		}

		//For cubemaps pass in a uint8_t* to continuous array of data for all 6 sides. Width, height, depth and bitsPerPixel will be the same for all faces.
		virtual void Create(Slot slot, Type type, Format format, SampleCount sampleCount, uint32_t width, uint32_t height, uint32_t depth, uint32_t bitsPerPixel, const uint8_t* data) = 0;
		virtual void Destroy() = 0;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void UseSampler(const Sampler* sampler) = 0;
		virtual void GenerateMips() = 0;

		inline const Sampler* GetSampler() const { return m_Sampler; }

		virtual bool ResourceInUse(int timeout) = 0;
		std::function<bool(Texture*, int)> ResourceInUseCallback = &Texture::ResourceInUse;

		friend class FrameBuffer;
	};
}