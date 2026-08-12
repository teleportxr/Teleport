// RFC 8785 JSON Canonicalisation Scheme.
//
// The vectors below are duplicated verbatim in
// teleport-nodejs/test/test_manifest_jcs.js. That duplication is deliberate
// and is the only thing holding the two canonicalisers together: if they
// diverge, a manifest signed by the Node.js server fails to verify here and
// the failure presents as a bad signature rather than as the serialisation
// bug it actually is. Add a vector to one file, add it to the other.

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>
#include "TeleportCore/Jcs.h"

using nlohmann::json;
using namespace teleport::core;

namespace
{
	struct Vector
	{
		const char *description;
		const char *input;
		const char *expected;
	};

	const Vector kSharedVectors[] = {
		{ "empty object",					"{}",								"{}" },
		{ "empty array",					"[]",								"[]" },
		{ "null",							"null",								"null" },
		{ "booleans in an array",			"[true,false]",						"[true,false]" },
		{ "member order is sorted",			"{\"b\":1,\"a\":2}",				"{\"a\":2,\"b\":1}" },
		{ "whitespace is removed",			"{ \"a\" : [ 1 , 2 ] }",			"{\"a\":[1,2]}" },
		{ "nested objects sort independently",
											"{\"b\":{\"d\":1,\"c\":2},\"a\":3}",	"{\"a\":3,\"b\":{\"c\":2,\"d\":1}}" },
		{ "array order is preserved",		"{\"a\":[3,1,2]}",					"{\"a\":[3,1,2]}" },
		// Sorting is on UTF-16 code units, so an uppercase letter sorts
		// before a lowercase one and a digit before both.
		{ "ascii key ordering",				"{\"a\":1,\"A\":2,\"1\":3}",		"{\"1\":3,\"A\":2,\"a\":1}" },
		{ "prefix keys",					"{\"ab\":1,\"a\":2}",				"{\"a\":2,\"ab\":1}" },
		{ "non-ascii key ordering",			"{\"\\u00e9\":1,\"z\":2}",			"{\"z\":2,\"\u00e9\":1}" },
		// The case that separates UTF-16 code-unit order from the UTF-8 byte
		// order a std::map would give: U+1F600 encodes as the surrogate pair
		// D83D DE00, and D83D sorts BELOW U+FFFD, so the emoji key comes
		// first — the opposite of code-point order.
		{ "astral key ordering",			"{\"\\ufffd\":1,\"\\ud83d\\ude00\":2}",
											"{\"\U0001F600\":2,\"\uFFFD\":1}" },
		{ "integer numbers",				"{\"a\":1,\"b\":-0,\"c\":0}",		"{\"a\":1,\"b\":0,\"c\":0}" },
		{ "fractional numbers",				"{\"a\":1.5,\"b\":1.0,\"c\":100.0}",	"{\"a\":1.5,\"b\":1,\"c\":100}" },
		{ "large and small numbers",		"{\"a\":1e21,\"b\":1e-7}",			"{\"a\":1e+21,\"b\":1e-7}" },
		// The positional/exponential switch is made on the decimal exponent
		// alone — below 1e-6 and at or above 1e21 — never on whichever form
		// happens to be shorter. printf-family formatting gets this wrong,
		// and so does std::to_chars' shortest mode, which is why Jcs.cpp
		// re-formats rather than using it directly.
		{ "1e-5 stays positional",			"{\"a\":1e-5}",						"{\"a\":0.00001}" },
		{ "1e-6 stays positional",			"{\"a\":1e-6}",						"{\"a\":0.000001}" },
		{ "1e-7 goes exponential",			"{\"a\":1e-7}",						"{\"a\":1e-7}" },
		{ "1e20 stays positional",			"{\"a\":1e20}",						"{\"a\":100000000000000000000}" },
		{ "1e21 goes exponential",			"{\"a\":1e21}",						"{\"a\":1e+21}" },
		{ "exponent carries no padding zeros",	"{\"a\":1.5e-7}",				"{\"a\":1.5e-7}" },
		{ "fraction below the switch",		"{\"a\":0.0001}",					"{\"a\":0.0001}" },
		{ "mixed integer and fraction",		"{\"a\":123.456}",					"{\"a\":123.456}" },
		{ "negative fraction",				"{\"a\":-1.5}",						"{\"a\":-1.5}" },
		{ "string escapes",					"{\"a\":\"\\\"\\\\\\b\\f\\n\\r\\t\"}",
											"{\"a\":\"\\\"\\\\\\b\\f\\n\\r\\t\"}" },
		{ "control character escape",		"{\"a\":\"\\u0000\\u001f\"}",		"{\"a\":\"\\u0000\\u001f\"}" },
		{ "forward slash is not escaped",	"{\"a\":\"/\"}",					"{\"a\":\"/\"}" },
		{ "non-ascii is not escaped",		"{\"a\":\"\\u00e9\\u20ac\"}",		"{\"a\":\"\u00e9\u20ac\"}" },
		{ "surrogate pair is not escaped",	"{\"a\":\"\\ud83d\\ude00\"}",		"{\"a\":\"\U0001F600\"}" },
	};
}

