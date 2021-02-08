/*
 * We save and load the IDs from disk, but then we need to re-assign them.
 * Otherwise we will end up with a lot of unused ID slots, and need to manually change the next unused ID to a safe value.
 */
#pragma once

#include <ctime>

#include "comdef.h"

#include "libavstream/geometry/material_interface.hpp"
#include "libavstream/geometry/mesh_interface.hpp"

namespace SCServer
{
	struct ExtractedMesh
	{
		_bstr_t guid;
		std::time_t lastModified;
		avs::Mesh mesh;

		friend std::wostream& operator<< (std::wostream& out, const ExtractedMesh& meshData)
		{
			return out << std::wstring{meshData.guid, SysStringLen(meshData.guid)}
				<< " " << meshData.lastModified
				<< std::endl << meshData.mesh << std::endl;
		}

		friend std::wistream& operator>> (std::wistream& in, ExtractedMesh& meshData)
		{
			std::wstring guidAsString;
			in >> guidAsString;
			meshData.guid = _bstr_t(guidAsString.data());

			return in >> meshData.lastModified >> meshData.mesh;
		}
	};

	struct ExtractedMaterial
	{
		_bstr_t guid;
		std::time_t lastModified;
		avs::Material material;

		friend std::wostream& operator<< (std::wostream& out, const ExtractedMaterial& materialData)
		{
			return out << std::wstring{materialData.guid, SysStringLen(materialData.guid)}
				<< " " << materialData.lastModified
				<< std::endl << materialData.material << std::endl;
		}

		friend std::wistream& operator>> (std::wistream& in, ExtractedMaterial& materialData)
		{
			std::wstring guidAsString;
			in >> guidAsString;
			materialData.guid = _bstr_t(guidAsString.data());

			return in >> materialData.lastModified >> materialData.material;
		}
	};

	struct ExtractedTexture
	{
		_bstr_t guid;
		std::time_t lastModified;
		avs::Texture texture;

		friend std::wostream& operator<< (std::wostream& out, const ExtractedTexture& textureData)
		{
			return out << std::wstring{textureData.guid, SysStringLen(textureData.guid)}
				<< " " << textureData.lastModified
				<< std::endl << textureData.texture << std::endl;
		}

		friend std::wistream& operator>> (std::wistream& in, ExtractedTexture& textureData)
		{
			std::wstring guidAsString;
			in >> guidAsString;
			textureData.guid = _bstr_t(guidAsString.data());

			return in >> textureData.lastModified >> textureData.texture;
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