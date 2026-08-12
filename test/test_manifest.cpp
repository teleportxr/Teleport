// Universal Manifest envelope handling: the Arrive, Project, Consent and
// Compose stages implemented in TeleportCore/ManifestJson.cpp. Mirrors
// teleport-nodejs/test/test_manifest_consent.js and the parsing half of
// teleport-nodejs/test/test_manifest_resolver.js.

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>
#include "TeleportCore/Manifest.h"

using nlohmann::json;
using namespace teleport::core;

namespace
{
	constexpr int64_t kNow = 1780000000;	// 2026-06-08

	json MinimalManifest()
	{
		return json::parse(R"JSON({
			"@context": "https://universalmanifest.net/ns/v0.3",
			"@id": "urn:uuid:1",
			"@type": ["um:Manifest"],
			"manifestVersion": "0.3",
			"subject": "did:web:example:users:a",
			"issuedAt": "2026-06-01T00:00:00Z",
			"expiresAt": "2026-12-01T00:00:00Z",
			"signature": {"algorithm":"Ed25519","canonicalization":"JCS-RFC8785","keyRef":"did:key:zX","value":"AA"}
		})JSON");
	}

	json Facet(const char *id, const char *name)
	{
		json f;
		f["@id"] = id;
		f["@type"] = json::array({ "um:Facet" });
		f["name"] = name;
		f["entity"] = json{ { "@type", json::array({ "um:Entity" }) } };
		return f;
	}

	json V03Consent(const char *facetRef)
	{
		json c;
		c["@id"] = "urn:consent:1";
		c["@type"] = "um:Consent";
		c["facetRef"] = facetRef;
		c["scope"] = json::array({ "read", "display" });
		c["purpose"] = "avatar-presentation";
		c["grantedAt"] = "2026-05-01T00:00:00Z";
		c["expiresAt"] = "2026-12-01T00:00:00Z";
		return c;
	}

	std::string Gate(const json &manifest, const json &facet, std::string &reason)
	{
		std::vector<std::string> warnings;
		return GateFacet(manifest, facet, kNow, { "read" }, "", reason, warnings);
	}
}

// Arrive -----------------------------------------------------------

TEST_CASE("a well-formed manifest parses", "[manifest]")
{
	json out;
	std::string reason;
	REQUIRE(ParseManifest(MinimalManifest().dump(), { kManifestContextV03 }, out, reason));
	REQUIRE(reason.empty());
	REQUIRE(out["@id"] == "urn:uuid:1");
}

TEST_CASE("unknown members survive parsing", "[manifest]")
{
	// They are covered by the signature, so losing one turns a valid
	// document into an invalid one.
	json manifest = MinimalManifest();
	manifest["somethingFromAFutureVersion"] = json{ { "nested", json::array({ 1, 2 }) } };
	json out;
	std::string reason;
	REQUIRE(ParseManifest(manifest.dump(), { kManifestContextV03 }, out, reason));
	REQUIRE(out.contains("somethingFromAFutureVersion"));
	REQUIRE(out["somethingFromAFutureVersion"]["nested"].size() == 2);
}

TEST_CASE("a body that is not JSON is malformed rather than throwing", "[manifest]")
{
	json out;
	std::string reason;
	REQUIRE_FALSE(ParseManifest("this is not json", { kManifestContextV03 }, out, reason));
	REQUIRE(reason == "manifest_malformed");
	REQUIRE_FALSE(ParseManifest("[1,2,3]", { kManifestContextV03 }, out, reason));
	REQUIRE(reason == "manifest_malformed");
}

TEST_CASE("every required member is required", "[manifest]")
{
	for (const char *member : { "@context", "@id", "@type", "manifestVersion", "subject", "issuedAt", "expiresAt", "signature" })
	{
		INFO("missing " << member);
		json manifest = MinimalManifest();
		manifest.erase(member);
		json out;
		std::string reason;
		REQUIRE_FALSE(ParseManifest(manifest.dump(), { kManifestContextV03 }, out, reason));
		REQUIRE(reason == "manifest_malformed");
	}
}

TEST_CASE("a document that is not a um:Manifest is refused", "[manifest]")
{
	json manifest = MinimalManifest();
	manifest["@type"] = json::array({ "um:Receipt" });
	json out;
	std::string reason;
	REQUIRE_FALSE(ParseManifest(manifest.dump(), { kManifestContextV03 }, out, reason));
	REQUIRE(reason == "manifest_malformed");
}

