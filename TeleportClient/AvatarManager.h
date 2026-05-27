#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "TeleportCore/Avatars.h"

namespace teleport
{
	namespace client
	{
		//! Per-server client-side state for avatar negotiation. Mirror of
		//! teleport::server::AvatarService. Phase 2 of the implementation in
		//! plans/avatars_implementation.md: receive an avatar-policy, hand it
		//! to the host application's callback, send back an avatar-offer.
		//!
		//! One AvatarManager is owned by each SessionClient. Incoming
		//! `avatar-policy`, `avatar-result` and `avatar-revoke` JSON text
		//! frames are dispatched in from SessionClient::Frame after the
		//! DiscoveryService has parsed them off the signaling WebSocket.
		class AvatarManager
		{
		public:
			using SendFn = std::function<void(const std::string &)>;

			//! Reply callback supplied to the host application's PolicyCallback.
			//! The host application calls this with the AvatarOffer it wants
			//! to send back to the server. It may be called synchronously
			//! from inside the PolicyCallback, or asynchronously later.
			using ReplyFn = std::function<void(const core::AvatarOffer &)>;

			//! Invoked when an avatar-policy arrives. The host application
			//! decides what to offer and invokes `reply` with the offer.
			//! The default implementation replies with have_avatar=false,
			//! which lets the server fall back to its default avatar.
			using PolicyCallback = std::function<void(const core::AvatarPolicy &policy, ReplyFn reply)>;

			//! Invoked when an avatar-result arrives. Optional; default no-op.
			using ResultCallback = std::function<void(const core::AvatarResult &result)>;

			//! Invoked when an avatar-revoke arrives. Optional; default no-op.
			using RevokeCallback = std::function<void(const core::AvatarRevoke &revoke)>;

			AvatarManager(avs::uid serverID, SendFn sendFn);

			//! Host-supplied policy handler. Pass an empty std::function to
			//! restore the default behaviour (have_avatar=false).
			void SetOnAvatarPolicy(PolicyCallback cb);
			void SetOnAvatarResult(ResultCallback cb);
			void SetOnAvatarRevoke(RevokeCallback cb);

			//! Dispatch an incoming signaling text frame. Returns true if the
			//! frame was an avatar-* message handled by this manager, false
			//! otherwise (caller should forward to the streaming pipeline).
			bool HandleSignalingMessage(const std::string &msg);

			//! Send (or re-send) an avatar-offer to the server. Normally
			//! invoked via the ReplyFn passed to the PolicyCallback, but
			//! exposed publicly so the host application can refresh its
			//! offer at any time.
			void SendOffer(const core::AvatarOffer &offer);

			//! Send an avatar-revoke from this client to the server,
			//! invalidating the previously-offered avatar.
			void SendRevoke(const core::AvatarRevoke &revoke);

			bool HasCurrentPolicy() const;
			std::optional<core::AvatarPolicy>  GetCurrentPolicy() const;
			std::optional<core::AvatarOffer>   GetLastOffer() const;
			std::optional<core::AvatarResult>  GetLastResult() const;

		private:
			void HandlePolicy(const nlohmann::json &content);
			void HandleResult(const nlohmann::json &content);
			void HandleRevoke(const nlohmann::json &content);

			mutable std::mutex                mutex;
			avs::uid                          serverID = 0;
			SendFn                            sendFn;
			PolicyCallback                    onAvatarPolicy;
			ResultCallback                    onAvatarResult;
			RevokeCallback                    onAvatarRevoke;
			std::optional<core::AvatarPolicy> currentPolicy;
			std::optional<core::AvatarOffer>  lastOffer;
			std::optional<core::AvatarResult> lastResult;
		};
	}
}
