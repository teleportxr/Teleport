// Cross-language verification of a Universal Manifest.
//
// This is the test that matters most in the whole manifest feature. The
// fixture below was produced by the Node.js implementation
// (teleport-nodejs/manifest/verify.js signing over
// teleport-nodejs/manifest/jcs.js) with a fixed Ed25519 key, and is verified
// here by the C++ one. Two independent canonicalisers and two independent
// Ed25519 stacks have to agree byte-for-byte for it to pass; nothing else in
// the codebase exercises that agreement end to end.
//
// The fixture deliberately contains a member no v0.3 consumer understands
// (`somethingFromAFutureVersion`, itself holding nested objects with unsorted
// keys and non-ASCII text). Unknown members are covered by the signature, so
// this doubles as proof that forward-compatibility does not come at the cost
// of verifiability.
//
// To regenerate after an intentional format change, see the generator note at
// the bottom of this file.

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include "TeleportCore/Jcs.h"
#include "TeleportCore/Manifest.h"
#include "TeleportServer/ManifestVerify.h"

using nlohmann::json;
using namespace teleport::server;

namespace
{
	// Signed by teleport-nodejs with the Ed25519 key whose seed is the bytes
	// 1..32, i.e. did:key:z6MkneMkZqwqRiU5mJzSG3kDwzt9P8C59N4NGTfBLfSGE7c7.
	const char *kSignedManifest = R"JSON({
 "@context": "https://universalmanifest.net/ns/v0.3",
 "@id": "urn:uuid:6dfc40f2-8797-4f7b-a5f7-49d6a010f600",
 "@type": [ "um:Manifest" ],
 "manifestVersion": "0.3",
 "subject": "did:web:xr.example:users:beta",
 "issuedAt": "2026-03-05T12:05:00Z",
 "expiresAt": "2036-03-05T12:05:00Z",
 "pointers": [
  { "@type": "um:Pointer", "name": "portableIdentity.avatar", "target": "https://assets.example/avatars/beta.glb" }
 ],
 "facets": [
  {
   "@id": "urn:facet:avatarProfile",
   "@type": [ "um:Facet" ],
   "name": "avatarProfile",
   "entity": {
    "@id": "did:web:xr.example:users:beta#avatar",
    "@type": [ "um:Entity", "xr:AvatarProfile" ],
    "skeletonProfile": "humanoid-v1",
    "supportsFacialBlendShapes": true
   }
  }
 ],
 "consents": [
  {
   "@id": "urn:consent:avatarProfile",
   "@type": "um:Consent",
   "facetRef": "urn:facet:avatarProfile",
   "scope": [ "read", "display" ],
   "purpose": "avatar-presentation",
   "grantedAt": "2026-03-05T12:05:00Z",
   "expiresAt": "2036-03-05T12:05:00Z"
  }
 ],
 "somethingFromAFutureVersion": {
  "nested": [ 1, 2, { "z": 1, "a": 2 } ],
  "unicode": "héllo €"
 },
 "signature": {
  "algorithm": "Ed25519",
  "canonicalization": "JCS-RFC8785",
  "keyRef": "did:key:z6MkneMkZqwqRiU5mJzSG3kDwzt9P8C59N4NGTfBLfSGE7c7",
  "value": "KOCqdwSRp2utPrJLf8ewIhV1v85ONlrlS0z0CkqoLEXgZ7osv_YB0_VUotraU1ciQnW26RzZPnYdXguERoz6AA"
 }
})JSON";

	const char *kDidKey = "did:key:z6MkneMkZqwqRiU5mJzSG3kDwzt9P8C59N4NGTfBLfSGE7c7";
	const char *kSpkiB64 = "MCowBQYDK2VwAyEAebVWLo/mVPlAeLES6KmLp5AfhTrmlb7X4OORC60ElmQ=";

	// Comfortably inside the fixture's validity window.
	constexpr int64_t kNow = 1800000000;	// 2027-01-15
}

