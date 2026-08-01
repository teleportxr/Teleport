#include "TeleportServer/AvatarService.h"

#include "TeleportServer/IAvatarImporter.h"

#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Redact.h"

using nlohmann::json;

namespace teleport
{
	namespace server
	{
		namespace
		{
			constexpr const char *kSignalTypeAvatarPolicy = "avatar-policy";
			constexpr const char *kSignalTypeAvatarResult = "avatar-result";

			std::string Envelope(const char *type, const json &content)
			{
				json j = {
					{ "teleport-signal-type", type },
					{ "content",              content }
				};
				return j.dump();
			}
		}

		AvatarService::AvatarService(avs::uid clid, SendFn fn)
			: clientID(clid)
			, sendFn(std::move(fn))
		{
		}

		void AvatarService::SendPolicy(const core::AvatarPolicy &policy)
		{
			if (!sendFn)
				return;
			currentPolicy = policy;
			json content = policy;
			TELEPORT_INTERNAL_COUT(Default, "avatar-policy -> client {} policy_id={}", clientID, policy.policyId);
			sendFn(Envelope(kSignalTypeAvatarPolicy, content));
		}

		void AvatarService::HandleOffer(const json &content)
		{
			core::AvatarOffer offer;
			try
			{
				offer = content.get<core::AvatarOffer>();
			}
			catch (const std::exception &e)
			{
				TELEPORT_INTERNAL_CERR("avatar-offer parse failed for client {}: {}", clientID, e.what());
				return;
			}
			lastOffer = offer;
			// The URL may carry a bearer token; only ever log it redacted
			// (plans/avatars_plan.md §8).
			TELEPORT_INTERNAL_COUT(Default, "avatar-offer  <- client {} policy_id={} have_avatar={} url={}"
				, clientID, offer.policyId, offer.haveAvatar, offer.url ? core::RedactUrl(*offer.url) : "<none>");

			// If we have not sent a policy, or the offer references a different
			// policy_id, reject so the client knows it is talking about something
			// the server does not currently care about.
			if (!currentPolicy.has_value() || offer.policyId != currentPolicy->policyId)
			{
				core::AvatarResult rejected;
				rejected.policyId     = offer.policyId;
				rejected.status       = "rejected";
				rejected.nodeUid      = 0;
				rejected.usingDefault = false;
				rejected.delivery     = "import";
				rejected.reasons      = { "policy_unknown" };
				Reply(rejected);
				return;
			}

			// Validation is host-side in the C++ server (IAvatarValidator has
			// no library default), so every matching offer falls back to the
			// default avatar. With an importer wired, the default is a real
			// scene node whose uid travels back in the result; without one,
			// nodeUid stays 0 (Phase-2 behaviour).
			core::AvatarResult ok;
			ok.policyId     = offer.policyId;
			ok.status       = "using_default";
			ok.nodeUid      = importer ? importer->ImportDefaultForClient(clientID) : 0;
			ok.usingDefault = true;
			ok.delivery     = "import";
			Reply(ok);
		}

		void AvatarService::HandleRevoke(const json &content)
		{
			avs::uid policyId = 0;
			if (content.is_object() && content.contains("policy_id"))
				policyId = content.at("policy_id").get<avs::uid>();
			TELEPORT_INTERNAL_COUT(Default, "avatar-revoke <- client {} policy_id={}", clientID, policyId);
			// A revoke from the client withdraws its avatar: drop cached
			// state and remove any imported node. The server keeps the same
			// policy in force and a new offer is expected next.
			lastOffer.reset();
			lastResult.reset();
			if (importer)
				importer->RemoveForClient(clientID);
		}

		void AvatarService::Reply(const core::AvatarResult &result)
		{
			lastResult = result;
			json content = result;
			TELEPORT_INTERNAL_COUT(Default, "avatar-result -> client {} status={} delivery={}"
				, clientID, result.status, result.delivery);
			if (sendFn)
				sendFn(Envelope(kSignalTypeAvatarResult, content));
		}
	}
}
