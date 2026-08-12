#pragma once

#include <functional>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "TeleportServer/Export.h"
#include "TeleportCore/Avatars.h"

namespace teleport
{
	namespace server
	{
		class IAvatarImporter;
		class IAvatarManifestResolver;

		//! Per-client server-side state for avatar negotiation. Phases 2–4
		//! of the implementation in plans/avatars_implementation.md:
		//! round-trip a policy, receive an offer, reply with using_default.
		//! Validation and download stay host-side (IAvatarValidator); when
		//! an IAvatarImporter is supplied, the default-avatar reply carries
		//! the real node uid of the imported scene node instead of 0.
		//!
		//! One AvatarService is owned by each ClientData; incoming
		//! `avatar-offer` and `avatar-revoke` JSON text frames are dispatched
		//! in from the signaling layer.
		class TELEPORT_SERVER_API AvatarService
		{
		public:
			using SendFn = std::function<void(const std::string &)>;

			AvatarService(avs::uid clientID, SendFn sendFn);

			//! Supply the host application's importer (may be nullptr to
			//! clear). Not owned; must outlive this service.
			void SetImporter(IAvatarImporter *i)
			{
				importer = i;
			}

			//! Supply the manifest resolver (may be nullptr to clear). Not
			//! owned; must outlive this service. With none set, an offer
			//! carrying only a manifest address is treated as an offer of
			//! nothing — which is correct for a deployment that has not
			//! advertised manifest support in its policy either.
			void SetManifestResolver(IAvatarManifestResolver *r)
			{
				manifestResolver = r;
			}

			//! Called with the app-specific facets a resolved manifest
			//! carried, after consent gating. Only facets this server named
			//! in requirements.manifest.requested_facets ever reach it.
			using ManifestProjectionFn = std::function<void(avs::uid clientID, const nlohmann::json &projection,
				const core::ManifestReceipt &receipt)>;

			void SetOnManifestProjected(ManifestProjectionFn fn)
			{
				onManifestProjected = std::move(fn);
			}

			//! Send (or re-send) the policy to the owning client. The
			//! client is expected to reply with an avatar-offer.
			void SendPolicy(const core::AvatarPolicy &policy);

			//! Handle an incoming avatar-offer. Phase 2 always replies
			//! using_default for any offer that targets the current policy.
			void HandleOffer(const nlohmann::json &content);

			//! Handle a client-initiated revoke (rare in Phase 2; provided
			//! for symmetry with later phases).
			void HandleRevoke(const nlohmann::json &content);

			bool HasCurrentPolicy() const
			{
				return currentPolicy.has_value();
			}

			const std::optional<core::AvatarPolicy> &GetCurrentPolicy() const
			{
				return currentPolicy;
			}

			const std::optional<core::AvatarOffer> &GetLastOffer() const
			{
				return lastOffer;
			}

			const std::optional<core::AvatarResult> &GetLastResult() const
			{
				return lastResult;
			}

		private:
			void Reply(const core::AvatarResult &result);

			//! Resolve offer.manifest to an asset url, writing it into
			//! offer.url so the rest of HandleOffer is unaware a manifest was
			//! involved. Returns false when it has already replied.
			bool ResolveManifest(core::AvatarOffer &offer);

			avs::uid                          clientID = 0;
			SendFn                            sendFn;
			IAvatarImporter                  *importer = nullptr;
			IAvatarManifestResolver          *manifestResolver = nullptr;
			ManifestProjectionFn              onManifestProjected;
			std::optional<core::AvatarPolicy> currentPolicy;
			std::optional<core::AvatarOffer>  lastOffer;
			std::optional<core::AvatarResult> lastResult;
			//! Receipt from this offer's manifest evaluation, attached to
			//! whichever avatar-result we end up sending. Cleared per offer so
			//! a later offer never inherits an earlier one's receipt.
			std::optional<core::ManifestReceipt> lastManifestReceipt;
		};
	}
}
