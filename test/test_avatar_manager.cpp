// Behavioural tests for teleport::client::AvatarManager — the Phase 2
// client-side implementation. Verifies that:
//  * the default policy callback replies have_avatar=false,
//  * a host-supplied callback gets the parsed policy and can reply,
//  * avatar-result frames are parsed and cached,
//  * avatar-revoke clears cached state,
//  * the SignalingMessage dispatcher only consumes avatar-* frames.

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include "TeleportClient/AvatarManager.h"

using nlohmann::json;
using namespace teleport::core;
using teleport::client::AvatarManager;

namespace
{
	// Capture every JSON text frame the manager tries to send.
	struct CapturedSender
	{
		std::vector<json> frames;

		AvatarManager::SendFn fn()
		{
			return [this](const std::string &s) { frames.push_back(json::parse(s)); };
		}
	};

	std::string PolicyEnvelope(avs::uid policyId)
	{
		AvatarPolicy p;
		p.policyId = policyId;
		p.requirement = "optional";
		p.defaultAvailable = true;
		p.requirements.formats = { "glb" };
		json content = p;
		return json{ { "teleport-signal-type", "avatar-policy" }, { "content", content } }.dump();
	}

	std::string ResultEnvelope(avs::uid policyId, const std::string &status)
	{
		AvatarResult r;
		r.policyId = policyId;
		r.status = status;
		r.usingDefault = (status == "using_default");
		r.delivery = "import";
		json content = r;
		return json{ { "teleport-signal-type", "avatar-result" }, { "content", content } }.dump();
	}

	std::string RevokeEnvelope(avs::uid policyId, const std::string &reason)
	{
		AvatarRevoke v;
		v.policyId = policyId;
		v.reason = reason;
		json content = v;
		return json{ { "teleport-signal-type", "avatar-revoke" }, { "content", content } }.dump();
	}
}

TEST_CASE("AvatarManager default callback replies have_avatar=false", "[avatar-manager]")
{
	CapturedSender sink;
	AvatarManager mgr(101, sink.fn());

	REQUIRE(mgr.HandleSignalingMessage(PolicyEnvelope(7)));

	REQUIRE(sink.frames.size() == 1);
	const json &frame = sink.frames.front();
	REQUIRE(frame.at("teleport-signal-type").get<std::string>() == "avatar-offer");
	const auto &content = frame.at("content");
	REQUIRE(content.at("policy_id").get<avs::uid>() == 7);
	REQUIRE(content.at("have_avatar").get<bool>() == false);
	REQUIRE(mgr.HasCurrentPolicy());
	REQUIRE(mgr.GetCurrentPolicy()->policyId == 7);
}

TEST_CASE("AvatarManager host-supplied callback receives policy and can reply", "[avatar-manager]")
{
	CapturedSender sink;
	AvatarManager mgr(101, sink.fn());

	AvatarPolicy captured;
	bool invoked = false;
	mgr.SetOnAvatarPolicy([&](const AvatarPolicy &policy, AvatarManager::ReplyFn reply)
	{
		captured = policy;
		invoked = true;
		AvatarOffer offer;
		offer.policyId   = policy.policyId;
		offer.haveAvatar = true;
		offer.url        = "https://avatars.example/u/1.glb";
		offer.allowRelay = true;
		reply(offer);
	});

	REQUIRE(mgr.HandleSignalingMessage(PolicyEnvelope(42)));

	REQUIRE(invoked);
	REQUIRE(captured.policyId == 42);
	REQUIRE(sink.frames.size() == 1);
	const auto &content = sink.frames.front().at("content");
	REQUIRE(content.at("have_avatar").get<bool>() == true);
	REQUIRE(content.at("url").get<std::string>() == "https://avatars.example/u/1.glb");
	REQUIRE(content.at("allow_relay").get<bool>() == true);
}

TEST_CASE("AvatarManager parses avatar-result and caches it", "[avatar-manager]")
{
	CapturedSender sink;
	AvatarManager mgr(101, sink.fn());

	AvatarResult observed;
	bool invoked = false;
	mgr.SetOnAvatarResult([&](const AvatarResult &r) { observed = r; invoked = true; });

	REQUIRE(mgr.HandleSignalingMessage(ResultEnvelope(11, "using_default")));

	REQUIRE(invoked);
	REQUIRE(observed.policyId == 11);
	REQUIRE(observed.status == "using_default");
	REQUIRE(observed.usingDefault == true);
	REQUIRE(mgr.GetLastResult().has_value());
	REQUIRE(mgr.GetLastResult()->status == "using_default");
}

TEST_CASE("AvatarManager handles avatar-revoke by clearing state", "[avatar-manager]")
{
	CapturedSender sink;
	AvatarManager mgr(101, sink.fn());
	REQUIRE(mgr.HandleSignalingMessage(PolicyEnvelope(7)));
	REQUIRE(mgr.HandleSignalingMessage(ResultEnvelope(7, "using_default")));
	REQUIRE(mgr.HasCurrentPolicy());
	sink.frames.clear();

	REQUIRE(mgr.HandleSignalingMessage(RevokeEnvelope(7, "licence_expired")));

	REQUIRE_FALSE(mgr.HasCurrentPolicy());
	REQUIRE_FALSE(mgr.GetLastOffer().has_value());
	REQUIRE_FALSE(mgr.GetLastResult().has_value());
}

TEST_CASE("AvatarManager ignores non-avatar signaling frames", "[avatar-manager]")
{
	CapturedSender sink;
	AvatarManager mgr(101, sink.fn());

	const std::string other = R"({"teleport-signal-type":"something-else","content":{}})";
	REQUIRE_FALSE(mgr.HandleSignalingMessage(other));
	REQUIRE_FALSE(mgr.HandleSignalingMessage("not json at all"));
	REQUIRE(sink.frames.empty());
}
