#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "libavstream/common_maths.h"
#include "libavstream/memory.hpp"

#include "material_extensions.h"

namespace avs
{
	//Convert from wide char to byte char.
	//We should really NOT use wide strings for the names of textures and materials, as we will want to support UTF-8.
	// Roderick: wstring is not UTF-8, but UTF-16, favoured only by Microsoft.
	// The correct unicode to use in most circumstances is UTF-8, which is represented adequately
	// by an std::string.
	static std::string convertToByteString(std::wstring wideString)
	{
		std::string byteString;
		byteString.resize(wideString.size());

		size_t stringSize = wideString.size() + 1;
	#if WIN32
		size_t charsConverted;
		wcstombs_s(&charsConverted, const_cast<char*>(byteString.data()), stringSize, wideString.data(), stringSize);
	#else
		wcstombs(const_cast<char*>(byteString.data()), wideString.data(), stringSize);
	#endif

		return byteString;
	}

	enum class SamplerFilter : uint32_t
	{
		NEAREST,					//GL_NEAREST (0x2600)
		LINEAR,						//GL_LINEAR (0x2601)
		NEAREST_MIPMAP_NEAREST,		//GL_NEAREST_MIPMAP_NEAREST (0x2700)
		LINEAR_MIPMAP_NEAREST,		//GL_LINEAR_MIPMAP_NEAREST (0x2701)
		NEAREST_MIPMAP_LINEAR,		//GL_NEAREST_MIPMAP_LINEAR (0x2702)
		LINEAR_MIPMAP_LINEAR,		//GL_LINEAR_MIPMAP_LINEAR (0x2703)
	};

	enum class SamplerWrap : uint32_t
	{
		REPEAT,					//GL_REPEAT (0x2901)
		CLAMP_TO_EDGE,			//GL_CLAMP_TO_EDGE (0x812F)
		CLAMP_TO_BORDER,		//GL_CLAMP_TO_BORDER (0x812D)
		MIRRORED_REPEAT,		//GL_MIRRORED_REPEAT (0x8370)
		MIRROR_CLAMP_TO_EDGE,	//GL_MIRROR_CLAMP_TO_EDGE (0x8743)
	};

	//Just copied the Unreal texture formats, this will likely need changing.
	enum class TextureFormat : uint32_t
	{
		INVALID,
		G8,
		BGRA8,
		BGRE8,
		RGBA16,
		RGBA16F,
		RGBA8,
		RGBE8,
		D16F,
		D24F,
		D32F,
		RGBA32F,
		MAX
	};
	
	enum class TextureCompression : uint32_t
	{
		UNCOMPRESSED = 0,
		BASIS_COMPRESSED
	};

	struct Sampler 
	{
		SamplerFilter magFilter;
		SamplerFilter minFilter;
		SamplerWrap wrapS;
		SamplerWrap wrapT;
	};

	struct Texture 
	{
		std::string name;

		uint32_t width;
		uint32_t height;
		uint32_t depth;
		uint32_t bytesPerPixel;
		uint32_t arrayCount;
		uint32_t mipCount;

		TextureFormat format;
		TextureCompression compression;
    
		uint32_t dataSize;
		unsigned char *data;

		uid sampler_uid = 0;

		friend std::wostream& operator<< (std::wostream& out, const Texture& texture)
		{
			//Name needs its own line, so spaces can be included.
			out << std::wstring{texture.name.begin(), texture.name.end()} << std::endl;

			out << texture.width
				<< " " << texture.height
				<< " " << texture.depth
				<< " " << texture.bytesPerPixel
				<< " " << texture.arrayCount
				<< " " << texture.mipCount
				<< " " << static_cast<uint32_t>(texture.format)
				<< " " << static_cast<uint32_t>(texture.compression)
				<< " " << texture.sampler_uid
				<< " " << texture.dataSize
				<< std::endl;
			
			///TODO: Figure out how to convert the data to wide char without going through each index. ostream::write(...) is detecting the malformed data and failing the write.
			{
				//size_t characterAmount = static_cast<size_t>(ceil(texture.dataSize / (sizeof(wchar_t) / static_cast<float>(sizeof(unsigned char)))));
				//wchar_t* dataBuffer = new wchar_t[characterAmount];
				//memcpy_s(dataBuffer, characterAmount * sizeof(wchar_t), texture.data, texture.dataSize);

				//out.write(dataBuffer, characterAmount);

				//delete[] dataBuffer;
			}

			size_t characterAmount = texture.dataSize;
			wchar_t* dataBuffer = new wchar_t[characterAmount];
			for(size_t i = 0; i < characterAmount; i++)
			{
				dataBuffer[i] = texture.data[i];
			}

			out.write(dataBuffer, characterAmount);

			delete[] dataBuffer;

			return out;
		}

