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

TEST_CASE("SignalingCapabilities round-trips through JSON", "[avatars][capabilities]")
{
	SignalingCapabilities a;
	a.avatarRelay = true;
	json j = a;
	REQUIRE(j.at("avatar_relay").get<bool>() == true);
	SignalingCapabilities b = j.get<SignalingCapabilities>();
	REQUIRE(b.avatarRelay == true);
}

TEST_CASE("SignalingCapabilities defaults to all-false on empty / missing keys", "[avatars][capabilities]")
{
	SignalingCapabilities c = json::object().get<SignalingCapabilities>();
	REQUIRE(c.avatarRelay == false);

	SignalingCapabilities d = json::parse("{\"unknown_future_flag\": true}").get<SignalingCapabilities>();
	REQUIRE(d.avatarRelay == false);
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

TEST_CASE("PeerAvatar round-trips", "[avatars]")
{
	PeerAvatar p;
	p.peerClientId = 100;
	p.peerNodeUid = 200;
	p.url = "https://example.com/a.glb";
	p.contentHash = "sha256:ff";
	p.format = "glb";
	AvatarProofOffer pr{ "well-known-url", "https://example.com/.well-known/avatar-binding" };
	p.proof = pr;
	json j = p;
	PeerAvatar q = j.get<PeerAvatar>();
	REQUIRE(q.peerClientId == 100);
	REQUIRE(q.peerNodeUid == 200);
	REQUIRE(q.url == p.url);
	REQUIRE(q.contentHash == p.contentHash);
	REQUIRE(q.format == p.format);
	REQUIRE(q.proof.has_value());
	REQUIRE(q.proof->scheme == "well-known-url");
}

TEST_CASE("PeerAvatarFailed round-trips", "[avatars]")
{
	PeerAvatarFailed f{ 200, "404" };
	json j = f;
	PeerAvatarFailed g = j.get<PeerAvatarFailed>();
	REQUIRE(g.peerNodeUid == 200);
	REQUIRE(g.reason == "404");
}
