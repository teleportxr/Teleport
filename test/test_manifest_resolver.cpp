// End-to-end manifest resolution in the C++ server: address → fetch → the
// Universal Manifest six-stage evaluation sequence → avatar url.
//
// The transport is injected so no network is touched, but IsBlockedAddress —
// the SSRF guard the real transport installs on every socket, including every
// redirect hop — is exercised directly against real sockaddr structures. It is
// the part of this feature it would be least excusable to get wrong: the fetch
// is a server-side request to a url a client chose.
//
// Mirrors teleport-nodejs/test/test_manifest_resolver.js.

#include <catch2/catch_test_macros.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <nlohmann/json.hpp>

#include "TeleportServer/DefaultAvatarManifestResolver.h"
#include "manifest_fixture.h"

using nlohmann::json;
using namespace teleport::server;
using namespace teleport::test;

namespace
{
	const char *kManifestUrl = "https://manifests.example/me.jsonld";

	teleport::core::AvatarManifestRequirements Requirements()
	{
		teleport::core::AvatarManifestRequirements r;
		r.accepted		 = { teleport::core::kManifestContextV03 };
		r.avatarPointers = { "portableIdentity.avatar" };
		r.requestedFacets = { "avatarProfile" };
		return r;
	}

	//! Builds a resolver whose transport serves `body` for any url, recording
	//! what it was asked for.
	struct Harness
	{
		std::unique_ptr<DefaultAvatarManifestResolver> resolver;
		std::vector<std::string> requestedUrls;
		std::vector<uint64_t>    requestedMaxBytes;
		int                      fetchCount = 0;

		AvatarManifestResolution Resolve(const teleport::core::AvatarManifestOffer &address,
			const teleport::core::AvatarManifestRequirements &requirements)
		{
			AvatarManifestResolution out;
			resolver->Resolve(address, requirements, [&out](const AvatarManifestResolution &r) { out = r; });
			return out;
		}
	};

	std::unique_ptr<Harness> MakeHarness(const std::string &body,
		DefaultAvatarManifestResolver::Options options = {}, const std::string &failReason = {})
	{
		auto harness = std::make_unique<Harness>();
		harness->resolver = std::make_unique<DefaultAvatarManifestResolver>(options);
		Harness *raw = harness.get();
		harness->resolver->SetFetcher([raw, body, failReason](const std::string &url, uint64_t maxBytes, uint32_t)
		{
			raw->requestedUrls.push_back(url);
			raw->requestedMaxBytes.push_back(maxBytes);
			raw->fetchCount++;
			ManifestFetchResult result;
			if (!failReason.empty())
			{
				result.reason = failReason;
				return result;
			}
			if (body.size() > maxBytes)
			{
				result.reason = "manifest_too_large";
				return result;
			}
			result.ok = true;
			result.body = body;
			result.finalUrl = url;
			return result;
		});
		return harness;
	}

	json FixtureWith(const std::function<void(json &)> &mutate)
	{
		json manifest = json::parse(kSignedManifestJson);
		mutate(manifest);
		return manifest;
	}
}

// SSRF guard -------------------------------------------------------