		friend std::wistream& operator>> (std::wistream& in, Texture& texture)
		{
			//Step past new line that may be next in buffer.
			if(in.peek() == '\n') in.get();

			//Read name with spaces included.
			std::wstring wideName;
			std::getline(in, wideName);

			texture.name = convertToByteString(wideName);

			uint32_t format, compression;

			in >> texture.width;
			in >> texture.height;
			in >> texture.depth;
			in >> texture.bytesPerPixel;
			in >> texture.arrayCount;
			in >> texture.mipCount;
			in >> format;
			in >> compression;
			in >> texture.sampler_uid;
			in >> texture.dataSize;

			{
				//Discard new line.
				in.get();

				size_t characterAmount = texture.dataSize;
				wchar_t* dataBuffer = new wchar_t[characterAmount];
				in.read(dataBuffer, characterAmount);

				texture.data = new unsigned char[texture.dataSize];
				for(size_t i = 0; i < characterAmount; i++)
				{
					texture.data[i] = static_cast<unsigned char>(dataBuffer[i]);
				}				

				delete[] dataBuffer;
			}

			texture.format = static_cast<TextureFormat>(format);
			texture.compression = static_cast<TextureCompression>(compression);

			return in;
		}
	};
	
	struct TextureAccessor
	{
		uid index = 0;		//An index into an array of texture.
		uid texCoord = 0;	//A reference to TEXCOORD_<N>
		
		vec2 tiling = {1.0f, 1.0f};
		
		union
		{
			float scale = 1.0f;		//Used in normal textures only.
			float strength;			//Used in occlusion textures only.
		};

		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const TextureAccessor& textureAccessor)
		{
			return out << textureAccessor.index
				<< " " << textureAccessor.texCoord
				<< " " << textureAccessor.tiling
				<< " " << textureAccessor.scale;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, TextureAccessor& textureAccessor)
		{
			return in >> textureAccessor.index
				>> textureAccessor.texCoord
				>> textureAccessor.tiling
				>> textureAccessor.scale;
		}
	};
	enum class RoughnessMode: uint16_t
	{
		CONSTANT=0,
		ROUGHNESS,
		SMOOTHNESS
	};

       template<typename istream> istream& operator>>( istream  &is, RoughnessMode &obj ) { 
	   is>>*((char*)&obj);
         return is;            
      }
	struct PBRMetallicRoughness
	{
		TextureAccessor baseColorTexture;
		vec4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};

		TextureAccessor metallicRoughnessTexture;
		float metallicFactor = 1.0f;
		float roughnessMultiplier = 1.0f;
		float roughnessOffset = 1.0f;
		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const PBRMetallicRoughness& metallicRoughness)
		{
			return out << metallicRoughness.baseColorTexture
				<< " " << metallicRoughness.baseColorFactor
				<< " " << metallicRoughness.metallicRoughnessTexture
				<< " " << metallicRoughness.metallicFactor
				<< " " << metallicRoughness.roughnessMultiplier
				<< " " << metallicRoughness.roughnessOffset;
				// TODO: roughnessMode not implemented here.
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, PBRMetallicRoughness& metallicRoughness)
		{
			return 
				in>> metallicRoughness.baseColorTexture
				>> metallicRoughness.baseColorFactor
				>> metallicRoughness.metallicRoughnessTexture
				>> metallicRoughness.metallicFactor
				>> metallicRoughness.roughnessMultiplier
				>> metallicRoughness.roughnessOffset
				;
		}
	};
	struct Material
	{
		std::string name;

		PBRMetallicRoughness pbrMetallicRoughness;
		TextureAccessor normalTexture;
		TextureAccessor occlusionTexture;
		TextureAccessor emissiveTexture;
		vec3 emissiveFactor = {1.0f, 1.0f, 1.0f};

		std::unordered_map<MaterialExtensionIdentifier, std::shared_ptr<MaterialExtension>> extensions; //Mapping of extensions for a material. There should only be one extension per identifier.
	
		friend std::ostream& operator<< (std::ostream& out, const Material& material)
		{
			//Name needs its own line, so spaces can be included.
			out << material.name << std::endl;

			return out <<  material.pbrMetallicRoughness
				<< " " << material.normalTexture
				<< " " << material.occlusionTexture
				<< " " << material.emissiveTexture
				<< " " << material.emissiveFactor;
		}

		friend std::wostream& operator<< (std::wostream& out, const Material& material)
		{
			//Name needs its own line, so spaces can be included.
			out << std::wstring{material.name.begin(), material.name.end()} << std::endl;

			return out << material.pbrMetallicRoughness
				<< " " << material.normalTexture
				<< " " << material.occlusionTexture
				<< " " << material.emissiveTexture
				<< " " << material.emissiveFactor;
		}

		friend std::wistream& operator>> (std::wistream& in, Material& material)
		{
			//Step past new line that may be next in buffer.
			if(in.peek() == '\n') in.get(); 

			//Read name with spaces included.
			std::wstring wideName;
			std::getline(in, wideName);

			material.name = convertToByteString(wideName);

			return in >> material.pbrMetallicRoughness
				>> material.normalTexture
				>> material.occlusionTexture
				>> material.emissiveTexture
				>> material.emissiveFactor;
		}
	};
};