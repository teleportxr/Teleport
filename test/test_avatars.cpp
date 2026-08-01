// Round-trip tests for the avatar-negotiation JSON codecs declared in
// TeleportCore/Avatars.h and implemented in TeleportCore/AvatarsJson.cpp.
// Each test builds a struct, serialises it to nlohmann::json, parses the
// result back, and compares — guaranteeing the wire format stays stable
// without coupling to a specific string layout.

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>
#include "TeleportCore/Avatars.h"

using nlohmann::json;
using namespace teleport::core;

TEST_CASE("SignalingCapabilities serialises as an empty object", "[avatars][capabilities]")
{
	// No capabilities are defined: the bag is an extension point only.
	// Avatars deliberately need none — an avatar arrives as an ordinary
	// mesh pointer, which every client can already fetch.
	json j = SignalingCapabilities{};
	REQUIRE(j.is_object());
	REQUIRE(j.empty());
}

TEST_CASE("SignalingCapabilities ignores unknown keys rather than failing", "[avatars][capabilities]")
{
	REQUIRE_NOTHROW(json::object().get<SignalingCapabilities>());
	REQUIRE_NOTHROW(json::parse("{\"unknown_future_flag\": true}").get<SignalingCapabilities>());
	REQUIRE_NOTHROW(json::parse("{\"avatar_relay\": true}").get<SignalingCapabilities>());
}

TEST_CASE("AvatarPolicy round-trips through JSON", "[avatars]")
{
	AvatarPolicy p;
	p.policyId = 12345ULL;
	p.requirement = "required";
	p.defaultAvailable = true;
	p.requirements.formats = { "glb", "vrm" };
	p.requirements.maxFileBytes = 8 * 1024 * 1024;
	p.requirements.maxTriangles = 60000;
	p.requirements.skeleton = "humanoid";
	p.requirements.licenceTagsAllowed = { "CC0", "CC-BY" };
	p.proof.required = true;
	p.proof.acceptedSchemes = { "jws-detached", "well-known-url" };
	p.fetchTimeoutMs = 7500;

	json j = p;
	AvatarPolicy q = j.get<AvatarPolicy>();
	REQUIRE(q.policyId == p.policyId);
	REQUIRE(q.requirement == p.requirement);
	REQUIRE(q.defaultAvailable == p.defaultAvailable);
	REQUIRE(q.requirements.formats == p.requirements.formats);
	REQUIRE(q.requirements.maxFileBytes == p.requirements.maxFileBytes);
	REQUIRE(q.requirements.maxTriangles == p.requirements.maxTriangles);
	REQUIRE(q.requirements.skeleton == p.requirements.skeleton);
	REQUIRE(q.requirements.licenceTagsAllowed == p.requirements.licenceTagsAllowed);
	REQUIRE(q.proof.required == true);
	REQUIRE(q.proof.acceptedSchemes == p.proof.acceptedSchemes);
	REQUIRE(q.fetchTimeoutMs == p.fetchTimeoutMs);
}

TEST_CASE("AvatarRequirements preserves unknown keys via extras bag", "[avatars]")
{
	json j = json::parse(R"({
		"formats": ["glb"],
		"max_file_bytes": 1024,
		"future_constraint": {"haircount": 9000}
	})");
	AvatarRequirements r = j.get<AvatarRequirements>();
	REQUIRE(r.formats == std::vector<std::string>{ "glb" });
	REQUIRE(r.maxFileBytes == 1024ULL);
	json re = r;
	REQUIRE(re.contains("future_constraint"));
	REQUIRE(re.at("future_constraint").at("haircount").get<int>() == 9000);
}

TEST_CASE("AvatarOffer round-trips with proof and declared", "[avatars]")
{
	AvatarOffer o;
	o.policyId = 42;
	o.haveAvatar = true;
	o.url = "https://avatars.example.com/u/42.glb";
	o.contentHash = "sha256:abcd";
	AvatarDeclared d;
	d.format = "glb";
	d.fileBytes = 4096;
	d.triangles = 1200;
	o.declared = d;
	AvatarProofOffer pr;
	pr.scheme = "jws-detached";
	pr.value = "eyJ...";
	o.proof = pr;
	o.allowRelay = false;

	json j = o;
	AvatarOffer q = j.get<AvatarOffer>();
	REQUIRE(q.policyId == o.policyId);
	REQUIRE(q.haveAvatar == o.haveAvatar);
	REQUIRE(q.url == o.url);
	REQUIRE(q.contentHash == o.contentHash);
	REQUIRE(q.declared.has_value());
	REQUIRE(q.declared->format == "glb");
	REQUIRE(q.declared->fileBytes == 4096ULL);
	REQUIRE(q.proof.has_value());
	REQUIRE(q.proof->scheme == "jws-detached");
	REQUIRE(q.allowRelay == false);
}

TEST_CASE("AvatarOffer round-trips with have_avatar=false", "[avatars]")
{
	AvatarOffer o;
	o.policyId = 7;
	o.haveAvatar = false;
	json j = o;
	AvatarOffer q = j.get<AvatarOffer>();
	REQUIRE(q.policyId == 7);
	REQUIRE(q.haveAvatar == false);
	REQUIRE_FALSE(q.url.has_value());
	REQUIRE_FALSE(q.declared.has_value());
}

TEST_CASE("AvatarResult round-trips", "[avatars]")
{
	AvatarResult r;
	r.policyId = 3;
	r.status = "accepted";
	r.nodeUid = 999;
	r.usingDefault = false;
	r.delivery = "relay";
	r.reasons = { "ok" };
	json j = r;
	AvatarResult q = j.get<AvatarResult>();
	REQUIRE(q.policyId == r.policyId);
	REQUIRE(q.status == r.status);
	REQUIRE(q.nodeUid == r.nodeUid);
	REQUIRE(q.delivery == r.delivery);
	REQUIRE(q.reasons == r.reasons);
}

TEST_CASE("AvatarRevoke round-trips", "[avatars]")
{
	AvatarRevoke r{ 17, "licence_expired" };
	json j = r;
	AvatarRevoke q = j.get<AvatarRevoke>();
	REQUIRE(q.policyId == 17);
	REQUIRE(q.reason == "licence_expired");
}

TEST_CASE("AvatarResult defaults to relay delivery", "[avatars]")
{
	// Relay is the default: peers fetch the owner's own url. A result that
	// omits the field must not be read as the server having re-hosted.
	AvatarResult r;
	REQUIRE(r.delivery == "relay");
	AvatarResult q = json::parse("{\"policy_id\": 1, \"status\": \"accepted\"}").get<AvatarResult>();
	REQUIRE(q.delivery == "relay");
}