TEST_CASE("IsBlockedAddress refuses everything a server-side fetch must not reach", "[manifest][resolver][security]")
{
	auto v4 = [](const char *text)
	{
		::sockaddr_in a{};
		a.sin_family = AF_INET;
		REQUIRE(inet_pton(AF_INET, text, &a.sin_addr) == 1);
		return IsBlockedAddress(&a, sizeof(a));
	};
	auto v6 = [](const char *text)
	{
		::sockaddr_in6 a{};
		a.sin6_family = AF_INET6;
		REQUIRE(inet_pton(AF_INET6, text, &a.sin6_addr) == 1);
		return IsBlockedAddress(&a, sizeof(a));
	};

	SECTION("private, loopback, link-local and metadata v4")
	{
		REQUIRE(v4("127.0.0.1"));
		REQUIRE(v4("10.1.2.3"));
		REQUIRE(v4("172.16.0.1"));
		REQUIRE(v4("172.31.255.255"));
		REQUIRE(v4("192.168.0.1"));
		REQUIRE(v4("169.254.169.254"));	// AWS/GCP metadata service
		REQUIRE(v4("0.0.0.0"));
		REQUIRE(v4("224.0.0.1"));
		REQUIRE(v4("198.18.0.1"));
	}
	SECTION("public v4 is allowed")
	{
		REQUIRE_FALSE(v4("8.8.8.8"));
		REQUIRE_FALSE(v4("1.1.1.1"));
		// Just outside the RFC1918 172.16/12 block, both sides.
		REQUIRE_FALSE(v4("172.15.0.1"));
		REQUIRE_FALSE(v4("172.32.0.1"));
	}
	SECTION("loopback, link-local, ULA and multicast v6")
	{
		REQUIRE(v6("::1"));
		REQUIRE(v6("::"));
		REQUIRE(v6("fe80::1"));
		REQUIRE(v6("fd00::1"));
		REQUIRE(v6("fc00::1"));
		REQUIRE(v6("ff02::1"));
	}
	SECTION("v4-mapped v6 does not bypass the v4 list")
	{
		REQUIRE(v6("::ffff:127.0.0.1"));
		REQUIRE(v6("::ffff:169.254.169.254"));
		REQUIRE_FALSE(v6("::ffff:8.8.8.8"));
	}
	SECTION("public v6 is allowed")
	{
		REQUIRE_FALSE(v6("2001:4860:4860::8888"));
	}
	SECTION("anything unparseable fails closed")
	{
		REQUIRE(IsBlockedAddress(nullptr, 0));
		::sockaddr_in tooShort{};
		REQUIRE(IsBlockedAddress(&tooShort, 1));
		::sockaddr unixSocket{};
		unixSocket.sa_family = AF_UNIX;
		REQUIRE(IsBlockedAddress(&unixSocket, sizeof(unixSocket)));
	}
}

// Addressing -------------------------------------------------------

TEST_CASE("a manifest address becomes a url", "[manifest][resolver]")
{
	DefaultAvatarManifestResolver resolver;
	teleport::core::AvatarManifestRequirements requirements;

	SECTION("an absolute url passes through")
	{
		teleport::core::AvatarManifestOffer address;
		address.url = kManifestUrl;
		REQUIRE(resolver.AddressToUrl(address, requirements) == kManifestUrl);
	}
	SECTION("a umid is appended to the resolver base")
	{
		teleport::core::AvatarManifestOffer address;
		address.umid = "abc123";
		REQUIRE(resolver.AddressToUrl(address, requirements) == "https://myum.net/abc123");
	}
	SECTION("a policy may name its own resolver")
	{
		teleport::core::AvatarManifestOffer address;
		address.umid = "abc123";
		requirements.resolver = "https://um.example";	// no trailing slash
		REQUIRE(resolver.AddressToUrl(address, requirements) == "https://um.example/abc123");
	}
	SECTION("a umid cannot escape its path segment")
	{
		teleport::core::AvatarManifestOffer address;
		address.umid = "../../etc/passwd";
		REQUIRE(resolver.AddressToUrl(address, requirements) == "https://myum.net/..%2F..%2Fetc%2Fpasswd");
	}
	SECTION("a b64u: prefix is preserved and only its payload escaped")
	{
		teleport::core::AvatarManifestOffer address;
		address.umid = "b64u:YWJj";
		REQUIRE(resolver.AddressToUrl(address, requirements) == "https://myum.net/b64u:YWJj");
	}
	SECTION("an address with neither form yields nothing")
	{
		REQUIRE(resolver.AddressToUrl(teleport::core::AvatarManifestOffer{}, requirements).empty());
	}
}