TEST_CASE("a context outside the accept list is refused", "[manifest]")
{
	json manifest = MinimalManifest();
	manifest["@context"] = "https://universalmanifest.net/ns/v0.1";
	json out;
	std::string reason;
	REQUIRE_FALSE(ParseManifest(manifest.dump(), { kManifestContextV03 }, out, reason));
	REQUIRE(reason == "manifest_context_not_accepted");

	// A deployment may widen the list once it has tested the version.
	REQUIRE(ParseManifest(manifest.dump(), { "https://universalmanifest.net/ns/v0.1" }, out, reason));
}

TEST_CASE("an empty accept list defaults to the stable context", "[manifest]")
{
	json out;
	std::string reason;
	REQUIRE(ParseManifest(MinimalManifest().dump(), {}, out, reason));
}

// Pointers ---------------------------------------------------------

TEST_CASE("pointers are read in both the v0.1 and v0.3 shapes", "[manifest][pointers]")
{
	json manifest = MinimalManifest();
	manifest["pointers"] = json::array({
		json{ { "name", "portableIdentity.wearables" }, { "url", "https://a.example/w.json" } },
		json{ { "@type", "um:Pointer" }, { "name", "portableIdentity.avatar" }, { "target", "https://a.example/me.glb" } },
	});

	// v0.1 {name, url}
	const json *wearables = FindPointer(manifest, { "portableIdentity.wearables" });
	REQUIRE(wearables != nullptr);
	REQUIRE(PointerTarget(*wearables) == "https://a.example/w.json");

	// v0.3 {@type, target}
	const json *avatar = FindPointer(manifest, { "portableIdentity.avatar" });
	REQUIRE(avatar != nullptr);
	REQUIRE(PointerTarget(*avatar) == "https://a.example/me.glb");
}

TEST_CASE("pointer names are tried in order so a caller can express precedence", "[manifest][pointers]")
{
	json manifest = MinimalManifest();
	manifest["pointers"] = json::array({
		json{ { "name", "portableIdentity.avatar" }, { "target", "https://a.example/default.glb" } },
		json{ { "name", "game.avatar" },             { "target", "https://a.example/game.glb" } },
	});
	REQUIRE(PointerTarget(*FindPointer(manifest, { "game.avatar", "portableIdentity.avatar" })) == "https://a.example/game.glb");
	REQUIRE(PointerTarget(*FindPointer(manifest, { "portableIdentity.avatar", "game.avatar" })) == "https://a.example/default.glb");
}

TEST_CASE("a pointer may be matched by its @type", "[manifest][pointers]")
{
	// v0.3 registers no pointer @type for an avatar, so a deployment that
	// has defined its own term can name that term instead of a label.
	json manifest = MinimalManifest();
	manifest["pointers"] = json::array({ json{ { "@type", "xr:AvatarPointer" }, { "target", "https://a.example/me.glb" } } });
	REQUIRE(FindPointer(manifest, { "xr:AvatarPointer" }) != nullptr);
	REQUIRE(FindPointer(manifest, { "portableIdentity.avatar" }) == nullptr);
}

TEST_CASE("no pointers at all is a miss, not a crash", "[manifest][pointers]")
{
	REQUIRE(FindPointer(MinimalManifest(), { "portableIdentity.avatar" }) == nullptr);
	json manifest = MinimalManifest();
	manifest["pointers"] = "not an array";
	REQUIRE(FindPointer(manifest, { "portableIdentity.avatar" }) == nullptr);
}

// Facets -----------------------------------------------------------

TEST_CASE("facets match by @id, name and semantic type", "[manifest][facets]")
{
	const json f = Facet("urn:facet:profile", "avatarProfile");
	REQUIRE(FacetMatches(f, "urn:facet:profile"));
	REQUIRE(FacetMatches(f, "avatarProfile"));
	REQUIRE(FacetMatches(f, "um:Facet"));
	REQUIRE_FALSE(FacetMatches(f, "medicalHistory"));

	json typed = f;
	typed["entity"]["@type"] = json::array({ "um:Entity", "xr:AvatarProfile" });
	REQUIRE(FacetMatches(typed, "xr:AvatarProfile"));
}

TEST_CASE("a sealed facet is recognised", "[manifest][facets]")
{
	json byProfile = Facet("urn:facet:sealed", "sealed");
	byProfile["encryptionProfile"] = "jwe-inline-v1";
	REQUIRE(IsSealedFacet(byProfile));

	json byShape = Facet("urn:facet:sealed", "sealed");
	byShape["entity"] = json{ { "protected", "eyJ" }, { "ciphertext", "abc" }, { "iv", "x" }, { "tag", "y" } };
	REQUIRE(IsSealedFacet(byShape));

	REQUIRE_FALSE(IsSealedFacet(Facet("urn:facet:plain", "plain")));
}

