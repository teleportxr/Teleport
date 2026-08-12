#include "TeleportServer/AvatarService.h"

#include "TeleportServer/IAvatarImporter.h"
#include "TeleportServer/IAvatarManifestResolver.h"

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
			lastManifestReceipt.reset();
			// A url or manifest address may carry a bearer token; only ever
			// log either redacted (plans/avatars_plan.md §8).
			std::string offeredAddress = "<none>";
			if (offer.manifest.has_value())
				offeredAddress = "manifest " + core::RedactUrl(offer.manifest->url.value_or(offer.manifest->umid.value_or(std::string())));
			else if (offer.url.has_value())
				offeredAddress = core::RedactUrl(*offer.url);
			TELEPORT_INTERNAL_COUT(Default, "avatar-offer  <- client {} policy_id={} have_avatar={} url={}"
				, clientID, offer.policyId, offer.haveAvatar, offeredAddress);

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

			// A manifest address is an indirection in front of an asset url:
			// resolve it, and everything downstream proceeds as if the client
			// had offered the resolved url directly.
			if (offer.haveAvatar && offer.manifest.has_value() && manifestResolver)
			{
				if (!ResolveManifest(offer))
					return;
				lastOffer = offer;
			}

			// Validation is host-side in the C++ server (IAvatarValidator has
			// no library default), so every matching offer falls back to the
			// default avatar. With an importer wired, the default is a real
			// scene node whose uid travels back in the result; without one,
			// nodeUid stays 0 (Phase-2 behaviour).
			//
			// Note this applies even after a manifest has resolved: the asset
			// url is known and verified, but nothing in the library will fetch
			// or measure it until a host supplies a validator and importer.
			core::AvatarResult ok;
			ok.policyId     = offer.policyId;
			ok.status       = "using_default";
			ok.nodeUid      = importer ? importer->ImportDefaultForClient(clientID) : 0;
			ok.usingDefault = true;
			ok.delivery     = "import";
			Reply(ok);
		}

		bool AvatarService::ResolveManifest(core::AvatarOffer &offer)
		{
			core::AvatarManifestRequirements requirements;
			if (currentPolicy.has_value() && currentPolicy->requirements.manifest.has_value())
				requirements = *currentPolicy->requirements.manifest;

			AvatarManifestResolution resolution;
			bool answered = false;
			manifestResolver->Resolve(*offer.manifest, requirements,
				[&resolution, &answered](const AvatarManifestResolution &r)
				{
					resolution = r;
					answered = true;
				});

			if (!answered)
			{
				// The interface permits an asynchronous callback, but this
				// service is synchronous: a resolver that defers has not
				// answered by the time we must reply, and pretending
				// otherwise would send a result built on an empty resolution.
				TELEPORT_INTERNAL_CERR("avatar manifest resolver for client {} did not answer synchronously", clientID);
				core::AvatarResult rejected;
				rejected.policyId = offer.policyId;
				rejected.status   = "rejected";
				rejected.delivery = "import";
				rejected.reasons  = { "manifest_unresolvable" };
				Reply(rejected);
				return false;
			}

			lastManifestReceipt = resolution.receipt;

			if (!resolution.ok)
			{
				// A manifest that fails to resolve is not a protocol error:
				// the client offered something the server could not use, which
				// is the same situation as a bad url. Fall back exactly as
				// that does, with the reasons attached so the client can tell
				// what went wrong.
				core::AvatarResult result;
				result.policyId = offer.policyId;
				result.reasons  = resolution.reasons.empty()
					? std::vector<std::string>{ "manifest_unresolvable" }
					: resolution.reasons;
				result.delivery = "import";
				if (currentPolicy.has_value() && currentPolicy->defaultAvailable)
				{
					result.status       = "using_default";
					result.usingDefault = true;
					result.nodeUid      = importer ? importer->ImportDefaultForClient(clientID) : 0;
				}
				else
				{
					result.status = "rejected";
				}
				Reply(result);
				return false;
			}

			TELEPORT_INTERNAL_COUT(Default, "avatar manifest for client {}: resolved {} -> {} (outcome {})",
				clientID, core::RedactUrl(resolution.manifestUrl), core::RedactUrl(resolution.avatarUrl),
				resolution.receipt.outcome);

			offer.url = resolution.avatarUrl;

			if (onManifestProjected)
			{
				try
				{
					onManifestProjected(clientID, resolution.projection, resolution.receipt);
				}
				catch (const std::exception &e)
				{
					// A host callback must never be able to fail a client's
					// avatar; the manifest itself was fine.
					TELEPORT_INTERNAL_CERR("avatar manifest projection callback threw for client {}: {}", clientID, e.what());
				}
			}
			return true;
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
			lastManifestReceipt.reset();
			if (importer)
				importer->RemoveForClient(clientID);
		}

		void AvatarService::Reply(const core::AvatarResult &result)
		{
			core::AvatarResult withReceipt = result;
			// Carry the manifest receipt on whichever result this offer
			// produced, unless the caller supplied one explicitly.
			if (!withReceipt.manifest.has_value() && lastManifestReceipt.has_value())
				withReceipt.manifest = *lastManifestReceipt;
			lastResult = withReceipt;
			json content = withReceipt;
			TELEPORT_INTERNAL_COUT(Default, "avatar-result -> client {} status={} delivery={}"
				, clientID, result.status, result.delivery);
			if (sendFn)
				sendFn(Envelope(kSignalTypeAvatarResult, content));
		}
	}
}