TEST_CASE("a manifest signed by the Node.js implementation verifies here", "[manifest][verify][crosslang]")
{
	const json manifest = json::parse(kSignedManifest);
	const ManifestVerifyResult result = VerifyManifest(manifest, kNow);
	REQUIRE(result.signatureCheck == "valid");
	REQUIRE(result.freshnessCheck == "fresh");
	REQUIRE(result.reasons.empty());
}

TEST_CASE("the canonical signing input matches what Node.js produced", "[manifest][verify][crosslang]")
{
	// 956 UTF-8 bytes for this payload. A mismatch here localises a
	// divergence to the canonicaliser rather than leaving it looking like a
	// key or signature problem.
	//
	// Note the unit. teleport-nodejs' canonicalize() returns a JS string, so
	// String.length reports 953 — UTF-16 code units, three fewer, because
	// this fixture's "héllo €" holds one two-byte and one three-byte
	// character. What gets signed is always the UTF-8 encoding (RFC 8785
	// §3.3), which is what canonicalizeToBuffer produces and what this
	// counts.
	const json manifest = json::parse(kSignedManifest);
	const std::string input = ManifestSigningInput(manifest);
	REQUIRE(input.size() == 956);
	REQUIRE(input.find("\"signature\"") == std::string::npos);
	REQUIRE(input.rfind("{\"@context\":", 0) == 0);
	// Members of the unknown future object are sorted too, all the way down.
	REQUIRE(input.find("{\"a\":2,\"z\":1}") != std::string::npos);
}

TEST_CASE("tampering with any covered member breaks the signature", "[manifest][verify]")
{
	SECTION("the avatar pointer")
	{
		json manifest = json::parse(kSignedManifest);
		manifest["pointers"][0]["target"] = "https://evil.example/other.glb";
		REQUIRE(VerifyManifest(manifest, kNow).signatureCheck == "invalid");
	}
	SECTION("the subject")
	{
		json manifest = json::parse(kSignedManifest);
		manifest["subject"] = "did:web:xr.example:users:mallory";
		REQUIRE(VerifyManifest(manifest, kNow).signatureCheck == "invalid");
	}
	SECTION("an unknown member added after signing")
	{
		// Unknown members are preserved AND covered; that is what makes
		// forward compatibility safe rather than a hole.
		json manifest = json::parse(kSignedManifest);
		manifest["yetAnotherFutureMember"] = 1;
		REQUIRE(VerifyManifest(manifest, kNow).signatureCheck == "invalid");
	}
	SECTION("an unknown member removed after signing")
	{
		json manifest = json::parse(kSignedManifest);
		manifest.erase("somethingFromAFutureVersion");
		REQUIRE(VerifyManifest(manifest, kNow).signatureCheck == "invalid");
	}
}

TEST_CASE("member order in the received document does not affect verification", "[manifest][verify]")
{
	// The whole point of canonicalisation: nlohmann stores members in its own
	// order, and a proxy or a different JSON library may reorder them again.
	// Neither may break the signature.
	const json manifest = json::parse(kSignedManifest);
	const json reserialised = json::parse(manifest.dump());
	REQUIRE(VerifyManifest(reserialised, kNow).signatureCheck == "valid");
}

TEST_CASE("key substitution is refused", "[manifest][verify][security]")
{
	// The attack: re-sign a modified manifest with your own key, then set
	// publicKeySpkiB64 to your key so the document self-verifies. Skipping
	// key resolution because an inline key is present is exactly the
	// non-conformance that allows it.
	json manifest = json::parse(kSignedManifest);
	// An inline key that is valid base64 SPKI but not the one keyRef names.
	manifest["signature"]["publicKeySpkiB64"] = "MCowBQYDK2VwAyEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
	const ManifestVerifyResult result = VerifyManifest(manifest, kNow);
	REQUIRE(result.signatureCheck == "invalid");
	REQUIRE(std::find(result.reasons.begin(), result.reasons.end(), "manifest_signature_invalid") != result.reasons.end());
}

