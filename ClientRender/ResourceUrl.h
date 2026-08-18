// (C) Copyright 2018-2026 Simul Software Ltd
#pragma once
#include <string>

namespace teleport
{
	namespace clientrender
	{
		//! Resolve a relative url against the url of the document that referenced it, as the glTF
		//! spec requires for a uri inside a .gltf/.glb: an asset that references its textures as
		//! external files writes them relative to itself, so "tex.png" inside
		//! https://host/props/chair.glb is https://host/props/tex.png.
		//!
		//! `relative` containing "://" is already absolute and is returned unchanged. A leading
		//! "/" is relative to `base`'s scheme and authority; anything else is relative to `base`'s
		//! directory, with "." and ".." segments resolved as RFC 3986 requires. Any query or
		//! fragment on `base` is discarded first.
		//!
		//! Percent encoding is left exactly as authored - it is part of the url, and the fetch
		//! wants it back in that form.
		//!
		//! A `base` with no scheme cannot be resolved against, and `relative` is returned as-is
		//! for the caller's own url-root handling to complete.
		std::string ResolveUrl(const std::string &base, const std::string &relative);

		//! True if this uri carries its own bytes ("data:image/png;base64,...") rather than naming
		//! a file to fetch. Such a uri is neither resolved nor matched against texture resources:
		//! there is no file, and nothing else can reference the same one.
		bool IsDataUri(const std::string &uri);
	}
}