TEST_CASE("an address with neither url nor umid is unresolvable", "[manifest][resolver]")
{
	auto harness = MakeHarness(kSignedManifestJson);
	const auto out = harness->Resolve({}, Requirements());
	REQUIRE_FALSE(out.ok);
	REQUIRE(out.reasons == std::vector<std::string>{ "manifest_unresolvable" });
	REQUIRE(harness->fetchCount == 0);
}

TEST_CASE("a non-https manifest address is refused before any network call", "[manifest][resolver][security]")
{
	auto harness = MakeHarness(kSignedManifestJson);
	teleport::core::AvatarManifestOffer address;
	address.url = "http://manifests.example/me.jsonld";
	const auto out = harness->Resolve(address, Requirements());
	REQUIRE_FALSE(out.ok);
	REQUIRE(out.reasons == std::vector<std::string>{ "scheme_not_allowed" });
	REQUIRE(harness->fetchCount == 0);
}

// Happy path -------------------------------------------------------

TEST_CASE("a valid manifest resolves to its avatar url", "[manifest][resolver]")
{
	auto harness = MakeHarness(kSignedManifestJson);
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;

	const auto out = harness->Resolve(address, Requirements());
	REQUIRE(out.ok);
	REQUIRE(out.avatarUrl == kFixtureAvatarUrl);
	REQUIRE(out.subject == "did:web:xr.example:users:beta");
	REQUIRE(out.receipt.outcome == "accepted");
	REQUIRE(out.receipt.signatureCheck == "valid");
	REQUIRE(out.receipt.freshnessCheck == "fresh");
	REQUIRE(out.receipt.manifestId == "urn:uuid:6dfc40f2-8797-4f7b-a5f7-49d6a010f600");
}

TEST_CASE("the requested facet is projected and consented", "[manifest][resolver]")
{
	auto harness = MakeHarness(kSignedManifestJson);
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;

	const auto out = harness->Resolve(address, Requirements());
	REQUIRE(out.ok);
	REQUIRE(out.projection["facets"].size() == 1);
	REQUIRE(out.projection["facets"][0]["name"] == "avatarProfile");
	REQUIRE(out.projection["facets"][0]["entity"]["skeletonProfile"] == "humanoid-v1");
	REQUIRE(out.receipt.facetStatuses.size() == 1);
	REQUIRE(out.receipt.facetStatuses[0].status == "processed");
}

TEST_CASE("a facet the server did not ask for is never read", "[manifest][resolver][privacy]")
{
	// Honest reporting: present in the manifest, deliberately not projected.
	// A different statement from absent — and its contents must not reach the
	// host application.
	auto harness = MakeHarness(kSignedManifestJson);
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;

	teleport::core::AvatarManifestRequirements requirements = Requirements();
	requirements.requestedFacets = { "somethingElseEntirely" };

	const auto out = harness->Resolve(address, requirements);
	REQUIRE(out.ok);
	REQUIRE(out.projection["facets"].empty());
	REQUIRE(out.receipt.facetStatuses.size() == 1);
	REQUIRE(out.receipt.facetStatuses[0].name == "avatarProfile");
	REQUIRE(out.receipt.facetStatuses[0].status == "not-projected");
	REQUIRE(out.receipt.outcome == "accepted-partial");
}

TEST_CASE("a requested facet with no consent is withheld but the avatar still resolves", "[manifest][resolver]")
{
	// Partial acceptance is the normal outcome, not an error: a subject
	// withholding one facet should still get their avatar.
	auto harness = MakeHarness(kManifestWithoutConsentJson);
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;

	const auto out = harness->Resolve(address, Requirements());
	REQUIRE(out.ok);
	REQUIRE(out.avatarUrl == kFixtureAvatarUrl);
	REQUIRE(out.projection["facets"].empty());
	REQUIRE(out.receipt.facetStatuses.size() == 1);
	REQUIRE(out.receipt.facetStatuses[0].status == "consent-missing");
	REQUIRE(out.receipt.outcome == "accepted-partial");
}