TEST_CASE("an inline key agreeing with keyRef is accepted", "[manifest][verify]")
{
	json manifest = json::parse(kSignedManifest);
	manifest["signature"]["publicKeySpkiB64"] = kSpkiB64;
	REQUIRE(VerifyManifest(manifest, kNow).signatureCheck == "valid");
}

TEST_CASE("an unresolvable keyRef is refused even with an inline key", "[manifest][verify][security]")
{
	json manifest = json::parse(kSignedManifest);
	manifest["signature"]["keyRef"] = "did:web:issuer.example";
	manifest["signature"]["publicKeySpkiB64"] = kSpkiB64;
	const ManifestVerifyResult result = VerifyManifest(manifest, kNow);
	REQUIRE(result.signatureCheck == "invalid");
	REQUIRE(std::find(result.reasons.begin(), result.reasons.end(), "manifest_key_unresolvable") != result.reasons.end());
}

TEST_CASE("an https keyRef resolves through the injected fetcher", "[manifest][verify]")
{
	json manifest = json::parse(kSignedManifest);
	manifest["signature"]["keyRef"] = "https://issuer.example/jwks.json#k1";

	// The raw key, base64url, as a JWKS would carry it.
	std::vector<uint8_t> raw;
	REQUIRE(KeyFromDidKey(kDidKey, raw));
	std::string x;
	{
		static const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
		int accumulator = 0, bits = 0;
		for (uint8_t b : raw)
		{
			accumulator = (accumulator << 8) | b;
			bits += 8;
			while (bits >= 6)
			{
				bits -= 6;
				x.push_back(alphabet[(accumulator >> bits) & 0x3F]);
			}
		}
		if (bits)
			x.push_back(alphabet[(accumulator << (6 - bits)) & 0x3F]);
	}

	std::string requested;
	auto fetcher = [&requested, &x](const std::string &url, std::string &bodyOut)
	{
		requested = url;
		bodyOut = "{\"keys\":[{\"kty\":\"OKP\",\"crv\":\"Ed25519\",\"kid\":\"k1\",\"x\":\"" + x + "\"}]}";
		return true;
	};

	const ManifestVerifyResult result = VerifyManifest(manifest, kNow, kDefaultClockSkewSeconds, fetcher);
	REQUIRE(result.signatureCheck == "valid");
	// The fragment names the key, it is not part of the url to fetch.
	REQUIRE(requested == "https://issuer.example/jwks.json");
}

TEST_CASE("an https keyRef with no fetcher is unresolvable", "[manifest][verify]")
{
	json manifest = json::parse(kSignedManifest);
	manifest["signature"]["keyRef"] = "https://issuer.example/jwks.json";
	const ManifestVerifyResult result = VerifyManifest(manifest, kNow);
	REQUIRE(std::find(result.reasons.begin(), result.reasons.end(), "manifest_key_unresolvable") != result.reasons.end());
}

TEST_CASE("an unsupported signature profile is reported as such", "[manifest][verify]")
{
	SECTION("algorithm")
	{
		json manifest = json::parse(kSignedManifest);
		manifest["signature"]["algorithm"] = "RS256";
		REQUIRE(VerifyManifest(manifest, kNow).signatureCheck == "unsupported-profile");
	}
	SECTION("canonicalisation")
	{
		json manifest = json::parse(kSignedManifest);
		manifest["signature"]["canonicalization"] = "URDNA2015";
		REQUIRE(VerifyManifest(manifest, kNow).signatureCheck == "unsupported-profile");
	}
	SECTION("missing signature")
	{
		json manifest = json::parse(kSignedManifest);
		manifest.erase("signature");
		REQUIRE(VerifyManifest(manifest, kNow).signatureCheck == "unsupported-profile");
	}
}

