/*
 * We save and load the IDs from disk, but then we need to re-assign them.
 * Otherwise we will end up with a lot of unused ID slots, and need to manually change the next unused ID to a safe value.
 */
#pragma once

#include <ctime>

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
		std::time_t lastModified;
		avs::Mesh mesh;
		avs::CompressedMesh compressedMesh;
		
		template<class OutStream>
		friend OutStream& operator<< (OutStream& out, const ExtractedMesh& meshData)
		{
			out << std::wstring{meshData.guid, SysStringLen(meshData.guid)}
				<< " " << meshData.lastModified
				<< std::endl << meshData.mesh
				<< std::endl << meshData.compressedMesh << std::endl;
			return out;
		}

		template<class InStream>
		friend InStream& operator>> (InStream& in, ExtractedMesh& meshData)
		{
			std::wstring guidAsString;
			in >> guidAsString;
			meshData.guid = _bstr_t(guidAsString.data());

			in >> meshData.lastModified >> meshData.mesh >> meshData.compressedMesh;
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
		std::time_t lastModified;
		avs::Material material;

		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const ExtractedMaterial& materialData)
		{
			return out << std::wstring{materialData.guid, SysStringLen(materialData.guid)}
				<< " " << materialData.lastModified
				<< std::endl << materialData.material << std::endl;
		}
		
		template<typename InStream>
		friend InStream& operator>> (InStream& in, ExtractedMaterial& materialData)
		{
			std::wstring guidAsString;
			in >> guidAsString;
			materialData.guid = _bstr_t(guidAsString.data());

			 in >> materialData.lastModified >> materialData.material;
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
		std::time_t lastModified;
		avs::Texture texture;
		float valueScale=1.0f;
		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const ExtractedTexture& textureData)
		{
			return out << std::wstring{textureData.guid, SysStringLen(textureData.guid)}
				<< " " << textureData.lastModified
				<< std::endl << textureData.texture << " " << textureData.valueScale << std::endl;
		}
		
		template<typename InStream>
		friend InStream& operator>> (InStream& in, ExtractedTexture& textureData)
		{
			std::wstring guidAsString;
			in >> guidAsString;
			textureData.guid = _bstr_t(guidAsString.data());

			in >> textureData.lastModified >> textureData.texture >> textureData.valueScale;
			return in;
		}
	};

	//Resource that has been loaded off disk, and needs a new ID.
	struct LoadedResource
	{
		avs::uid oldID; //The id the resource was using previously; kept so the links between resources can be re-linked.
		BSTR guid; //Uniquely identifying string that the engine uses to identify assets.
		BSTR name; //Name of the asset to tell it apart from assets with the GUID; i.e. they come from the same source file.
		std::time_t lastModified;

		LoadedResource() = default;
		LoadedResource(avs::uid oldID, _bstr_t guid, BSTR name, std::time_t lastModified)
			:oldID(oldID), guid(guid), name(name), lastModified(lastModified)
		{}
	};

	//Resource that the server found to still be in-use, and has assigned a new ID to.
	struct ReaffirmedResource
	{
		avs::uid oldID;
		avs::uid newID;
	};
}