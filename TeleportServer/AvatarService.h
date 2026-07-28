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

			avs::uid                          clientID = 0;
			SendFn                            sendFn;
			IAvatarImporter                  *importer = nullptr;
			std::optional<core::AvatarPolicy> currentPolicy;
			std::optional<core::AvatarOffer>  lastOffer;
			std::optional<core::AvatarResult> lastResult;
		};
	}
}