// Consent ----------------------------------------------------------

TEST_CASE("a facet with no consent entry is consent-missing", "[manifest][consent]")
{
	// Default-deny is the whole contract: absence of permission is refusal,
	// never permission.
	std::string reason;
	REQUIRE(Gate(MinimalManifest(), Facet("urn:facet:profile", "avatarProfile"), reason) == "consent-missing");
}

TEST_CASE("a consent for a different facet does not grant this one", "[manifest][consent]")
{
	json manifest = MinimalManifest();
	manifest["consents"] = json::array({ V03Consent("urn:facet:something-else") });
	std::string reason;
	REQUIRE(Gate(manifest, Facet("urn:facet:profile", "avatarProfile"), reason) == "consent-missing");
}

TEST_CASE("a matching consent permits processing", "[manifest][consent]")
{
	json manifest = MinimalManifest();
	manifest["consents"] = json::array({ V03Consent("urn:facet:profile") });
	std::string reason;
	REQUIRE(Gate(manifest, Facet("urn:facet:profile", "avatarProfile"), reason) == "processed");
}

TEST_CASE("a consent may reference a facet by name", "[manifest][consent]")
{
	json manifest = MinimalManifest();
	manifest["consents"] = json::array({ V03Consent("avatarProfile") });
	std::string reason;
	REQUIRE(Gate(manifest, Facet("urn:facet:profile", "avatarProfile"), reason) == "processed");
}

TEST_CASE("scope, purpose and validity are all enforced", "[manifest][consent]")
{
	const json facet = Facet("urn:facet:profile", "avatarProfile");
	std::string reason;
	std::vector<std::string> warnings;

	SECTION("scope omitting a required operation denies")
	{
		json manifest = MinimalManifest();
		json c = V03Consent("urn:facet:profile");
		c["scope"] = json::array({ "display" });
		manifest["consents"] = json::array({ c });
		REQUIRE(Gate(manifest, facet, reason) == "consent-denied");
		REQUIRE(reason == "scope_not_permitted");
	}
	SECTION("no scope at all denies")
	{
		json manifest = MinimalManifest();
		json c = V03Consent("urn:facet:profile");
		c.erase("scope");
		manifest["consents"] = json::array({ c });
		REQUIRE(Gate(manifest, facet, reason) == "consent-denied");
	}
	SECTION("a purpose the consent does not name denies")
	{
		json manifest = MinimalManifest();
		manifest["consents"] = json::array({ V03Consent("urn:facet:profile") });
		const std::string status = GateFacet(manifest, facet, kNow, { "read" }, "analytics", reason, warnings);
		REQUIRE(status == "consent-denied");
		REQUIRE(reason == "purpose_mismatch");
	}
	SECTION("a consent listing several purposes matches any of them")
	{
		json manifest = MinimalManifest();
		json c = V03Consent("urn:facet:profile");
		c["purpose"] = json::array({ "analytics", "avatar-presentation" });
		manifest["consents"] = json::array({ c });
		REQUIRE(GateFacet(manifest, facet, kNow, { "read" }, "avatar-presentation", reason, warnings) == "processed");
	}
	SECTION("an evaluator stating no purpose does not check purpose")
	{
		json manifest = MinimalManifest();
		manifest["consents"] = json::array({ V03Consent("urn:facet:profile") });
		REQUIRE(GateFacet(manifest, facet, kNow, { "read" }, "", reason, warnings) == "processed");
	}
	SECTION("an expired consent denies")
	{
		json manifest = MinimalManifest();
		json c = V03Consent("urn:facet:profile");
		c["expiresAt"] = "2026-01-01T00:00:00Z";
		manifest["consents"] = json::array({ c });
		REQUIRE(Gate(manifest, facet, reason) == "consent-denied");
		REQUIRE(reason == "expired");
	}
	SECTION("a withdrawn consent denies")
	{
		json manifest = MinimalManifest();
		json c = V03Consent("urn:facet:profile");
		c["withdrawnAt"] = "2026-05-15T00:00:00Z";
		manifest["consents"] = json::array({ c });
		REQUIRE(Gate(manifest, facet, reason) == "consent-denied");
		REQUIRE(reason == "withdrawn");
	}
	SECTION("a consent not yet granted denies")
	{
		json manifest = MinimalManifest();
		json c = V03Consent("urn:facet:profile");
		c["grantedAt"] = "2026-11-01T00:00:00Z";
		manifest["consents"] = json::array({ c });
		REQUIRE(Gate(manifest, facet, reason) == "consent-denied");
		REQUIRE(reason == "not_yet_granted");
	}
}