TEST_CASE("a sealed facet is acknowledged, not a rejection", "[manifest][resolver]")
{
	// Being unable to decrypt someone else's facet says nothing about
	// whether the manifest is valid, or whether their avatar should load.
	auto harness = MakeHarness(kManifestWithSealedFacetJson);
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;

	const auto out = harness->Resolve(address, Requirements());
	REQUIRE(out.ok);
	REQUIRE(out.avatarUrl == kFixtureAvatarUrl);
	REQUIRE(out.projection["facets"].empty());
	REQUIRE(out.receipt.facetStatuses[0].status == "opaque");
	REQUIRE(out.receipt.outcome == "accepted-partial");
}

TEST_CASE("a denied avatar pointer is refused", "[manifest][resolver]")
{
	// Where the subject has spoken about the pointer, we obey.
	auto harness = MakeHarness(kManifestWithDeniedAvatarJson);
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;

	const auto out = harness->Resolve(address, Requirements());
	REQUIRE_FALSE(out.ok);
	REQUIRE(out.reasons == std::vector<std::string>{ "manifest_consent_missing" });
	REQUIRE(out.receipt.outcome == "rejected");
}

TEST_CASE("a relative pointer target resolves against the manifest url", "[manifest][resolver]")
{
	auto harness = MakeHarness(kManifestWithRelativeTargetJson);
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;

	const auto out = harness->Resolve(address, Requirements());
	REQUIRE(out.ok);
	REQUIRE(out.avatarUrl == "https://manifests.example/avatars/me.glb");
}

// Verify stage -----------------------------------------------------

TEST_CASE("a tampered manifest is rejected with a receipt recording why", "[manifest][resolver][security]")
{
	const json manifest = FixtureWith([](json &m)
	{
		m["pointers"][0]["target"] = "https://evil.example/other.glb";
	});
	auto harness = MakeHarness(manifest.dump());
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;

	const auto out = harness->Resolve(address, Requirements());
	REQUIRE_FALSE(out.ok);
	REQUIRE(out.avatarUrl.empty());
	REQUIRE(out.receipt.signatureCheck == "invalid");
	REQUIRE(out.receipt.outcome == "rejected");
}

TEST_CASE("a manifest in a context the deployment does not accept is refused", "[manifest][resolver]")
{
	auto harness = MakeHarness(kSignedManifestJson);
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;

	teleport::core::AvatarManifestRequirements requirements = Requirements();
	requirements.accepted = { "https://universalmanifest.net/ns/v0.9" };

	const auto out = harness->Resolve(address, requirements);
	REQUIRE_FALSE(out.ok);
	REQUIRE(out.reasons == std::vector<std::string>{ "manifest_context_not_accepted" });
}

TEST_CASE("a body that is not a manifest is malformed", "[manifest][resolver]")
{
	auto harness = MakeHarness("this is not json");
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;
	REQUIRE(harness->Resolve(address, Requirements()).reasons == std::vector<std::string>{ "manifest_malformed" });
}

// Pointers ---------------------------------------------------------

TEST_CASE("a manifest with no avatar pointer is rejected", "[manifest][resolver]")
{
	auto harness = MakeHarness(kManifestWithoutAvatarPointerJson);
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;

	const auto out = harness->Resolve(address, Requirements());
	REQUIRE_FALSE(out.ok);
	REQUIRE(out.reasons == std::vector<std::string>{ "manifest_no_avatar_pointer" });
	REQUIRE(out.receipt.outcome == "rejected");
}

TEST_CASE("portableIdentity.avatar is tried as a last resort", "[manifest][resolver]")
{
	// A deployment that names only its own pointer still resolves a manifest
	// using the conventional name, which is what the published Universal
	// Manifest XR fixtures use.
	auto harness = MakeHarness(kSignedManifestJson);
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;

	teleport::core::AvatarManifestRequirements requirements = Requirements();
	requirements.avatarPointers = { "game.avatar" };
	REQUIRE(harness->Resolve(address, requirements).avatarUrl == kFixtureAvatarUrl);
}

