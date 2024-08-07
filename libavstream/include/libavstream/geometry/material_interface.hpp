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
	template <typename T>
	bool verify_values(const T &t1, const T &t2)
	{
		return (t1==t2);
	}
	template <>
	inline bool verify_values(const float &t1, const float &t2)
	{
		return fabs(t1-t2)/(fabs(t1)+fabs(t2)+0.0001f)<0.0001f;
	}
	template <>
	inline bool verify_values(const vec2 &t1, const vec2 &t2)
	{
		return verify_values(t1.x, t2.x) && verify_values(t1.y, t2.y);
	}
	template <>
	inline bool verify_values(const vec3 &t1, const vec3 &t2)
	{
		return verify_values(t1.x, t2.x) && verify_values(t1.y, t2.y) && verify_values(t1.z, t2.z);
	}
	template <>
	inline bool verify_values(const vec4 &t1, const vec4 &t2)
	{
		return verify_values(t1.x, t2.x) && verify_values(t1.y, t2.y) && verify_values(t1.z, t2.z) && verify_values(t1.w, t2.w);
	}
	#define TELEPORT_VERIFY(t1,t2) \
	if(!verify_values(t1,t2))\
	{\
		std::cerr<<"Verify failed for "<<#t1<<"\n";\
		return false;\
	}
	#define TELEPORT_VERIFY_ASSERT(t) \
	if(!(t))\
	{\
		std::cerr<<"Verify failed for "<<#t<<"\n";\
		return false;\
	}

	//Convert from wide char to byte char.
	//We should really NOT use wide strings for the names of textures and materials, as we will want to support UTF-8.
	// Roderick: wstring is not UTF-8, but UTF-16, favoured only by Microsoft.
	// The correct unicode to use in most circumstances is UTF-8, which is represented adequately
	// by an std::string.
	static inline std::string convertToByteString(std::wstring wideString)
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
	static inline std::string convertToByteString(const char *txt)
	{
		std::string byteString(txt);
		return byteString;
	}
	
	struct guid
	{
		guid()
		{
			txt[0]=0;
		}
		char txt[49];
		friend bool operator<(const guid &a,const guid &b)
		{
			return (strcmp(a.txt,b.txt)<0);
		};
		friend bool operator==(const guid &a,const guid &b)
		{
			return strcmp(a.txt,b.txt)==0;
		};
		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const guid& g)
		{
			out << g.txt << std::endl;
			return out;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, guid& g)
		{
			std::wstring w;
			std::getline(in, w);
			std::string str = convertToByteString(w);
			while(str.length()>0&&str[0]==' ')
			{
				str.erase(str.begin());
			}
			if(str.length()>48)
				throw std::runtime_error("guid length>48");
			strcpy(g.txt,str.c_str());
			g.txt[48]=0;
			return in;
		}
	};
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

	// This will likely need changing.
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
		RGB8,
		MAX
	};
	
	enum class TextureCompression : uint32_t
	{
		UNCOMPRESSED = 0,
		BASIS_COMPRESSED,
		PNG,
		KTX
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
		~Texture()
		{
		}
		std::string name;

		uint32_t width;
		uint32_t height;
		uint32_t depth;

		uint32_t arrayCount;
		uint32_t mipCount;
		bool cubemap=false;

		TextureFormat format;
		uint32_t bytesPerPixel;
		float valueScale=1.0f;	// Scale for the texture values as transported, so we can reconstruct the true dynamic range. 


		TextureCompression compression;
		bool compressed=false;

		
		struct Image
		{
			std::vector<uint8_t> data;
		};

		std::vector<Image> images;

		std::vector<uint8_t> compressedData;

		bool operator==(const Texture& t) const
		{
			TELEPORT_VERIFY(t.name, name);
			TELEPORT_VERIFY(t.width, width);
			TELEPORT_VERIFY(t.height, height);
			TELEPORT_VERIFY(t.depth, depth);
			TELEPORT_VERIFY(t.bytesPerPixel, bytesPerPixel);
			TELEPORT_VERIFY(t.arrayCount, arrayCount);
			TELEPORT_VERIFY(t.mipCount, mipCount);
			TELEPORT_VERIFY(t.format, format);
			TELEPORT_VERIFY(t.compression, compression);
			TELEPORT_VERIFY(t.valueScale, valueScale);
			const unsigned char* start1 = (const unsigned char*)&width;
			const unsigned char* end1 = (const unsigned char*)&compressed+sizeof(compressed);
			const unsigned char* start2 = (const unsigned char*)&t.width;
			const unsigned char *end2 = (const unsigned char *)&t.compressed + sizeof(t.compressed);
			auto c = memcmp(start1, start2, size_t(end1 - start1));
			if (c != 0)
			{
			// memory check fails but could be junk data in between aligned members, so check individually:
				TELEPORT_VERIFY(width ,t.width);
				if (height != t.height)
					return false;
				if (depth != t.depth)
					return false;
				if (bytesPerPixel != t.bytesPerPixel)
					return false;
				if (arrayCount != t.arrayCount)
					return false;
				if (mipCount != t.mipCount)
					return false;
				if (format != t.format)
					return false;
				if (compression != t.compression)
					return false;
				if (compressed != t.compressed)
					return false;
				if (valueScale != t.valueScale)
					return false;
				if (cubemap != t.cubemap)
					return false;
			}
			if(compressedData.size()&&t.compressedData.size()){
				TELEPORT_VERIFY(compressedData.size(), t.compressedData.size());
				auto compressedDataCompare = memcmp(compressedData.data(), t.compressedData.data(), t.compressedData.size());
				TELEPORT_VERIFY_ASSERT (compressedDataCompare == 0);
			}
			else if(images.size()&&t.images.size()){
				for(size_t i=0;i<images.size();i++) {
					TELEPORT_VERIFY(images[i].data.size(), t.images[i].data.size());
					auto uncompressedDataCompare = memcmp(images[i].data.data(), t.images[i].data.data(), images[i].data.size());
					TELEPORT_VERIFY_ASSERT (uncompressedDataCompare == 0);
				}
			}
			else{
				std::cerr<<"Can't compare compressed with uncompressed texture.\n";
			}
			return true;
		}
		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const Texture& texture)
		{
			//Name needs its own line, so spaces can be included.
			out<< texture.name;
			uint32_t sz=texture.compressedData.size();
			out.write((const char*)texture.compressedData.data(), sz);
			return out;
		}
		
		template<typename InStream>
		friend InStream& operator>> (InStream& in, Texture& texture)
		{
			in>>texture.name;
			while(!in.eof())
			{
				uint8_t c=0;
				in.read((char*)&c,1);
				texture.compressedData.push_back(c);
			}
			return in;
		}
	};
	#pragma pack(push,1)
	struct TextureAccessor
	{
		uid index = 0;			// Session uid of the texture.
		uint8_t texCoord = 0;	// A reference to TEXCOORD_<N>
		
		vec2 tiling = {1.0f, 1.0f};
		
		union
		{
			float scale = 1.0f;		//Used in normal textures only.
			float strength;			//Used in occlusion textures only.
		};

		template<typename OutStream>
		friend OutStream& operator<<(OutStream& out, const TextureAccessor& textureAccessor)
		{
			out<<textureAccessor.index;
			out.writeChunk(textureAccessor.tiling);
			out.writeChunk(textureAccessor.scale);
			return out;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, TextureAccessor& textureAccessor)
		{
			in>>textureAccessor.index;
			in.readChunk(textureAccessor.tiling);
			in.readChunk(textureAccessor.scale);
			textureAccessor.texCoord=(uint8_t)0;
			return in;
		}
	};
	enum class RoughnessMode: uint16_t
	{
		CONSTANT=0,
		ROUGHNESS,
		SMOOTHNESS
	};

	template<typename istream> istream& operator>>( istream  &in, RoughnessMode &obj )
	{
		in.readChunk(obj);
		return in;
	}

	template<typename OutStream> OutStream& operator<< (OutStream& out, const vec4& vec)
	{
		out.writeChunk(vec);
		return out;
	}
	
	template<typename InStream> InStream& operator>> (InStream& in, vec4& vec)
	{
		in.readChunk(vec);
		return in;
	}
	enum class MaterialMode: uint8_t
	{
		UNKNOWNMODE,
		OPAQUE_MATERIAL,
		TRANSPARENT_MATERIAL
	};
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
			out << metallicRoughness.baseColorTexture;
			out << metallicRoughness.baseColorFactor;
			out << metallicRoughness.metallicRoughnessTexture;
			out.writeChunk(metallicRoughness.metallicFactor);
			out.writeChunk(metallicRoughness.roughnessMultiplier);
			out.writeChunk(metallicRoughness.roughnessOffset);
			// TODO: roughnessMode not implemented here.
			return out;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, PBRMetallicRoughness& metallicRoughness)
		{
			in >> metallicRoughness.baseColorTexture;
			in >> metallicRoughness.baseColorFactor;
			in >> metallicRoughness.metallicRoughnessTexture;
			in.readChunk(metallicRoughness.metallicFactor);
			in.readChunk(metallicRoughness.roughnessMultiplier);
			in.readChunk(metallicRoughness.roughnessOffset);
				
			return in;
		}
	};

	template<> inline bool verify_values(const TextureAccessor& t1,const TextureAccessor& t2)
	{
		TELEPORT_VERIFY(t1.index,t2.index);
		TELEPORT_VERIFY(t1.texCoord,t2.texCoord);
		TELEPORT_VERIFY(t1.tiling,t2.tiling);
		TELEPORT_VERIFY(t1.scale,t2.scale);
		return true;
	}

	template<> inline bool verify_values(const PBRMetallicRoughness& t1,const PBRMetallicRoughness& t2)
	{
		TELEPORT_VERIFY(t1.baseColorTexture,t2.baseColorTexture);
		TELEPORT_VERIFY(t1.baseColorFactor,t2.baseColorFactor);
		TELEPORT_VERIFY(t1.metallicRoughnessTexture,t2.metallicRoughnessTexture);
		TELEPORT_VERIFY(t1.metallicFactor,t2.metallicFactor);
		TELEPORT_VERIFY(t1.roughnessMultiplier,t2.roughnessMultiplier);
		TELEPORT_VERIFY(t1.roughnessOffset,t2.roughnessOffset);
		return true;
	}
	#pragma pack(pop)

	struct Material
	{
		std::string name;
		MaterialMode materialMode=MaterialMode::UNKNOWNMODE;
		PBRMetallicRoughness pbrMetallicRoughness;
		TextureAccessor normalTexture;
		TextureAccessor occlusionTexture;
		TextureAccessor emissiveTexture;
		vec3 emissiveFactor = {0.0f, 0.0f, 0.0f};
		bool doubleSided=false; // TODO: stream this
		uint8_t lightmapTexCoordIndex=0;
		std::unordered_map<MaterialExtensionIdentifier, std::shared_ptr<MaterialExtension>> extensions; //Mapping of extensions for a material. There should only be one extension per identifier.
		
		bool Verify(const Material &t) const
		{
			TELEPORT_VERIFY(name,t.name);
			TELEPORT_VERIFY(pbrMetallicRoughness,t.pbrMetallicRoughness);
			TELEPORT_VERIFY(normalTexture,t.normalTexture);
			TELEPORT_VERIFY(occlusionTexture,t.occlusionTexture);
			TELEPORT_VERIFY(emissiveTexture,t.emissiveTexture);
			TELEPORT_VERIFY(materialMode,t.materialMode);
			TELEPORT_VERIFY(doubleSided,t.doubleSided);
			TELEPORT_VERIFY(lightmapTexCoordIndex,t.lightmapTexCoordIndex);
			return true;
		}
		inline std::vector<avs::uid> GetTextureUids() const
		{
			std::vector<avs::uid> uids;
			uids.reserve(5);
			if(pbrMetallicRoughness.baseColorTexture.index)
				uids.push_back(pbrMetallicRoughness.baseColorTexture.index);
			
			if(pbrMetallicRoughness.metallicRoughnessTexture.index)
				uids.push_back(pbrMetallicRoughness.metallicRoughnessTexture.index);
			
			if(normalTexture.index)
				uids.push_back(normalTexture.index);
			if(occlusionTexture.index)
				uids.push_back(occlusionTexture.index);
			if(emissiveTexture.index)
				uids.push_back(emissiveTexture.index);
			return uids;
		}
		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const Material& material)
		{
			//Name needs its own line, so spaces can be included.
			out << material.name;

			out << material.pbrMetallicRoughness;
			out << material.normalTexture;
			out << material.occlusionTexture;
			out << material.emissiveTexture;
			out.writeChunk(material.emissiveFactor);
			out.writeChunk(material.materialMode);
			out.writeChunk(material.doubleSided);
			out.writeChunk(material.lightmapTexCoordIndex);
			return out;
		}
		
		template<typename InStream>
		friend InStream& operator>> (InStream& in, Material& material)
		{
			in >> material.name;
			in >> material.pbrMetallicRoughness;
			in >> material.normalTexture;
			in >> material.occlusionTexture;
			in >> material.emissiveTexture;
			in.readChunk(material.emissiveFactor);
			in.readChunk(material.materialMode);
			in.readChunk(material.doubleSided);
			in.readChunk(material.lightmapTexCoordIndex);
			return in;
		}
	};
};