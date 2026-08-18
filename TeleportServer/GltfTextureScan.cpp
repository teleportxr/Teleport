// (C) Copyright 2018-2026 Simul Software Ltd
#include "GltfTextureScan.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>

using nlohmann::json;

namespace
{
	constexpr uint32_t GLB_MAGIC	  = 0x46546C67; // "glTF"
	constexpr uint32_t GLB_CHUNK_JSON = 0x4E4F534A; // "JSON"

	//! The JSON document of a glTF asset, whether it arrived as a .glb container or as text.
	//! Only the container is walked here - the BIN chunk and any image data are skipped, which
	//! is what keeps this cheap on a large avatar.
	bool ParseGltfJson(const uint8_t *data, size_t size, json &doc)
	{
		if (!data || size < 4)
		{
			return false;
		}
		uint32_t magic = 0;
		std::memcpy(&magic, data, 4);
		if (magic == GLB_MAGIC)
		{
			if (size < 20)
			{
				return false;
			}
			uint32_t declaredLength = 0;
			std::memcpy(&declaredLength, data + 8, 4);
			const size_t end = (declaredLength >= 12 && declaredLength <= size) ? declaredLength : size;
			size_t offset	 = 12;
			while (offset + 8 <= end)
			{
				uint32_t chunkLength = 0, chunkType = 0;
				std::memcpy(&chunkLength, data + offset, 4);
				std::memcpy(&chunkType, data + offset + 4, 4);
				const size_t chunkStart = offset + 8;
				if (chunkStart + chunkLength > end)
				{
					return false;
				}
				if (chunkType == GLB_CHUNK_JSON)
				{
					doc = json::parse(data + chunkStart, data + chunkStart + chunkLength, nullptr, false);
					return !doc.is_discarded() && doc.is_object();
				}
				offset = chunkStart + chunkLength;
			}
			return false;
		}
		// Plain JSON glTF.
		doc = json::parse(data, data + size, nullptr, false);
		return !doc.is_discarded() && doc.is_object();
	}
}

std::vector<std::string> teleport::server::GetExternalImageUris(const uint8_t *data, size_t size)
{
	std::vector<std::string> uris;
	json					 doc;
	if (!ParseGltfJson(data, size, doc))
	{
		return uris;
	}
	auto images = doc.find("images");
	if (images == doc.end() || !images->is_array())
	{
		return uris;
	}
	for (const auto &image : *images)
	{
		if (!image.is_object())
		{
			continue;
		}
		auto uri = image.find("uri");
		if (uri == image.end() || !uri->is_string())
		{
			// An embedded image: it travels with the asset and is nobody's dependency.
			continue;
		}
		const std::string &u = uri->get_ref<const std::string &>();
		if (u.empty() || u.rfind("data:", 0) == 0)
		{
			continue;
		}
		if (std::find(uris.begin(), uris.end(), u) == uris.end())
		{
			uris.push_back(u);
		}
	}
	return uris;
}

std::vector<std::string> teleport::server::GetExternalImageUrisFromFile(const std::string &filename)
{
	std::ifstream in(filename, std::ios::binary);
	if (!in)
	{
		return {};
	}
	std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	return GetExternalImageUris(data.data(), data.size());
}

std::string teleport::server::ResolveAssetRelativePath(const std::string &assetPath, const std::string &uri)
{
	if (uri.empty() || uri.find("://") != std::string::npos)
	{
		// Somewhere else entirely: not one of this server's assets, and not ours to stream.
		return std::string();
	}

	std::string combined;
	if (uri[0] == '/')
	{
		// Already relative to the resource root, once the leading slash is dropped.
		combined = uri.substr(1);
	}
	else
	{
		std::string directory  = assetPath;
		const size_t lastSlash = directory.rfind('/');
		directory			   = (lastSlash == std::string::npos) ? std::string() : directory.substr(0, lastSlash + 1);
		combined			   = directory + uri;
	}

	// Resolve "." and ".." against the segments, refusing anything that climbs above the root:
	// a uri must not be able to name a file outside the served assets.
	std::vector<std::string> segments;
	size_t					 start = 0;
	while (start <= combined.size())
	{
		const size_t	  slash	  = combined.find('/', start);
		const bool		  last	  = (slash == std::string::npos);
		const std::string segment = combined.substr(start, last ? std::string::npos : slash - start);
		if (segment == "..")
		{
			if (segments.empty())
			{
				return std::string();
			}
			segments.pop_back();
		}
		else if (segment != "." && !segment.empty())
		{
			segments.push_back(segment);
		}
		if (last)
		{
			break;
		}
		start = slash + 1;
	}

	std::string path;
	for (const std::string &segment : segments)
	{
		if (!path.empty())
		{
			path += "/";
		}
		path += segment;
	}
	return path;
}