TEST_CASE("JCS shared vectors match the Node.js canonicaliser", "[jcs]")
{
	for (const Vector &v : kSharedVectors)
	{
		INFO("vector: " << v.description);
		REQUIRE(CanonicalizeJson(json::parse(v.input)) == v.expected);
	}
}

TEST_CASE("JCS canonicalisation is stable under re-parsing", "[jcs]")
{
	const json value = json::parse("{\"z\":[1,{\"b\":2,\"a\":3}],\"a\":\"x\"}");
	const std::string once = CanonicalizeJson(value);
	REQUIRE(CanonicalizeJson(json::parse(once)) == once);
}

TEST_CASE("JCS ignores the order members were inserted in", "[jcs]")
{
	json a;
	a["one"] = 1;
	a["two"] = 2;
	a["three"] = 3;

	json b;
	b["three"] = 3;
	b["one"] = 1;
	b["two"] = 2;

	REQUIRE(CanonicalizeJson(a) == CanonicalizeJson(b));
}

TEST_CASE("JCS refuses non-finite numbers rather than serialising them", "[jcs]")
{
	// RFC 8785 has no representation for these, and quietly emitting null
	// would produce a document that verifies against bytes nobody signed.
	REQUIRE_THROWS_AS(SerializeJsonNumber(std::nan("")), std::invalid_argument);
	REQUIRE_THROWS_AS(SerializeJsonNumber(HUGE_VAL), std::invalid_argument);
	REQUIRE_THROWS_AS(SerializeJsonNumber(-HUGE_VAL), std::invalid_argument);
}

TEST_CASE("CompareUtf16 orders by utf-16 code unit, not by code point", "[jcs]")
{
	REQUIRE(CompareUtf16("A", "a") < 0);
	REQUIRE(CompareUtf16("a", "b") < 0);
	REQUIRE(CompareUtf16("a", "a") == 0);
	REQUIRE(CompareUtf16("z", "\u00e9") < 0);
	// The distinguishing case: byte-wise (code-point) comparison would put
	// U+FFFD first; UTF-16 comparison puts the astral character first.
	REQUIRE(CompareUtf16("\U0001F600", "\uFFFD") < 0);
	REQUIRE(std::string("\U0001F600") > std::string("\uFFFD"));
}

TEST_CASE("CompareUtf16 does not throw on malformed utf-8", "[jcs]")
{
	// A key we cannot decode still has to sort somewhere, deterministically.
	// Refusing to canonicalise a document over one bad byte would be a worse
	// failure than ordering it oddly.
	const std::string truncated = "\xE2\x82";	// leading bytes of U+20AC, cut short
	const std::string stray     = "\xFF";
	REQUIRE_NOTHROW(CompareUtf16(truncated, stray));
	REQUIRE_NOTHROW(CanonicalizeJson(json{ { "ok", 1 } }));
}

TEST_CASE("SerializeJsonNumber folds negative zero", "[jcs]")
{
	REQUIRE(SerializeJsonNumber(-0.0) == "0");
	REQUIRE(SerializeJsonNumber(0.0) == "0");
}

TEST_CASE("JCS emits utf-8 unescaped", "[jcs]")
{
	const std::string out = CanonicalizeJson(json::parse("{\"\\u00e9\":\"\\u20ac\"}"));
	REQUIRE(out == "{\"\u00e9\":\"\u20ac\"}");
	// Two bytes for é, three for €, plus the seven ASCII delimiters.
	REQUIRE(out.size() == 2 + 3 + 7);
}
