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
	template<typename T> bool verify(const T& t1,const T& t2)
	{
		return (t1==t2);
	}
	template<> inline bool verify(const float& t1,const float& t2)
	{
		return fabs(t1-t2)/(fabs(t1)+fabs(t2)+0.0001f)<0.0001f;
	}
	template<> inline bool verify(const vec2& t1,const vec2& t2)
	{
		return verify(t1.x,t2.x)&&verify(t1.y,t2.y);
	}
	template<> inline bool verify(const vec3& t1,const vec3& t2)
	{
		return verify(t1.x,t2.x)&&verify(t1.y,t2.y)
				&&verify(t1.z,t2.z);
	}
	template<> inline bool verify(const vec4& t1,const vec4& t2)
	{
		return verify(t1.x,t2.x)&&verify(t1.y,t2.y)
				&&verify(t1.z,t2.z)&&verify(t1.w,t2.w);
	}
	#define TELEPORT_VERIFY(t1,t2) \
	if(!verify(t1,t2))\
	{\
		std::cerr<<"Verify failed for "<<#t1<<"\n";\
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
		BASIS_COMPRESSED,
		PNG
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

		float valueScale=1.0f;	// Scale for the texture values as transported, so we can reconstruct the true dynamic range. 

		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const Texture& texture)
		{
			//Name needs its own line, so spaces can be included.
			out<< std::wstring{texture.name.begin(), texture.name.end()} << std::endl;

			out.operator<<((unsigned int)texture.width);
			out	<<" " << texture.height;
			out	<<" " << texture.depth;
			out	<<" " << texture.bytesPerPixel;
			out	<<" " << texture.arrayCount;
			out	<<" " << texture.mipCount;
			out	<<" " << static_cast<uint32_t>(texture.format);
			out	<<" " << static_cast<uint32_t>(texture.compression);
			if(texture.sampler_uid!=0)
				out	<<" ";
			out	<<texture.sampler_uid;
			
			out	<<" " << texture.dataSize
				<<" " << texture.valueScale
				<< std::endl;

			size_t characterCount = texture.dataSize;
			wchar_t* dataBuffer = new wchar_t[characterCount];
			for(size_t i = 0; i < characterCount; i++)
			{
				dataBuffer[i] = texture.data[i];
			}

			out.write(dataBuffer, characterCount);

			delete[] dataBuffer;

			return out;
		}
		
		template<typename InStream>
		friend InStream& operator>> (InStream& in, Texture& texture)
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
			in >> texture.valueScale;

			{
				//Discard new line.
				in.get();

				size_t characterCount = texture.dataSize;
				wchar_t* dataBuffer = new wchar_t[characterCount];
				in.read(dataBuffer, characterCount);

				texture.data = new unsigned char[texture.dataSize];
				for(size_t i = 0; i < characterCount; i++)
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
			//wchar_t tc=(wchar_t)textureAccessor.texCoord;
			out<<textureAccessor.index;
 			//out.write(&tc,1);
			out	<< textureAccessor.tiling
				<< " " << textureAccessor.scale;
			return out;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, TextureAccessor& textureAccessor)
		{
			//guid g;
			//wchar_t tc=0;
			in>>textureAccessor.index;
			//in.read(&tc,1);
			//in>>textureAccessor.texCoord;
			in>> textureAccessor.tiling
				>> textureAccessor.scale;
			
			textureAccessor.texCoord=(uint8_t)0;
			//textureAccessor.index g=OutStream.uid_to_guid(g);
			return in;
		}
	};
	enum class RoughnessMode: uint16_t
	{
		CONSTANT=0,
		ROUGHNESS,
		SMOOTHNESS
	};

	template<typename istream> istream& operator>>( istream  &is, RoughnessMode &obj )
	{ 
		is>>*((char*)&obj);
		return is;            
	}

	template<typename OutStream> OutStream& operator<< (OutStream& out, const vec4& vec)
	{
		std::basic_ostream<wchar_t,std::char_traits<wchar_t>> &o=out;
		o << vec.x << " " << vec.y << " " << vec.z << " " << vec.w;
		return out;
	}
	
	template<typename InStream> InStream& operator>> (InStream& in, vec4& vec)
	{
		std::basic_istream<wchar_t,std::char_traits<wchar_t>> &i=in;
		i >> vec.x >> vec.y >> vec.z >> vec.w;
		return in;
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
			out << metallicRoughness.baseColorTexture;
			out<< " " ;
			out << metallicRoughness.baseColorFactor;
			out<< " " ;
			out << metallicRoughness.metallicRoughnessTexture;
			out<< " "  << metallicRoughness.metallicFactor;
			out<< " "  << metallicRoughness.roughnessMultiplier;
			out<< " " << metallicRoughness.roughnessOffset;
				// TODO: roughnessMode not implemented here.
			 return out;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, PBRMetallicRoughness& metallicRoughness)
		{
			in>> metallicRoughness.baseColorTexture;
			in>> metallicRoughness.baseColorFactor;
			in>> metallicRoughness.metallicRoughnessTexture;
			in>> metallicRoughness.metallicFactor;
			in>> metallicRoughness.roughnessMultiplier;
			in>> metallicRoughness.roughnessOffset;
				
				return in;
		}
	};
	template<> inline bool verify(const TextureAccessor& t1,const TextureAccessor& t2)
	{
		TELEPORT_VERIFY(t1.index,t2.index);
		TELEPORT_VERIFY(t1.texCoord,t2.texCoord);
		TELEPORT_VERIFY(t1.tiling,t2.tiling);
		TELEPORT_VERIFY(t1.scale,t2.scale);
		return true;
	}
	template<> inline bool verify(const PBRMetallicRoughness& t1,const PBRMetallicRoughness& t2)
	{
		TELEPORT_VERIFY(t1.baseColorTexture,t2.baseColorTexture);
		TELEPORT_VERIFY(t1.baseColorFactor,t2.baseColorFactor);
		TELEPORT_VERIFY(t1.metallicRoughnessTexture,t2.metallicRoughnessTexture);
		TELEPORT_VERIFY(t1.metallicFactor,t2.metallicFactor);
		TELEPORT_VERIFY(t1.roughnessMultiplier,t2.roughnessMultiplier);
		TELEPORT_VERIFY(t1.roughnessOffset,t2.roughnessOffset);
		return true;
	}
	struct Material
	{
		std::string name;
		PBRMetallicRoughness pbrMetallicRoughness;
		TextureAccessor normalTexture;
		TextureAccessor occlusionTexture;
		TextureAccessor emissiveTexture;
		vec3 emissiveFactor = {0.0f, 0.0f, 0.0f};

		std::unordered_map<MaterialExtensionIdentifier, std::shared_ptr<MaterialExtension>> extensions; //Mapping of extensions for a material. There should only be one extension per identifier.
		
		bool Verify(const Material &t) const
		{
			TELEPORT_VERIFY(name,t.name);
			TELEPORT_VERIFY(pbrMetallicRoughness,t.pbrMetallicRoughness);
			TELEPORT_VERIFY(normalTexture,t.normalTexture);
			TELEPORT_VERIFY(occlusionTexture,t.occlusionTexture);
			TELEPORT_VERIFY(emissiveTexture,t.emissiveTexture);
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
			out << std::wstring{material.name.begin(), material.name.end()} << std::endl;

			out << material.pbrMetallicRoughness<< " ";
			out << material.normalTexture<< " ";
			out << material.occlusionTexture<< " ";
			out << material.emissiveTexture<< " ";
			out << material.emissiveFactor<< " ";
			return out;
		}
		
		template<typename InStream>
		friend InStream& operator>> (InStream& in, Material& material)
		{
			//Step past new line that may be next in buffer.
			if(in.peek() == '\n') in.get(); 

			//Read name with spaces included.
			std::wstring wideName;
			std::getline(in, wideName);

			material.name = convertToByteString(wideName);

			in >> material.pbrMetallicRoughness;
			in >> material.normalTexture;
			in >> material.occlusionTexture;
			in >> material.emissiveTexture;
			in >> material.emissiveFactor;
			return in;
		}
	};
};