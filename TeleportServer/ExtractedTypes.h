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
#include "Font.h"

namespace teleport
{
	namespace server
	{
		// Here we implement special cases of std::wofstream and wifstream that are able to convert guid_to_uid to guids and vice versa.
		inline std::string WStringToString(const std::wstring& text)
		{
			size_t origsize = text.length() + 1;
			const size_t newsize = origsize;
			char* cstring = new char[newsize];

#ifdef _MSC_VER
			size_t convertedChars = 0;
			wcstombs_s(&convertedChars, cstring, (size_t)origsize, text.c_str(), (size_t)newsize);
#else
			wcstombs(cstring, text.c_str(), (size_t)newsize);
#endif
			std::string str;
			str = std::string(cstring);
			delete[] cstring;
			return str;
		}
		inline std::wstring StringToWString(const std::string& text)
		{
			size_t origsize = strlen(text.c_str()) + 1;
			const size_t newsize = origsize;
			wchar_t* wcstring = new wchar_t[newsize + 2];
#ifdef _MSC_VER
			size_t convertedChars = 0;
			mbstowcs_s(&convertedChars, wcstring, origsize, text.c_str(), _TRUNCATE);
#else
			mbstowcs(wcstring, text.c_str(), origsize);
#endif
			std::wstring str(wcstring);
			delete[] wcstring;
			return str;
		}
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
			bool Verify(const ExtractedMesh& t) const
			{
				return true;
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
				std::wstring pathAsString = StringToWString(meshData.path);
				std::replace(pathAsString.begin(), pathAsString.end(), ' ', '%');
				out << StringToWString(meshData.guid)
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
				meshData.guid = WStringToString(guidAsString.data());

				std::wstring pathAsString;
				in >> pathAsString;
				std::replace(pathAsString.begin(), pathAsString.end(), '%', ' ');
				meshData.path = WStringToString(pathAsString.data());

				in >> meshData.lastModified >> meshData.mesh >> meshData.compressedMesh;
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
				std::wstring pathAsString = StringToWString(materialData.path);
				std::replace(pathAsString.begin(), pathAsString.end(), ' ', '%');
				out << StringToWString(materialData.guid)
					<< " " << pathAsString
					<< " " << materialData.lastModified
					<< "\n";
				out << materialData.material;
				out << "\n";
				return out;
			}

			template<typename InStream>
			friend InStream& operator>> (InStream& in, ExtractedMaterial& materialData)
			{
				std::wstring guidAsString;
				in >> guidAsString;
				materialData.guid = WStringToString(guidAsString);

				std::wstring pathAsString;
				in >> pathAsString;
				std::replace(pathAsString.begin(), pathAsString.end(), '%', ' ');
				materialData.path = WStringToString(pathAsString);

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
			std::string guid;
			//! The path to the asset, which is both the relative path from the cache directory, and the URI
			//! relative to the server.
			std::string path;
			std::time_t lastModified;
			avs::Texture texture;
			bool Verify(const ExtractedTexture& t) const
			{
				return true;
			}
			template<typename OutStream>
			friend OutStream& operator<< (OutStream& out, const ExtractedTexture& textureData)
			{
				std::wstring pathAsString = StringToWString(textureData.path);
				std::replace(pathAsString.begin(), pathAsString.end(), ' ', '%');
				out << StringToWString(textureData.guid);
				out << " " << pathAsString;
				out << " " << textureData.lastModified;
				out << "\n";
				out << textureData.texture;
				return out;
			}

			template<typename InStream>
			friend InStream& operator>> (InStream& in, ExtractedTexture& textureData)
			{
				std::wstring wguid;
				in >> wguid;
				textureData.guid = WStringToString(wguid);
				std::wstring pathAsString;
				in >> pathAsString;
				std::replace(pathAsString.begin(), pathAsString.end(), '%', ' ');
				textureData.path = WStringToString(pathAsString);

				in >> textureData.lastModified;
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