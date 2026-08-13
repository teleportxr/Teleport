// A .glb/.vrm may reference its textures as external files rather than embedding them - which
// is what GltfConverter --split-objects produces. Those files are then dependencies of the
// mesh: a client streaming it has nothing to resolve the asset's own image uris against unless
// the server streams them too, and no material in the store names them.
//
// This is how the server finds them. It reads the container and its JSON chunk only, so it can
// run when an asset is registered rather than per client.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "TeleportServer/GltfTextureScan.h"

using teleport::server::GetExternalImageUris;
using teleport::server::GetExternalImageUrisFromFile;
using teleport::server::ResolveAssetRelativePath;

//! Wrap a JSON document in a GLB container, as a .glb/.vrm is.
static std::vector<uint8_t> MakeGlb(const std::string &jsonText)
{
	std::string json = jsonText;
	while (json.size() % 4 != 0)
	{
		json.push_back(' ');
	}
	std::vector<uint8_t> out(12 + 8 + json.size());
	const uint32_t		 magic = 0x46546C67, version = 2, total = (uint32_t)out.size();
	const uint32_t		 chunkLength = (uint32_t)json.size(), chunkType = 0x4E4F534A;
	std::memcpy(&out[0], &magic, 4);
	std::memcpy(&out[4], &version, 4);
	std::memcpy(&out[8], &total, 4);
	std::memcpy(&out[12], &chunkLength, 4);
	std::memcpy(&out[16], &chunkType, 4);
	std::memcpy(&out[20], json.data(), json.size());
	return out;
}

static std::vector<std::string> ScanText(const std::string &jsonText)
{
	return GetExternalImageUris((const uint8_t *)jsonText.data(), jsonText.size());
}

static std::vector<std::string> ScanGlb(const std::string &jsonText)
{
	const std::vector<uint8_t> glb = MakeGlb(jsonText);
	return GetExternalImageUris(glb.data(), glb.size());
}

TEST_CASE("External image uris are found in both containers", "[gltf]")
{
	const std::string doc = R"({"asset":{"version":"2.0"},
		"images":[{"uri":"chair_base.png"},{"uri":"textures/normal.ktx2"}]})";

	const std::vector<std::string> expected = {"chair_base.png", "textures/normal.ktx2"};
	REQUIRE(ScanGlb(doc) == expected);
	REQUIRE(ScanText(doc) == expected);
}

TEST_CASE("Embedded images are nobody's dependency", "[gltf]")
{
	// A bufferView image travels inside the asset; a data uri is the asset. Neither is a
	// separate resource, so neither is streamed.
	REQUIRE(ScanGlb(R"({"images":[{"bufferView":0,"mimeType":"image/png"}]})").empty());
	REQUIRE(ScanGlb(R"({"images":[{"uri":"data:image/png;base64,AAAA"}]})").empty());
	REQUIRE(ScanGlb(R"({"asset":{"version":"2.0"}})").empty());
}

TEST_CASE("Duplicate uris are reported once", "[gltf]")
{
	const std::vector<std::string> uris = ScanGlb(R"({"images":[{"uri":"t.png"},{"uri":"t.png"}]})");
	REQUIRE(uris.size() == 1);
	REQUIRE(uris[0] == "t.png");
}

TEST_CASE("A file that cannot be read declares no dependencies", "[gltf]")
{
	// Garbage in, empty out - never a crash and never a guess: this runs over whatever files a
	// server has been pointed at.
	REQUIRE(GetExternalImageUris(nullptr, 0).empty());
	const std::string notGltf = "this is not a glTF file at all";
	REQUIRE(ScanText(notGltf).empty());
	const std::string truncated = "glTF";
	REQUIRE(GetExternalImageUris((const uint8_t *)truncated.data(), truncated.size()).empty());
	// A GLB header promising more than is there.
	std::vector<uint8_t> shortGlb = MakeGlb(R"({"images":[{"uri":"t.png"}]})");
	shortGlb.resize(24);
	REQUIRE(GetExternalImageUris(shortGlb.data(), shortGlb.size()).empty());
	REQUIRE(GetExternalImageUrisFromFile("/no/such/file.glb").empty());
	// Malformed JSON inside a well-formed container.
	REQUIRE(ScanGlb("{\"images\":[{\"uri\":").empty());
}

TEST_CASE("A uri resolves against the asset's own path", "[gltf]")
{
	REQUIRE(ResolveAssetRelativePath("props/chair.glb", "tex.png") == "props/tex.png");
	REQUIRE(ResolveAssetRelativePath("props/chair.glb", "textures/tex.png") == "props/textures/tex.png");
	REQUIRE(ResolveAssetRelativePath("chair.glb", "tex.png") == "tex.png");
	// A leading slash is the resource root, which these paths are already relative to.
	REQUIRE(ResolveAssetRelativePath("props/chair.glb", "/shared/atlas.png") == "shared/atlas.png");
	REQUIRE(ResolveAssetRelativePath("props/chair.glb", "./tex.png") == "props/tex.png");
	REQUIRE(ResolveAssetRelativePath("props/a/b/chair.glb", "../tex.png") == "props/a/tex.png");
}

TEST_CASE("A uri that leaves the server's assets is not ours to stream", "[gltf]")
{
	// An absolute url is somebody else's file; the client fetches it directly.
	REQUIRE(ResolveAssetRelativePath("props/chair.glb", "https://cdn.example/t.png").empty());
	// And nothing may climb above the root: a uri must not be able to name a file outside the
	// assets we serve.
	REQUIRE(ResolveAssetRelativePath("props/chair.glb", "../../../etc/passwd").empty());
	REQUIRE(ResolveAssetRelativePath("chair.glb", "../secret.png").empty());
	REQUIRE(ResolveAssetRelativePath("props/chair.glb", "").empty());
}
