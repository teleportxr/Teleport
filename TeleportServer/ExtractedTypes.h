#pragma once

#include <ctime>
#include <algorithm>
#include <vector>
#include <filesystem>

#ifdef _MSC_VER
#include "comdef.h"
#endif

#include "libavstream/geometry/material_interface.hpp"
#include "libavstream/geometry/mesh_interface.hpp"
#include "Font.h"
#include "TeleportCore/StringFunctions.h"
#include "Texture.h"

namespace teleport
{
	namespace server
	{
		struct ExtractedMesh
		{
			const char* fileExtension() const
			{
				return ".mesh";
			}
			static const char* fileExtensions()
			{
				return ".mesh";
			}
			std::string MakeFilename(std::string path) const
			{
				std::string file_name;
				file_name = path + fileExtension();
				return file_name;
			}
			std::string getName() const
			{
				return mesh.name;
			}
			bool IsValid() const
			{
				return true;
			}
			std::time_t lastModified;
			avs::Mesh mesh;
			avs::CompressedMesh compressedMesh;
			bool operator==(const ExtractedMesh& t) const
			{
				if (!(mesh == t.mesh))
					return false;
				return compressedMesh ==t.compressedMesh;
			}
			bool Verify(const ExtractedMesh& t) const
			{
				return operator==(t);
			}
			void GetAccessorRange(uint64_t& lowest, uint64_t& highest) const
			{
				lowest = 0xFFFFFFFFFFFFFFFF;
				highest = 0;
				mesh.GetAccessorRange(lowest, highest);
				compressedMesh.GetAccessorRange(lowest, highest);
			}
			void ResetAccessorRange()
			{
				uint64_t lowest, highest;
				GetAccessorRange(lowest, highest);
				mesh.ResetAccessors(lowest);
				compressedMesh.ResetAccessors(lowest);
			}

			template<class OutStream>
			friend OutStream& operator<< (OutStream& out, const ExtractedMesh& meshData)
			{
				out.writeChunk(meshData.lastModified);
				out << meshData.mesh
					<< meshData.compressedMesh;
				return out;
			}

			template<class InStream>
			friend InStream& operator>> (InStream& in, ExtractedMesh& meshData)
			{
				in.readChunk(meshData.lastModified);
				in >> meshData.mesh >> meshData.compressedMesh;
				// having loaded, now rescale the uid's:
				meshData.ResetAccessorRange();
				return in;
			}
		};

		struct ExtractedMaterial
		{
			const char* fileExtension() const
			{
				return ".material";
			}
			static const char* fileExtensions()
			{
				return ".material";
			}
			std::string getName() const
			{
				return material.name;
			}
			std::string MakeFilename(std::string path) const
			{
				std::string file_name;
				file_name = path + fileExtension();
				return file_name;
			}
			bool IsValid() const
			{
				return true;
			}
			std::time_t lastModified;
			avs::Material material;

			bool Verify(const ExtractedMaterial& t) const
			{
				return material.Verify(t.material);
			}
			template<typename OutStream>
			friend OutStream& operator<< (OutStream& out, const ExtractedMaterial& materialData)
			{
				out.writeChunk(materialData.lastModified);
				out << materialData.material;
				return out;
			}

			template<typename InStream>
			friend InStream& operator>> (InStream& in, ExtractedMaterial& materialData)
			{
				in.readChunk(materialData.lastModified);
				in >> materialData.material;
				return in;
			}
		};
		// Stores data on a texture that is to be compressed.
		struct PrecompressedTexture
		{
			std::vector<std::vector<uint8_t>> images;
			size_t numMips;
			bool genMips; // if false, numMips tells how many are in the data already.
			bool highQualityUASTC;
			avs::TextureCompression textureCompression = avs::TextureCompression::UNCOMPRESSED;
			avs::TextureFormat format = avs::TextureFormat::INVALID;
		};
		struct ExtractedTexture
		{
		// In order of preference:
			static const char* fileExtensions()
			{
				return ".ktx2;.basis;.ktx;.texture";// diabled ;.png for now.
			}
			const char *fileExtension() const
			{
				if(extension.size())
					return extension.c_str();// diabled ;.png for now.
				else
					return ".texture";
			}
			std::string getName() const
			{
				return texture.name;
			}
			std::string MakeFilename(std::string path) const
			{
				std::string file_name;
				file_name += path;
				std::string &s=const_cast<std::string&>(extension);
				if (texture.compression == avs::TextureCompression::BASIS_COMPRESSED)
					s= ".ktx2";
				else if (texture.compression == avs::TextureCompression::PNG)
					s= ".texture";
				file_name+=extension;
				// if (resource.texture.compression == avs::TextureCompression::KTX)
				//		file_name += ".ktx";
				return file_name;
			}

