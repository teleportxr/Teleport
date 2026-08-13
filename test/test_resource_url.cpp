// Url resolution for assets that reference their textures as external files.
//
// A .glb/.vrm may reference its images by uri rather than embedding them - which is what
// GltfConverter --split-objects produces, and what a glTF exporter writing "separate" files
// produces. Those uris are relative to the asset itself, so the client has to resolve each one
// against the url it fetched the asset from before it can either match it to a texture resource
// the server has already delivered, or fetch it.
//
// Getting this wrong is quiet rather than loud: a mis-resolved url 404s and the model renders
// untextured, so the rules are pinned here.

#include <catch2/catch_test_macros.hpp>

#include "ClientRender/ResourceUrl.h"

using teleport::clientrender::ResolveUrl;

static const std::string asset = "https://host.example/props/chair.glb";

TEST_CASE("A relative uri resolves against the asset's own directory", "[url]")
{
	REQUIRE(ResolveUrl(asset, "tex.png") == "https://host.example/props/tex.png");
	REQUIRE(ResolveUrl(asset, "textures/base.ktx2") == "https://host.example/props/textures/base.ktx2");
	REQUIRE(ResolveUrl("https://host.example/chair.glb", "tex.png") == "https://host.example/tex.png");
}

TEST_CASE("A root-relative uri resolves against the scheme and authority only", "[url]")
{
	REQUIRE(ResolveUrl(asset, "/shared/atlas.png") == "https://host.example/shared/atlas.png");
	REQUIRE(ResolveUrl("https://host.example:8443/a/b/c.glb", "/t.png") == "https://host.example:8443/t.png");
}

TEST_CASE("An absolute uri is returned untouched", "[url]")
{
	REQUIRE(ResolveUrl(asset, "https://cdn.example/t.png") == "https://cdn.example/t.png");
	REQUIRE(ResolveUrl(asset, "http://cdn.example/t.png") == "http://cdn.example/t.png");
}

TEST_CASE("Dot segments are resolved", "[url]")
{
	REQUIRE(ResolveUrl(asset, "./tex.png") == "https://host.example/props/tex.png");
	REQUIRE(ResolveUrl(asset, "../textures/tex.png") == "https://host.example/textures/tex.png");
	REQUIRE(ResolveUrl("https://host.example/a/b/c/model.glb", "../../t.png") == "https://host.example/a/t.png");
	// Climbing past the root cannot escape it.
	REQUIRE(ResolveUrl(asset, "../../../../t.png") == "https://host.example/t.png");
}

TEST_CASE("The base's query and fragment do not take part", "[url]")
{
	REQUIRE(ResolveUrl("https://host.example/props/chair.glb?v=3", "tex.png") == "https://host.example/props/tex.png");
	REQUIRE(ResolveUrl("https://host.example/props/chair.glb#scene", "tex.png") == "https://host.example/props/tex.png");
}

TEST_CASE("Percent encoding is left exactly as authored", "[url]")
{
	// The url is what gets fetched; decoding it here would produce a different request.
	REQUIRE(ResolveUrl(asset, "my%20texture.png") == "https://host.example/props/my%20texture.png");
}

TEST_CASE("Degenerate inputs are handled without inventing a url", "[url]")
{
	REQUIRE(ResolveUrl(asset, "") == "");
	// No scheme on the base: nothing to resolve against, so the caller's own url-root handling
	// gets the uri back unchanged rather than a fabricated absolute url.
	REQUIRE(ResolveUrl("chair.glb", "tex.png") == "tex.png");
	REQUIRE(ResolveUrl("", "tex.png") == "tex.png");
	// An authority with no path at all.
	REQUIRE(ResolveUrl("https://host.example", "tex.png") == "https://host.example/tex.png");
}