TEST_CASE("freshness is checked independently of authenticity", "[manifest][verify]")
{
	const json manifest = json::parse(kSignedManifest);

	SECTION("inside the window")
	{
		REQUIRE(CheckManifestFreshness(manifest, kNow) == "fresh");
	}
	SECTION("past expiresAt beyond the skew allowance")
	{
		// 2036-03-05 plus a decade.
		REQUIRE(CheckManifestFreshness(manifest, 2400000000) == "expired");
	}
	SECTION("just past expiresAt but inside the skew allowance")
	{
		int64_t expires = 0;
		REQUIRE(teleport::core::ParseRfc3339("2036-03-05T12:05:00Z", expires));
		REQUIRE(CheckManifestFreshness(manifest, expires + 10) == "fresh");
		REQUIRE(CheckManifestFreshness(manifest, expires + 10, 0) == "expired");
	}
	SECTION("before issuedAt beyond the skew allowance")
	{
		// Not expired, but not something a correct clock should present.
		REQUIRE(CheckManifestFreshness(manifest, 1500000000) == "stale");
	}
	SECTION("an expired manifest still has a valid signature")
	{
		const ManifestVerifyResult result = VerifyManifest(manifest, 2400000000);
		REQUIRE(result.freshnessCheck == "expired");
		REQUIRE(result.signatureCheck == "valid");
	}
}

TEST_CASE("did:key decoding", "[manifest][verify]")
{
	std::vector<uint8_t> raw;
	REQUIRE(KeyFromDidKey(kDidKey, raw));
	REQUIRE(raw.size() == 32);

	SECTION("a fragment names the same key")
	{
		std::vector<uint8_t> withFragment;
		REQUIRE(KeyFromDidKey(std::string(kDidKey) + "#z6Mk", withFragment));
		REQUIRE(withFragment == raw);
	}
	SECTION("the inline SPKI form decodes to the same bytes")
	{
		std::vector<uint8_t> fromSpki;
		REQUIRE(KeyFromSpkiBase64(kSpkiB64, fromSpki));
		REQUIRE(fromSpki == raw);
	}
	SECTION("a non-Ed25519 multicodec prefix is refused")
	{
		// z6LS... is the X25519 prefix (0xec01): a key agreement key, which
		// cannot verify a signature.
		std::vector<uint8_t> out;
		REQUIRE_FALSE(KeyFromDidKey("did:key:z6LSbysY2xFMRpGMhb7tFTLMpeuPRaqaWM1yECx2AtzE3KCc", out));
	}
	SECTION("malformed input is refused rather than throwing")
	{
		std::vector<uint8_t> out;
		REQUIRE_FALSE(KeyFromDidKey("did:key:z0OIl", out));	// not in the base58 alphabet
		REQUIRE_FALSE(KeyFromDidKey("did:web:example.com", out));
		REQUIRE_FALSE(KeyFromDidKey("", out));
	}
}

TEST_CASE("base64 decoding accepts both standard and url-safe alphabets", "[manifest][verify]")
{
	std::vector<uint8_t> standard;
	std::vector<uint8_t> urlSafe;
	REQUIRE(Base64Decode(kSpkiB64, standard));
	std::string swapped = kSpkiB64;
	for (char &c : swapped)
	{
		if (c == '+') c = '-';
		else if (c == '/') c = '_';
	}
	REQUIRE(Base64Decode(swapped, urlSafe));
	REQUIRE(standard == urlSafe);
}

// To regenerate kSignedManifest after an intentional format change, run from
// teleport-nodejs/ (the seed is the bytes 1..32, so the key is stable):
//
//   node -e '
//     const crypto=require("node:crypto");
//     const fx=require("./test/helpers/manifest_fixtures.js");
//     const seed=Buffer.alloc(32); for(let i=0;i<32;i++) seed[i]=i+1;
//     const pkcs8=Buffer.concat([Buffer.from("302e020100300506032b657004220420","hex"),seed]);
//     const key=crypto.createPrivateKey({key:pkcs8,format:"der",type:"pkcs8"});
//     const did=fx.didKeyFor(crypto.createPublicKey(key));
//     /* build the envelope exactly as above, then: */
//     console.log(JSON.stringify(fx.sign(envelope,key,did),null,1));'
//
// A regeneration that changes the signature value without an intended format
// change means the two canonicalisers have drifted — investigate rather than
// pasting in the new value.
