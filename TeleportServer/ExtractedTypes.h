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
			static const char* fileExtension()
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
			static const char* fileExtension()
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
			static const char* fileExtension()
			{
				return ".ktx2;.basis;.ktx;.texture;.png";
			}
			std::string getName() const
			{
				return texture.name;
			}
			std::string MakeFilename(std::string path) const
			{
				std::string file_name;
				file_name += path;
				if (texture.compression == avs::TextureCompression::BASIS_COMPRESSED)
					file_name += ".ktx2";
				else if (texture.compression == avs::TextureCompression::PNG)
					file_name += ".texture";
				// if (resource.texture.compression == avs::TextureCompression::KTX)
				//		file_name += ".ktx";
				return file_name;
			}

			void SetNameFromPath(std::string path)
			{
				texture.name = std::filesystem::path(path).filename().generic_u8string();
				size_t hash_pos = texture.name.rfind('#');
				if (hash_pos < texture.name.length())
					texture.name = texture.name.substr(hash_pos + 1, texture.name.length() - hash_pos - 1);
			}
			std::time_t lastModified;
			avs::Texture texture;
			bool IsValid() const
			{
				return texture.images.size()!=0;
			}
			bool Verify(const ExtractedTexture& t) const
			{
				return texture==t.texture;
			}
			template<typename OutStream>
			friend OutStream& operator<< (OutStream& out, const ExtractedTexture& textureData)
			{
				if (out.filename.find(".basis") == out.filename.length() - 6)
				{
					out.write((const char*)textureData.texture.compressedData.data(),textureData.texture.compressedData.size());
					return out;
				}
				else if (out.filename.find(".ktx") == out.filename.length() - 4)
				{
					out.write((const char*)textureData.texture.compressedData.data(),textureData.texture.compressedData.size());
					return out;
				}
				else if (out.filename.find(".ktx2") == out.filename.length() - 5)
				{
					out.write((const char*)textureData.texture.compressedData.data(),textureData.texture.compressedData.size());
					return out;
				}
				out.writeChunk(textureData.lastModified);
				out << textureData.texture;
				return out;
			}
			template<typename InStream>
			friend InStream& operator>> (InStream& in, ExtractedTexture& textureData)
			{
				if (in.filename.rfind(".basis") == in.filename.length() - 6||in.filename.rfind(".ktx2") == in.filename.length() - 5||in.filename.rfind(".ktx") == in.filename.length() - 4)
				{
					LoadAsBasisFile(textureData, in.readData(), in.filename);
					return in;
				}
				in.readChunk(textureData.lastModified);
				in >> textureData.texture;
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