TEST_CASE("several consents for one facet are conjunctive", "[manifest][consent]")
{
	const json facet = Facet("urn:facet:profile", "avatarProfile");
	json manifest = MinimalManifest();
	json good = V03Consent("urn:facet:profile");
	json bad = V03Consent("urn:facet:profile");
	bad["@id"] = "urn:consent:2";
	bad["expiresAt"] = "2026-01-01T00:00:00Z";
	manifest["consents"] = json::array({ good, bad });

	std::string reason;
	REQUIRE(Gate(manifest, facet, reason) == "consent-denied");
}

TEST_CASE("v0.1 flat permissions are honoured", "[manifest][consent]")
{
	const json facet = Facet("urn:facet:x", "portableIdentity.voiceCapture");
	std::string reason;
	std::vector<std::string> warnings;

	SECTION("allowed")
	{
		json manifest = MinimalManifest();
		manifest["consents"] = json::array({ json{ { "@type", "um:Consent" }, { "name", "portableIdentity.voiceCapture" }, { "value", "allowed" } } });
		REQUIRE(Gate(manifest, facet, reason) == "processed");
	}
	SECTION("denied")
	{
		json manifest = MinimalManifest();
		manifest["consents"] = json::array({ json{ { "@type", "um:Consent" }, { "name", "portableIdentity.voiceCapture" }, { "value", "denied" } } });
		REQUIRE(Gate(manifest, facet, reason) == "consent-denied");
		REQUIRE(reason == "denied");
	}
	SECTION("restricted permits but warns")
	{
		json manifest = MinimalManifest();
		manifest["consents"] = json::array({ json{ { "@type", "um:Consent" }, { "name", "portableIdentity.voiceCapture" }, { "value", "restricted" } } });
		REQUIRE(GateFacet(manifest, facet, kNow, { "read" }, "", reason, warnings) == "processed");
		REQUIRE(warnings == std::vector<std::string>{ "consent_restricted" });
	}
	SECTION("an unrecognised value refuses")
	{
		json manifest = MinimalManifest();
		manifest["consents"] = json::array({ json{ { "@type", "um:Consent" }, { "name", "portableIdentity.voiceCapture" }, { "value", "maybe" } } });
		REQUIRE(Gate(manifest, facet, reason) == "consent-denied");
	}
}

TEST_CASE("a sealed facet is opaque, not a rejection", "[manifest][consent]")
{
	// Being unable to decrypt someone else's facet says nothing about
	// whether the manifest is valid.
	json facet = Facet("urn:facet:sealed", "sealed");
	facet["encryptionProfile"] = "jwe-inline-v1";
	std::string reason;
	REQUIRE(Gate(MinimalManifest(), facet, reason) == "opaque");
}

TEST_CASE("an unstated reference is permitted but a denied one is not", "[manifest][consent]")
{
	std::string reason;
	std::vector<std::string> warnings;

	// Consent in UM is defined over facets; demanding one per pointer would
	// reject every manifest in the wild.
	REQUIRE(GateReference(MinimalManifest(), "portableIdentity.avatar", kNow, { "read" }, "", reason, warnings) == "processed");

	json manifest = MinimalManifest();
	manifest["consents"] = json::array({ json{ { "@type", "um:Consent" }, { "name", "portableIdentity.avatar" }, { "value", "denied" } } });
	REQUIRE(GateReference(manifest, "portableIdentity.avatar", kNow, { "read" }, "", reason, warnings) == "consent-denied");
}

// Compose ----------------------------------------------------------

TEST_CASE("outcomes are graded by what happened to the facets", "[manifest][receipt]")
{
	SECTION("nothing withheld")
	{
		ManifestReceipt r;
		r.AddFacet("a", "processed");
		ComposeOutcome(r, false);
		REQUIRE(r.outcome == "accepted");
	}
	SECTION("a warning")
	{
		ManifestReceipt r;
		r.AddFacet("a", "processed");
		r.warnings.push_back("consent_restricted");
		ComposeOutcome(r, false);
		REQUIRE(r.outcome == "accepted-with-warnings");
	}
	SECTION("something withheld is partial, not a failure")
	{
		// Partial acceptance is the normal case: a subject is not obliged to
		// consent to everything, and a server is not obliged to refuse them
		// over it.
		for (const char *status : { "consent-missing", "consent-denied", "not-projected", "opaque" })
		{
			ManifestReceipt r;
			r.AddFacet("a", "processed");
			r.AddFacet("b", status);
			ComposeOutcome(r, false);
			REQUIRE(r.outcome == "accepted-partial");
		}
	}
	SECTION("fatal overrides everything")
	{
		ManifestReceipt r;
		r.AddFacet("a", "processed");
		ComposeOutcome(r, true);
		REQUIRE(r.outcome == "rejected");
	}
}

