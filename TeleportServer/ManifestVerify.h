#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "TeleportCore/Manifest.h"

//! Stage 2 of the Universal Manifest evaluation sequence: Verify.
//!
//! Signature Profile A is the only profile supported: JCS-RFC8785
//! canonicalisation of the manifest with `signature` removed, signed with
//! Ed25519. Anything else is reported as an unsupported profile rather than
//! silently accepted.
//!
//! Mirrors teleport-nodejs/manifest/verify.js. The two are held together by
//! the cross-language fixtures in Teleport/test/test_manifest_verify.cpp: a
//! manifest signed by the Node.js implementation must verify here.
//!
//! What this deliberately does NOT do is check that the manifest's `subject`
//! is the client presenting it. Verification proves the document is authentic
//! and unmodified; it does not prove the bearer has any right to it. Binding
//! subject to the connecting identity is the avatar-proof work and is tracked
//! separately. Until it lands, a verified manifest is genuine but not
//! necessarily yours.

namespace teleport
{
	namespace server
	{
		//! Fetches an https url, for resolving a keyRef that is not self-
		//! contained. Returns false on any failure; the body is JSON.
		using ManifestKeyFetchFn = std::function<bool(const std::string &url, std::string &bodyOut)>;

		struct ManifestVerifyResult
		{
			//! valid | invalid | unsupported-profile
			std::string              signatureCheck = "unsupported-profile";
			//! fresh | expired | stale
			std::string              freshnessCheck = "expired";
			std::vector<std::string> reasons;
		};

		//! Tolerance for disagreeing clocks between the manifest issuer and
		//! this server. Generous enough to survive an unsynchronised host,
		//! tight enough that an expired manifest is not usable for long.
		constexpr int64_t kDefaultClockSkewSeconds = 60;

		//! The bytes that get signed: the manifest minus `signature`,
		//! canonicalised per RFC 8785.
		std::string ManifestSigningInput(const nlohmann::json &manifest);

		//! Freshness against issuedAt/expiresAt. Returns fresh | expired |
		//! stale. `stale` is used for a manifest whose issuedAt is in the
		//! future beyond the skew allowance: not expired, but not something a
		//! correct clock should be presenting either.
		std::string CheckManifestFreshness(const nlohmann::json &manifest, int64_t nowUnix,
			int64_t clockSkewSeconds = kDefaultClockSkewSeconds);

		//! Full stage 2.
		ManifestVerifyResult VerifyManifest(const nlohmann::json &manifest, int64_t nowUnix,
			int64_t clockSkewSeconds = kDefaultClockSkewSeconds, ManifestKeyFetchFn keyFetch = {});

		// Key handling, exposed for testing ----------------------------

		//! base58btc, as used by did:key.
		bool Base58Decode(const std::string &text, std::vector<uint8_t> &out);
		bool Base64Decode(const std::string &text, std::vector<uint8_t> &out);

		//! Extract the 32 raw Ed25519 public key bytes from each supported
		//! keyRef form. Return false when the reference cannot be resolved —
		//! which is a verification failure even when an inline key is present,
		//! because accepting an inline key without checking it against the
		//! keyRef is exactly what lets an attacker re-sign a modified manifest
		//! with their own key.
		bool KeyFromDidKey(const std::string &did, std::vector<uint8_t> &rawOut);
		bool KeyFromSpkiBase64(const std::string &b64, std::vector<uint8_t> &rawOut);
		bool KeyFromJwkDocument(const nlohmann::json &doc, const std::string &kid, std::vector<uint8_t> &rawOut);

		//! Raw Ed25519 verify over `message`.
		bool Ed25519Verify(const std::vector<uint8_t> &rawKey, const std::string &message,
			const std::vector<uint8_t> &signature);
	}
}
