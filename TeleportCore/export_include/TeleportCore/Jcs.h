#pragma once

#include <string>

#include <nlohmann/json.hpp>

//! RFC 8785 JSON Canonicalisation Scheme (JCS).
//!
//! Universal Manifest Signature Profile A signs the JCS serialisation of a
//! manifest with its `signature` member removed, so this is on the critical
//! path for every manifest verification.
//!
//! It is mirrored byte-for-byte by teleport-nodejs/manifest/jcs.js. The two
//! implementations are held together by the shared vector set duplicated in
//! Teleport/test/test_jcs.cpp and teleport-nodejs/test/test_manifest_jcs.js —
//! if they diverge, a manifest signed by the Node.js server fails to verify
//! on this one and the failure presents as a bad signature rather than as
//! the serialisation bug it actually is. Change one, change both.
//!
//! Two details do all the work and neither is what a C++ programmer would
//! reach for by default:
//!
//!   * Object members sort by UTF-16 code unit, NOT by UTF-8 byte value.
//!     The two orders agree for everything in the Basic Multilingual Plane
//!     and disagree above it, because a surrogate pair's leading unit
//!     (0xD800-0xDBFF) sorts below U+E000-U+FFFF while its code point sorts
//!     above. nlohmann's std::map ordering is byte-wise and therefore wrong
//!     for keys containing astral characters; Serialize sorts explicitly.
//!   * Numbers serialise per ECMAScript Number::toString, which is not any
//!     printf format. See SerializeNumber in Jcs.cpp.

namespace teleport
{
	namespace core
	{
		//! Canonicalise a parsed JSON value to its RFC 8785 form.
		//! Throws std::invalid_argument for values RFC 8785 cannot represent
		//! (non-finite numbers), rather than emitting something that would
		//! verify against bytes nobody signed.
		std::string CanonicalizeJson(const nlohmann::json &value);

		//! Compare two UTF-8 strings by the UTF-16 code units they encode.
		//! Returns <0, 0 or >0. Exposed for testing.
		int CompareUtf16(const std::string &a, const std::string &b);

		//! ECMAScript Number::toString for a finite double. Exposed for testing.
		std::string SerializeJsonNumber(double value);
	}
}
