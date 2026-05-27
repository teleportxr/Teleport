// Unit tests for the bounded-LRU validation cache introduced in Phase 3
// of the avatars implementation plan (see plans/avatars_implementation.md
// §4.1 / §4.3). The cache is header-only, so this TU only needs to pull
// in the header and Catch2.

#include <catch2/catch_test_macros.hpp>

#include "TeleportServer/AvatarValidationCache.h"

using teleport::server::AvatarValidationCache;
using teleport::server::AvatarValidationResult;

namespace
{
	AvatarValidationResult MakeOk(std::uint64_t bytes)
	{
		AvatarValidationResult r;
		r.ok     = true;
		r.format = "glb";
		r.bytes  = bytes;
		return r;
	}
}

TEST_CASE("AvatarValidationCache: round-trips a single entry", "[avatar][cache]")
{
	AvatarValidationCache c(8, 1 << 20);
	c.Put("sha256:aa", MakeOk(100), 100);
	REQUIRE(c.Size()        == 1);
	REQUIRE(c.TotalBytes()  == 100);
	auto got = c.Get("sha256:aa");
	REQUIRE(got.has_value());
	REQUIRE(got->ok          == true);
	REQUIRE(got->contentHash == "sha256:aa");
	REQUIRE(got->bytes       == 100);
}

TEST_CASE("AvatarValidationCache: Get on a missing key returns nullopt", "[avatar][cache]")
{
	AvatarValidationCache c(8, 1 << 20);
	REQUIRE_FALSE(c.Get("sha256:none").has_value());
}

TEST_CASE("AvatarValidationCache: replacing an existing key updates byte total", "[avatar][cache]")
{
	AvatarValidationCache c(8, 1 << 20);
	c.Put("sha256:aa", MakeOk(100), 100);
	c.Put("sha256:aa", MakeOk(250), 250);
	REQUIRE(c.Size()        == 1);
	REQUIRE(c.TotalBytes()  == 250);
}

TEST_CASE("AvatarValidationCache: evicts oldest entry past maxEntries", "[avatar][cache]")
{
	AvatarValidationCache c(2, 1 << 20);
	c.Put("a", MakeOk(1), 1);
	c.Put("b", MakeOk(1), 1);
	c.Put("c", MakeOk(1), 1);
	REQUIRE(c.Size() == 2);
	REQUIRE_FALSE(c.Get("a").has_value());
	REQUIRE(c.Get("b").has_value());
	REQUIRE(c.Get("c").has_value());
}

TEST_CASE("AvatarValidationCache: evicts to honour maxBytes", "[avatar][cache]")
{
	AvatarValidationCache c(100, 10);
	c.Put("a", MakeOk(8), 8);
	REQUIRE(c.TotalBytes() == 8);
	c.Put("b", MakeOk(5), 5);
	REQUIRE_FALSE(c.Get("a").has_value());
	REQUIRE(c.Get("b").has_value());
	REQUIRE(c.TotalBytes() == 5);
}

TEST_CASE("AvatarValidationCache: Get promotes recency", "[avatar][cache]")
{
	AvatarValidationCache c(2, 1 << 20);
	c.Put("a", MakeOk(1), 1);
	c.Put("b", MakeOk(1), 1);
	// Touch 'a' so it becomes most-recently-used; 'b' should be evicted next.
	REQUIRE(c.Get("a").has_value());
	c.Put("c", MakeOk(1), 1);
	REQUIRE(c.Get("a").has_value());
	REQUIRE_FALSE(c.Get("b").has_value());
	REQUIRE(c.Get("c").has_value());
}

TEST_CASE("AvatarValidationCache: Clear empties the cache", "[avatar][cache]")
{
	AvatarValidationCache c(8, 1 << 20);
	c.Put("a", MakeOk(1), 1);
	c.Put("b", MakeOk(1), 1);
	c.Clear();
	REQUIRE(c.Size()       == 0);
	REQUIRE(c.TotalBytes() == 0);
	REQUIRE_FALSE(c.Get("a").has_value());
}

TEST_CASE("AvatarValidationCache: minimum cap of 1 entry survives a zero argument", "[avatar][cache]")
{
	AvatarValidationCache c(0, 1 << 20);
	c.Put("a", MakeOk(1), 1);
	c.Put("b", MakeOk(1), 1);
	REQUIRE(c.Size() == 1);
	REQUIRE_FALSE(c.Get("a").has_value());
	REQUIRE(c.Get("b").has_value());
}
