// Behavioural tests for teleport::server::AvatarService — the Phase 2
// implementation should always reply `using_default` to a well-formed
// offer that targets the current policy, and `rejected` with reason
// `policy_unknown` for any other offer.

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include "TeleportServer/AvatarService.h"

using nlohmann::json;
using namespace teleport::core;
using teleport::server::AvatarService;

namespace
{
	// Capture every JSON text frame the service tries to send.
	struct CapturedSender
	{
		std::vector<json> frames;

		AvatarService::SendFn fn()
		{
			return [this](const std::string &s) { frames.push_back(json::parse(s)); };
		}
	};

	AvatarPolicy MakePolicy(avs::uid id)
	{
		AvatarPolicy p;
		p.policyId = id;
		p.requirement = "optional";
		p.defaultAvailable = true;
		p.requirements.formats = { "glb" };
		return p;
	}
}

TEST_CASE("AvatarService::SendPolicy emits avatar-policy envelope", "[avatar-service]")
{
	CapturedSender sink;
	AvatarService svc(42, sink.fn());

	svc.SendPolicy(MakePolicy(7));

	REQUIRE(sink.frames.size() == 1);
	const json &frame = sink.frames.front();
	REQUIRE(frame.at("teleport-signal-type").get<std::string>() == "avatar-policy");
	REQUIRE(frame.at("content").at("policy_id").get<avs::uid>() == 7);
	REQUIRE(frame.at("content").at("default_available").get<bool>() == true);
	REQUIRE(svc.HasCurrentPolicy());
}

TEST_CASE("AvatarService::HandleOffer replies using_default for matching policy_id", "[avatar-service]")
{
	CapturedSender sink;
	AvatarService svc(42, sink.fn());
	svc.SendPolicy(MakePolicy(11));
	sink.frames.clear();

	AvatarOffer offer;
	offer.policyId = 11;
	offer.haveAvatar = true;
	offer.url = "https://avatars.example/u/1.glb";
	json offerJson = offer;

	svc.HandleOffer(offerJson);

	REQUIRE(sink.frames.size() == 1);
	const json &frame = sink.frames.front();
	REQUIRE(frame.at("teleport-signal-type").get<std::string>() == "avatar-result");
	const auto &content = frame.at("content");
	REQUIRE(content.at("policy_id").get<avs::uid>() == 11);
	REQUIRE(content.at("status").get<std::string>() == "using_default");
	REQUIRE(content.at("using_default").get<bool>() == true);
	REQUIRE(content.at("delivery").get<std::string>() == "import");
	REQUIRE(content.at("node_uid").get<avs::uid>() == 0);
}

TEST_CASE("AvatarService::HandleOffer also uses_default when client has no avatar", "[avatar-service]")
{
	CapturedSender sink;
	AvatarService svc(42, sink.fn());
	svc.SendPolicy(MakePolicy(11));
	sink.frames.clear();

	AvatarOffer offer;
	offer.policyId = 11;
	offer.haveAvatar = false;
	json offerJson = offer;

	svc.HandleOffer(offerJson);

	REQUIRE(sink.frames.size() == 1);
	REQUIRE(sink.frames.front().at("content").at("status").get<std::string>() == "using_default");
}

TEST_CASE("AvatarService::HandleOffer rejects offer with unknown policy_id", "[avatar-service]")
{
	CapturedSender sink;
	AvatarService svc(42, sink.fn());
	svc.SendPolicy(MakePolicy(11));
	sink.frames.clear();

	AvatarOffer offer;
	offer.policyId = 999;
	json offerJson = offer;

	svc.HandleOffer(offerJson);

	REQUIRE(sink.frames.size() == 1);
	const auto &content = sink.frames.front().at("content");
	REQUIRE(content.at("status").get<std::string>() == "rejected");
	REQUIRE(content.at("reasons").size() == 1);
	REQUIRE(content.at("reasons").at(0).get<std::string>() == "policy_unknown");
}

TEST_CASE("AvatarService::HandleOffer rejects when no policy has been sent", "[avatar-service]")
{
	CapturedSender sink;
	AvatarService svc(42, sink.fn());

	AvatarOffer offer;
	offer.policyId = 11;
	json offerJson = offer;

	svc.HandleOffer(offerJson);

	REQUIRE(sink.frames.size() == 1);
	REQUIRE(sink.frames.front().at("content").at("status").get<std::string>() == "rejected");
}

TEST_CASE("AvatarService::HandleRevoke clears cached offer / result", "[avatar-service]")
{
	CapturedSender sink;
	AvatarService svc(42, sink.fn());
	svc.SendPolicy(MakePolicy(11));

	AvatarOffer offer;
	offer.policyId = 11;
	svc.HandleOffer(json(offer));
	REQUIRE(svc.GetLastOffer().has_value());
	REQUIRE(svc.GetLastResult().has_value());

	json revoke = { { "policy_id", 11 } };
	svc.HandleRevoke(revoke);
	REQUIRE_FALSE(svc.GetLastOffer().has_value());
	REQUIRE_FALSE(svc.GetLastResult().has_value());
}
