// The manifest evaluation cache: LRU by entry count and byte total, with the
// manifest's own expiresAt as the hard lifetime bound.

#include <catch2/catch_test_macros.hpp>

#include "TeleportServer/ManifestCache.h"

using namespace teleport::server;

namespace
{
	constexpr int64_t kNow = 1780000000;

	AvatarManifestResolution Resolution(const char *avatarUrl, int64_t expiresAtUnix)
	{
		AvatarManifestResolution r;
		r.ok = true;
		r.avatarUrl = avatarUrl;
		r.expiresAtUnix = expiresAtUnix;
		return r;
	}
}

TEST_CASE("a stored evaluation is returned while it is still valid", "[manifest][cache]")
{
	ManifestCache cache;
	cache.Set("https://m.example/a", Resolution("https://a.example/a.glb", kNow + 3600), 100);

	AvatarManifestResolution out;
	REQUIRE(cache.Get("https://m.example/a", kNow, out));
	REQUIRE(out.avatarUrl == "https://a.example/a.glb");
}

TEST_CASE("a miss is a miss", "[manifest][cache]")
{
	ManifestCache cache;
	AvatarManifestResolution out;
	REQUIRE_FALSE(cache.Get("https://m.example/never-seen", kNow, out));
}

TEST_CASE("an entry is not served past the manifest expiresAt", "[manifest][cache]")
{
	// The UMID resolver serves Cache-Control: max-age=60, but its contract
	// says consumers MUST enforce the manifest TTL regardless of HTTP
	// caching. The manifest's own expiry is therefore the hard bound.
	ManifestCache cache;
	cache.Set("https://m.example/a", Resolution("https://a.example/a.glb", kNow + 10), 100);

	AvatarManifestResolution out;
	REQUIRE(cache.Get("https://m.example/a", kNow, out));
	REQUIRE_FALSE(cache.Get("https://m.example/a", kNow + 11, out));
	// And the expired entry is dropped rather than lingering, so a caller
	// that re-fetches gets an honest fresh verdict.
	REQUIRE(cache.Size() == 0);
}

TEST_CASE("an entry expiring exactly now is not served", "[manifest][cache]")
{
	ManifestCache cache;
	cache.Set("https://m.example/a", Resolution("https://a.example/a.glb", kNow), 100);
	AvatarManifestResolution out;
	REQUIRE_FALSE(cache.Get("https://m.example/a", kNow, out));
}

TEST_CASE("re-storing a url replaces rather than duplicates", "[manifest][cache]")
{
	ManifestCache cache;
	cache.Set("https://m.example/a", Resolution("https://a.example/old.glb", kNow + 3600), 100);
	cache.Set("https://m.example/a", Resolution("https://a.example/new.glb", kNow + 3600), 250);

	REQUIRE(cache.Size() == 1);
	REQUIRE(cache.Bytes() == 250);
	AvatarManifestResolution out;
	REQUIRE(cache.Get("https://m.example/a", kNow, out));
	REQUIRE(out.avatarUrl == "https://a.example/new.glb");
}

TEST_CASE("the oldest entry is evicted beyond maxEntries", "[manifest][cache]")
{
	ManifestCache cache(2, 1 << 20);
	cache.Set("a", Resolution("https://a.example/a.glb", kNow + 3600), 1);
	cache.Set("b", Resolution("https://a.example/b.glb", kNow + 3600), 1);
	cache.Set("c", Resolution("https://a.example/c.glb", kNow + 3600), 1);

	AvatarManifestResolution out;
	REQUIRE(cache.Size() == 2);
	REQUIRE_FALSE(cache.Get("a", kNow, out));
	REQUIRE(cache.Get("b", kNow, out));
	REQUIRE(cache.Get("c", kNow, out));
}

TEST_CASE("entries are evicted to stay inside maxBytes", "[manifest][cache]")
{
	ManifestCache cache(100, 1000);
	cache.Set("a", Resolution("https://a.example/a.glb", kNow + 3600), 600);
	cache.Set("b", Resolution("https://a.example/b.glb", kNow + 3600), 600);

	AvatarManifestResolution out;
	REQUIRE(cache.Size() == 1);
	REQUIRE_FALSE(cache.Get("a", kNow, out));
	REQUIRE(cache.Get("b", kNow, out));
	REQUIRE(cache.Bytes() == 600);
}

TEST_CASE("a hit promotes an entry so it is not the next evicted", "[manifest][cache]")
{
	ManifestCache cache(2, 1 << 20);
	cache.Set("a", Resolution("https://a.example/a.glb", kNow + 3600), 1);
	cache.Set("b", Resolution("https://a.example/b.glb", kNow + 3600), 1);

	AvatarManifestResolution out;
	REQUIRE(cache.Get("a", kNow, out));	// a is now most recently used
	cache.Set("c", Resolution("https://a.example/c.glb", kNow + 3600), 1);

	REQUIRE(cache.Get("a", kNow, out));
	REQUIRE_FALSE(cache.Get("b", kNow, out));
}

TEST_CASE("clear empties the cache and its byte total", "[manifest][cache]")
{
	ManifestCache cache;
	cache.Set("a", Resolution("https://a.example/a.glb", kNow + 3600), 500);
	cache.Clear();
	REQUIRE(cache.Size() == 0);
	REQUIRE(cache.Bytes() == 0);
}

TEST_CASE("a zero maxEntries still holds one entry rather than none", "[manifest][cache]")
{
	// A cache that can never store anything would silently turn every
	// resolution into a fresh fetch.
	ManifestCache cache(0, 1 << 20);
	cache.Set("a", Resolution("https://a.example/a.glb", kNow + 3600), 1);
	AvatarManifestResolution out;
	REQUIRE(cache.Get("a", kNow, out));
}
