/*
 * We save and load the IDs from disk, but then we need to re-assign them.
 * Otherwise we will end up with a lot of unused ID slots, and need to manually change the next unused ID to a safe value.
 */
#pragma once

#include <ctime>
#include <algorithm>

#ifdef _MSC_VER
#include "comdef.h"
#endif

#include "libavstream/geometry/material_interface.hpp"
#include "libavstream/geometry/mesh_interface.hpp"
#include "Font.h"
#include "StringFunctions.h"

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
			std::string getName() const
			{
				return mesh.name;
			}
			std::string guid;
			//! The path to the asset, which is both the relative path from the cache directory, and the URI
			//! relative to the server.
			std::string path;
			std::time_t lastModified;
			avs::Mesh mesh;
			avs::CompressedMesh compressedMesh;
			bool operator==(const ExtractedMesh& t) const
			{
				if (guid != t.guid)
					return false;
				if (path != t.path)
					return false;
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
				std::string pathAsString = meshData.path;
				std::replace(pathAsString.begin(), pathAsString.end(), ' ', '%');
				out << meshData.guid
					<< pathAsString;
				out.writeChunk(meshData.lastModified);
				out << meshData.mesh
					<< meshData.compressedMesh;
				return out;
			}

			template<class InStream>
			friend InStream& operator>> (InStream& in, ExtractedMesh& meshData)
			{
				in >> meshData.guid;
				in >> meshData.path;
				std::replace(meshData.path.begin(), meshData.path.end(), '%', ' ');
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
			std::string guid;
			//! The path to the asset, which is both the relative path from the cache directory, and the URI
			//! relative to the server.
			std::string path;
			std::time_t lastModified;
			avs::Material material;

			bool Verify(const ExtractedMaterial& t) const
			{
				return material.Verify(t.material);
			}
			template<typename OutStream>
			friend OutStream& operator<< (OutStream& out, const ExtractedMaterial& materialData)
			{
				std::string pathAsString = materialData.path;
				std::replace(pathAsString.begin(), pathAsString.end(), ' ', '%');
				out << materialData.guid
					<< pathAsString;
				out.writeChunk(materialData.lastModified);
				out << materialData.material;
				return out;
			}

			template<typename InStream>
			friend InStream& operator>> (InStream& in, ExtractedMaterial& materialData)
			{
				in >> materialData.guid;
				in >> materialData.path;
				std::replace(materialData.path.begin(), materialData.path.end(), '%', ' ');
				in.readChunk(materialData.lastModified);
				in >> materialData.material;
				return in;
			}
		};

		struct ExtractedTexture
		{
			static const char* fileExtension()
			{
				return ".texture";
			}
			std::string getName() const
			{
				return texture.name;
			}
			std::string guid;
			//! The path to the asset, which is both the relative path from the cache directory, and the URI
			//! relative to the server.
			std::string path;
			std::time_t lastModified;
			avs::Texture texture;
			bool Verify(const ExtractedTexture& t) const
			{
				return texture==t.texture;
			}
			template<typename OutStream>
			friend OutStream& operator<< (OutStream& out, const ExtractedTexture& textureData)
			{
				out << textureData.guid;
				std::string pathAsString = textureData.path;
				std::replace(pathAsString.begin(), pathAsString.end(), ' ', '%');
				out << pathAsString;
				out.writeChunk(textureData.lastModified);
				out << textureData.texture;
				return out;
			}

			template<typename InStream>
			friend InStream& operator>> (InStream& in, ExtractedTexture& textureData)
			{
				in >> textureData.guid;
				std::string pathAsString;
				in >> pathAsString;
				std::replace(pathAsString.begin(), pathAsString.end(), '%', ' ');
				textureData.path = pathAsString;
				in.readChunk(textureData.lastModified);
				in >> textureData.texture;
				return in;
			}
		};

		//! Each font size represented has a FontMap.
		struct ExtractedFontAtlas
		{
			core::FontAtlas fontAtlas;

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
		};
		//Resource that has been loaded from disk.
		struct LoadedResource
		{
			avs::uid id;		// The id of the resource in this session.
			const char* guid;	// 
			const char* path;	// Uniquely identifying string that the engine uses to identify assets.
			const char* name;	// Name of the asset to tell it apart from assets with the GUID; i.e. they come from the same source file.
			std::time_t lastModified;

			LoadedResource() = default;
			LoadedResource(avs::uid uid, const char* g, const char* pth, const char* name, std::time_t lastModified)
				:id(uid), guid(g), path(pth), name(name), lastModified(lastModified)
			{}
		};
	}
}