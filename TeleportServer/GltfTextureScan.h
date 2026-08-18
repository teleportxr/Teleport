// (C) Copyright 2018-2026 Simul Software Ltd
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace teleport
{
	namespace server
	{
		//! The uris of images a glTF references as external files, in the order they appear in
		//! its `images` array. Duplicates are removed.
		//!
		//! A .glb/.vrm/.gltf may either embed its images - in a bufferView, or inline as a data
		//! uri - or reference them as separate files beside it. The second form makes each of
		//! those files a resource in its own right, which a client streaming the mesh must also
		//! be streamed, and which nothing else in the scene declares. This is how the server
		//! finds them.
		//!
		//! An asset that embeds all its images yields an empty list, which is the common case.
		//! So does anything unparseable: this reads a list of dependencies out of a file, and a
		//! file it cannot read simply has none to declare.
		//!
		//! Uris are returned exactly as authored, relative to the asset itself per the glTF
		//! spec. Use ResolveAssetRelativePath to turn one into an asset path.
		//!
		//! Only the container and its JSON chunk are read - no geometry, no image data - so this
		//! is cheap enough to run when an asset is registered.
		std::vector<std::string> GetExternalImageUris(const uint8_t *data, size_t size);
		//! As above, reading the file at `filename`. An unreadable file yields an empty list.
		std::vector<std::string> GetExternalImageUrisFromFile(const std::string &filename);

		//! Resolve a uri written inside an asset against that asset's own path, giving a path in
		//! the same space: "tex.png" inside "props/chair.glb" is "props/tex.png".
		//!
		//! A leading "/" means the server's resource root, and is returned without it - resource
		//! paths here are relative. "." and ".." segments are resolved. A uri with a scheme
		//! ("://") points outside this server's assets entirely and comes back empty, as does one
		//! that climbs above the root.
		std::string ResolveAssetRelativePath(const std::string &assetPath, const std::string &uri);
	}
}
