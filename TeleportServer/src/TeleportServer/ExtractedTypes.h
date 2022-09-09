/*
 * We save and load the IDs from disk, but then we need to re-assign them.
 * Otherwise we will end up with a lot of unused ID slots, and need to manually change the next unused ID to a safe value.
 */
#pragma once

#include <ctime>
#include <algorithm>

#include "comdef.h"

#include "libavstream/geometry/material_interface.hpp"
#include "libavstream/geometry/mesh_interface.hpp"

namespace teleport
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
		_bstr_t guid;
		//! The path to the asset, which is both the relative path from the cache directory, and the URI
		//! relative to the server.
		_bstr_t path;
		std::time_t lastModified;
		avs::Mesh mesh;
		avs::CompressedMesh compressedMesh;
		bool Verify(const ExtractedMesh &t) const
		{
			return true;
		}
		void GetAccessorRange(uint64_t &lowest,uint64_t &highest) const
		{
			lowest=0xFFFFFFFFFFFFFFFF;
			highest=0;
			mesh.GetAccessorRange(lowest,highest);
			compressedMesh.GetAccessorRange(lowest,highest);
		}
		void ResetAccessorRange()
		{
			uint64_t lowest,highest;
			GetAccessorRange(lowest,highest);
			mesh.ResetAccessors(lowest);
			compressedMesh.ResetAccessors(lowest);
		}
		
		template<class OutStream>
		friend OutStream& operator<< (OutStream& out, const ExtractedMesh& meshData)
		{
			std::wstring pathAsString={meshData.path, SysStringLen(meshData.path)};
			std::replace(pathAsString.begin(),pathAsString.end(),' ','%');
			out << std::wstring{meshData.guid, SysStringLen(meshData.guid)}
				<< " " << pathAsString
				<< " " << meshData.lastModified
				<< "\n" << meshData.mesh
				<< "\n" << meshData.compressedMesh << "\n";
			return out;
		}

		template<class InStream>
		friend InStream& operator>> (InStream& in, ExtractedMesh& meshData)
		{
			std::wstring guidAsString;
			in >> guidAsString;
			meshData.guid = _bstr_t(guidAsString.data());

			std::wstring pathAsString;
			in >> pathAsString;
			std::replace(pathAsString.begin(),pathAsString.end(),'%',' ');
			meshData.path = _bstr_t(pathAsString.data());

			in >> meshData.lastModified >> meshData.mesh >> meshData.compressedMesh;
			// having loaded, now rescale the uid's:
			meshData.ResetAccessorRange();
			return in;
		}
	};

	struct ExtractedMaterial
	{
		static const char *fileExtension()
		{
			return ".material";
		}
		std::string getName() const
		{
			return material.name;
		}
		_bstr_t guid;
		//! The path to the asset, which is both the relative path from the cache directory, and the URI
		//! relative to the server.
		_bstr_t path;
		std::time_t lastModified;
		avs::Material material;
		
		bool Verify(const ExtractedMaterial &t) const
		{
			return material.Verify(t.material);
		}
		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const ExtractedMaterial& materialData)
		{
			std::wstring pathAsString={materialData.path, SysStringLen(materialData.path)};
			std::replace(pathAsString.begin(),pathAsString.end(),' ','%');
			out << std::wstring{materialData.guid, SysStringLen(materialData.guid)}
				<< " " << pathAsString
				<< " " << materialData.lastModified
				<< "\n";
			out<< materialData.material;
			out<< "\n";
			return out;
		}
		
		template<typename InStream>
		friend InStream& operator>> (InStream& in, ExtractedMaterial& materialData)
		{
			std::wstring guidAsString;
			in >> guidAsString;
			materialData.guid = _bstr_t(guidAsString.data());

			std::wstring pathAsString;
			in >> pathAsString;
			std::replace(pathAsString.begin(),pathAsString.end(),'%',' ');
			materialData.path = _bstr_t(pathAsString.data());

			 in >> materialData.lastModified;
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
		_bstr_t guid;
		//! The path to the asset, which is both the relative path from the cache directory, and the URI
		//! relative to the server.
		_bstr_t path;
		std::time_t lastModified;
		avs::Texture texture;
		float valueScale=1.0f;
		bool Verify(const ExtractedTexture &t) const
		{
			return true;
		}
		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const ExtractedTexture& textureData)
		{
			std::wstring pathAsString={textureData.path, SysStringLen(textureData.path)};
			std::replace(pathAsString.begin(),pathAsString.end(),' ','%');

			out << std::wstring{textureData.guid, SysStringLen(textureData.guid)};
			out << " " << pathAsString;
			out << " " << textureData.lastModified;
			out << "\n";
			out << textureData.texture;
			out << " " << textureData.valueScale << "\n";
			return out;
		}
		
		template<typename InStream>
		friend InStream& operator>> (InStream& in, ExtractedTexture& textureData)
		{
			std::wstring guidAsString;
			in >> guidAsString;
			textureData.guid = _bstr_t(guidAsString.data());
			
			std::wstring pathAsString;
			in >> pathAsString;
			std::replace(pathAsString.begin(),pathAsString.end(),'%',' ');
			textureData.path = _bstr_t(pathAsString.data());

			in >> textureData.lastModified;
			in >> textureData.texture;
			in >> textureData.valueScale;
			return in;
		}
	};

	//Resource that has been loaded off disk, and needs a new ID.
	struct LoadedResource
	{
		avs::uid id;	// The id of the resource in this session.
		BSTR guid;		// Uniquely identifying string that the engine uses to identify assets.
		BSTR path;		// Uniquely identifying string that the engine uses to identify assets.
		BSTR name;		// Name of the asset to tell it apart from assets with the GUID; i.e. they come from the same source file.
		std::time_t lastModified;

		LoadedResource() = default;
		LoadedResource(avs::uid uid, _bstr_t gud, _bstr_t pth, BSTR name, std::time_t lastModified)
			:id(uid), guid(gud), path(pth), name(name), lastModified(lastModified)
		{}
	};

}