TEST_CASE("the offer pointer hint is tried before the policy list", "[manifest][resolver]")
{
	auto harness = MakeHarness(kSignedManifestJson);
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;
	address.pointer = "portableIdentity.avatar";
	REQUIRE(harness->Resolve(address, Requirements()).avatarUrl == kFixtureAvatarUrl);
}

// Trust tiers ------------------------------------------------------

TEST_CASE("a trust tier above 0 refuses rather than downgrades", "[manifest][resolver]")
{
	// Tier 0 is signature-only, which is all this implementation can verify.
	// Claiming otherwise would be worse than admitting we cannot.
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;

	SECTION("declared by the manifest")
	{
		auto harness = MakeHarness(kManifestRequiringTier1Json);
		const auto out = harness->Resolve(address, Requirements());
		REQUIRE_FALSE(out.ok);
		REQUIRE(out.reasons == std::vector<std::string>{ "manifest_trust_tier_unsupported" });
	}
	SECTION("demanded by the policy even when the manifest declares none")
	{
		auto harness = MakeHarness(kSignedManifestJson);
		teleport::core::AvatarManifestRequirements requirements = Requirements();
		requirements.requiredTrustTier = 1;
		const auto out = harness->Resolve(address, requirements);
		REQUIRE_FALSE(out.ok);
		REQUIRE(out.reasons == std::vector<std::string>{ "manifest_trust_tier_unsupported" });
	}
}

// Transport --------------------------------------------------------

TEST_CASE("the manifest byte cap is far below the asset cap and can only be tightened", "[manifest][resolver]")
{
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;

	SECTION("the default")
	{
		auto harness = MakeHarness(kSignedManifestJson);
		harness->Resolve(address, Requirements());
		REQUIRE(harness->requestedMaxBytes.front() == 256 * 1024);
	}
	SECTION("a policy may tighten it")
	{
		auto harness = MakeHarness(kSignedManifestJson);
		teleport::core::AvatarManifestRequirements requirements = Requirements();
		requirements.maxBytes = 1024;
		harness->Resolve(address, requirements);
		REQUIRE(harness->requestedMaxBytes.front() == 1024);
	}
	SECTION("a policy may not loosen it")
	{
		auto harness = MakeHarness(kSignedManifestJson);
		teleport::core::AvatarManifestRequirements requirements = Requirements();
		requirements.maxBytes = 100 * 1024 * 1024;
		harness->Resolve(address, requirements);
		REQUIRE(harness->requestedMaxBytes.front() == 256 * 1024);
	}
}

TEST_CASE("transport failures pass through the shared reason vocabulary", "[manifest][resolver]")
{
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;
	for (const char *reason : { "ssrf_blocked", "fetch_timeout", "too_many_redirects", "http_404", "manifest_too_large" })
	{
		INFO("reason: " << reason);
		auto harness = MakeHarness(kSignedManifestJson, {}, reason);
		REQUIRE(harness->Resolve(address, Requirements()).reasons == std::vector<std::string>{ reason });
	}
}

// Caching ----------------------------------------------------------

TEST_CASE("a second resolution of the same url is served from cache", "[manifest][resolver]")
{
	auto harness = MakeHarness(kSignedManifestJson);
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;

	REQUIRE(harness->Resolve(address, Requirements()).ok);
	REQUIRE(harness->Resolve(address, Requirements()).ok);
	REQUIRE(harness->fetchCount == 1);
}

TEST_CASE("a rejected manifest is not cached", "[manifest][resolver]")
{
	// Caching a rejection would let a transient problem stick.
	const json manifest = FixtureWith([](json &m) { m["subject"] = "did:web:evil"; });
	auto harness = MakeHarness(manifest.dump());
	teleport::core::AvatarManifestOffer address;
	address.url = kManifestUrl;

	REQUIRE_FALSE(harness->Resolve(address, Requirements()).ok);
	REQUIRE_FALSE(harness->Resolve(address, Requirements()).ok);
	REQUIRE(harness->fetchCount == 2);
}