			void SetNameFromPath(std::string path)
			{
				texture.name = std::filesystem::path(path).filename().generic_u8string();
				size_t hash_pos = texture.name.rfind('#');
				size_t tilde_pos=texture.name.rfind('~');
				if(hash_pos<tilde_pos)
					tilde_pos=hash_pos;
				if (tilde_pos < texture.name.length())
					texture.name = texture.name.substr(tilde_pos + 1, texture.name.length() - tilde_pos - 1);
			}
			std::time_t lastModified;
			avs::Texture texture;
			std::string extension;
			bool IsValid() const
			{
				return texture.images.size()!=0||texture.compressedData.size()!=0;
			}
			bool Verify(const ExtractedTexture& t) const
			{
				return texture==t.texture;
			}
			template<typename OutStream>
			friend OutStream& operator<< (OutStream& out, const ExtractedTexture& textureData)
			{
				size_t extPos=out.filename.rfind(".");
				string ext=textureData.extension;
				if (ext==".basis")
				{
					out.write((const char*)textureData.texture.compressedData.data(),textureData.texture.compressedData.size());
					return out;
				}
				else if (ext==".ktx")
				{
					out.write((const char*)textureData.texture.compressedData.data(),textureData.texture.compressedData.size());
					return out;
				}
				else if (ext==".ktx2")
				{
					out.write((const char*)textureData.texture.compressedData.data(),textureData.texture.compressedData.size());
					return out;
				}
				out.write((const char*)textureData.texture.compressedData.data(),textureData.texture.compressedData.size());
				if(textureData.texture.compression==avs::TextureCompression::UNCOMPRESSED)
				{
					TELEPORT_BREAK_ONCE("Uncompressed texture.");
				}
				return out;
			}
			template<typename InStream>
			friend InStream& operator>> (InStream& in, ExtractedTexture& textureData)
			{
				size_t extPos=in.filename.rfind(".");
				string ext=in.filename.substr(extPos,in.filename.length()-extPos);
				if (ext==".basis"||ext==".ktx2"||ext==".ktx")
				{
					LoadAsBasisFile(textureData, in.readData(), in.filename);
					textureData.extension=ext;
					return in;
				}
				if (ext==".png")
				{
					LoadAsPng(textureData, in.readData(), in.filename);
					textureData.extension=ext;
					return in;
				}
				LoadAsTeleportTexture(textureData, in.readData(), in.filename);
					
				//in >> textureData.texture;
				textureData.extension = ".texture";
				return in;
			}
		};

		//! Each font size represented has a FontMap.
		struct ExtractedFontAtlas
		{
			core::FontAtlas fontAtlas;
			bool IsValid() const
			{
				return true;
			}

			template<typename OutStream>
			friend OutStream& operator<< (OutStream& out, const ExtractedFontAtlas& extractedFontAtlas)
			{
				out << extractedFontAtlas.fontAtlas;
				return out;
			}

			template<typename InStream>
			friend InStream& operator>> (InStream& in, ExtractedFontAtlas& extractedFontAtlas)
			{
				in >> extractedFontAtlas.fontAtlas;
				return in;
			}
			bool Verify(const ExtractedFontAtlas& t) const
			{
				return (fontAtlas.Verify(t.fontAtlas));
			}
		};
		//! Each font size represented has a FontMap.
		struct ExtractedText
		{
			std::string text;
			bool IsValid() const
			{
				return true;
			}
		};
		//Resource that has been loaded from disk.
		struct LoadedResource
		{
			avs::uid id;		// The id of the resource in this session.
			const char* name;	// Name of the asset to tell it apart from assets with the GUID; i.e. they come from the same source file.
			std::time_t lastModified;

			LoadedResource() = default;
			LoadedResource(avs::uid uid,  const char* name, std::time_t lastModified)
				:id(uid),  name(name), lastModified(lastModified)
			{}
		};
	}
}