TEST_CASE("trust tiers read as raise-only integers", "[manifest][receipt]")
{
	REQUIRE(ManifestTrustTier(json(0)) == 0);
	REQUIRE(ManifestTrustTier(json(2)) == 2);
	REQUIRE(ManifestTrustTier(json(-1)) == 0);
	REQUIRE(ManifestTrustTier(json("2")) == 0);
	REQUIRE(ManifestTrustTier(json()) == 0);
	// Only tier 0 — signature-only — is actually verifiable here.
	REQUIRE(kSupportedTrustTier == 0);
}

// Codecs -----------------------------------------------------------

TEST_CASE("the receipt serialises to the compact wire shape", "[manifest][receipt]")
{
	ManifestReceipt r;
	r.manifestId = "urn:uuid:1";
	r.outcome = "accepted-partial";
	r.signatureCheck = "valid";
	r.freshnessCheck = "fresh";
	r.AddFacet("avatarProfile", "consent-missing");

	const json j = r;
	REQUIRE(j["manifest_id"] == "urn:uuid:1");
	REQUIRE(j["outcome"] == "accepted-partial");
	REQUIRE(j["signature_check"] == "valid");
	REQUIRE(j["freshness_check"] == "fresh");
	REQUIRE(j["facets"].size() == 1);
	REQUIRE(j["facets"][0]["name"] == "avatarProfile");
	REQUIRE(j["facets"][0]["status"] == "consent-missing");

	const ManifestReceipt back = j.get<ManifestReceipt>();
	REQUIRE(back.manifestId == r.manifestId);
	REQUIRE(back.outcome == r.outcome);
	REQUIRE(back.facetStatuses.size() == 1);
	REQUIRE(back.facetStatuses[0].status == "consent-missing");
}

TEST_CASE("the signature struct round-trips", "[manifest]")
{
	ManifestSignature s;
	s.algorithm = "Ed25519";
	s.canonicalization = "JCS-RFC8785";
	s.keyRef = "did:key:zAbc";
	s.value = "AAAA";

	json j = s;
	REQUIRE_FALSE(j.contains("publicKeySpkiB64"));
	ManifestSignature back = j.get<ManifestSignature>();
	REQUIRE(back.keyRef == s.keyRef);
	REQUIRE_FALSE(back.publicKeySpkiB64.has_value());

	s.publicKeySpkiB64 = "MCow";
	j = s;
	back = j.get<ManifestSignature>();
	REQUIRE(back.publicKeySpkiB64.has_value());
	REQUIRE(*back.publicKeySpkiB64 == "MCow");
}

// Time -------------------------------------------------------------

TEST_CASE("RFC 3339 timestamps parse without locale or timezone influence", "[manifest][time]")
{
	int64_t t = 0;
	REQUIRE(ParseRfc3339("1970-01-01T00:00:00Z", t));
	REQUIRE(t == 0);
	REQUIRE(ParseRfc3339("2026-06-08T00:00:00Z", t));
	REQUIRE(t == 1780876800);
	// Fractional seconds are ignored, not fatal.
	REQUIRE(ParseRfc3339("2026-06-08T00:00:00.123Z", t));
	REQUIRE(t == 1780876800);

	SECTION("an explicit offset shifts the instant")
	{
		int64_t utc = 0;
		int64_t plusOne = 0;
		REQUIRE(ParseRfc3339("2026-06-08T12:00:00Z", utc));
		REQUIRE(ParseRfc3339("2026-06-08T12:00:00+01:00", plusOne));
		REQUIRE(plusOne == utc - 3600);
	}
	SECTION("garbage is refused, which callers treat as expired")
	{
		REQUIRE_FALSE(ParseRfc3339("not-a-date", t));
		REQUIRE_FALSE(ParseRfc3339("", t));
		REQUIRE_FALSE(ParseRfc3339("2026-13-01T00:00:00Z", t));
	}
}
