#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "TeleportCore/Avatars.h"
#include "TeleportCore/Manifest.h"

namespace teleport
{
	namespace server
	{
		//! Outcome of resolving a manifest address to an avatar asset url.
		struct AvatarManifestResolution
		{
			bool                     ok = false;
			//! Machine-readable codes: manifest_unresolvable, manifest_malformed,
			//! manifest_context_not_accepted, manifest_signature_invalid,
			//! manifest_key_unresolvable, manifest_expired,
			//! manifest_trust_tier_unsupported, manifest_no_avatar_pointer,
			//! manifest_consent_missing, manifest_too_large — plus the shared
			//! transport vocabulary (ssrf_blocked, fetch_timeout, http_<n>, …)
			//! so there is one set of transport reasons, not two.
			std::vector<std::string> reasons;
			std::string              manifestUrl;
			//! The asset url read out of the manifest. This is what the rest
			//! of the pipeline sees; nothing downstream knows a manifest was
			//! involved.
			std::string              avatarUrl;
			std::string              subject;
			//! The app-specific facets and claims the deployment asked for,
			//! after consent gating. Handed to the host application.
			nlohmann::json           projection;
			core::ManifestReceipt    receipt;
			//! Manifest expiry, seconds since epoch; 0 when unknown.
			int64_t                  expiresAtUnix = 0;
		};

		//! Resolver for manifest addresses supplied in avatar-offer.
		//!
		//! Unlike IAvatarValidator — which has no library default because a
		//! conformant one needs a glTF parser and an asset pipeline —
		//! DefaultAvatarManifestResolver IS shipped here. Manifest resolution
		//! needs only an HTTPS fetch and a JSON parser, both of which
		//! TeleportServer already links, so there is no dependency argument
		//! for pushing the work onto every host.
		//!
		//! Implementations MUST be safe to call from arbitrary threads. The
		//! callback MAY be invoked synchronously (e.g. on a cache hit) or
		//! from a worker thread.
		class IAvatarManifestResolver
		{
		public:
			using Callback = std::function<void(const AvatarManifestResolution &)>;

			virtual ~IAvatarManifestResolver() = default;

			//! Resolve `address` against `requirements` — the manifest block
			//! of the originating avatar-policy — and invoke `cb`.
			virtual void Resolve(
				const core::AvatarManifestOffer        &address,
				const core::AvatarManifestRequirements &requirements,
				Callback                                cb) = 0;
		};
	}
}
