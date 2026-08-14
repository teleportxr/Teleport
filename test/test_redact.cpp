// Tests for the URL/proof redaction helpers (plans/avatars_plan.md §8).
// Mirrors teleport-nodejs/test/test_redact.js and
// teleport-web-client/tests/redact.test.ts so the three implementations
// keep identical semantics.

#include <catch2/catch_test_macros.hpp>

#include "TeleportCore/Redact.h"

using teleport::core::RedactProof;
using teleport::core::RedactUrl;

TEST_CASE("RedactUrl strips path, query and credentials down to scheme+host", "[redact]")
{
	CHECK(RedactUrl("https://avatars.example.com/u/abcd1234.glb?token=SECRET") == "https://avatars.example.com/...");
	CHECK(RedactUrl("https://user:pass@host.example/x") == "https://host.example/...");
	CHECK(RedactUrl("http://host.example:8080/deep/path") == "http://host.example:8080/...");
	CHECK(RedactUrl("https://host.example") == "https://host.example/...");
}

TEST_CASE("RedactUrl keeps server-relative paths but strips query and fragment", "[redact]")
{
	CHECK(RedactUrl("/avatars/abc123.glb") == "/avatars/abc123.glb");
	CHECK(RedactUrl("/avatars/abc.glb?token=SECRET") == "/avatars/abc.glb");
	CHECK(RedactUrl("/avatars/abc.glb#SECRET") == "/avatars/abc.glb");
	// Protocol-relative is a host, not a path.
	CHECK(RedactUrl("//host.example/x") == "<invalid-url>");
}

TEST_CASE("RedactUrl never echoes an unparseable input", "[redact]")
{
	CHECK(RedactUrl("not a url with SECRET in it") == "<invalid-url>");
	CHECK(RedactUrl("://missing-scheme") == "<invalid-url>");
	CHECK(RedactUrl("https://") == "<invalid-url>");
	CHECK(RedactUrl("") == "<no-url>");
}

TEST_CASE("RedactProof describes without echoing the value", "[redact]")
{
	const std::string value(84, 'x');
	CHECK(RedactProof(value, "jws-detached") == "<jws-detached 84 bytes>");
	CHECK(RedactProof("abc") == "<proof 3 bytes>");
	CHECK(RedactProof("TOPSECRET").find("TOPSECRET") == std::string::npos);
}
