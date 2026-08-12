#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "libavstream/common_exports.h"
#include "TeleportCore/Manifest.h"

// Avatar-negotiation signaling messages and the session-level capability
// bag carried on the existing `connect` signal. Wire format is documented
// in Teleport/docs/protocol/signaling.rst.
//
// All structs are plain C++ (not packed binary): every avatar message
// travels as a JSON text frame on the signaling WebSocket, so there is
// no binary protocol change. ToJson / FromJson helpers below mirror the
// wire format byte-for-byte.

namespace teleport
{
	namespace core
	{
		using json = nlohmann::json;

		// ---------------------------------------------------------------
		// connect.content.capabilities
		// ---------------------------------------------------------------

		//! Session-level capability bag advertised by the client in the
		//! `connect` signaling message. Free-form on the wire — unknown
		//! keys MUST be ignored by the peer. It is an extension point for
		//! future signaling-level capabilities; none are defined at
		//! present. Avatars deliberately need none: an avatar reaches a
		//! client as an ordinary mesh pointer, which every client can
		//! already fetch (plans/avatars_plan.md D7).
		struct SignalingCapabilities
		{
		};

		inline void to_json(json &j, const SignalingCapabilities &)
		{
			j = json::object();
		}

		inline void from_json(const json &, SignalingCapabilities &c)
		{
			c = SignalingCapabilities{};
		}

		// ---------------------------------------------------------------
		// Shared sub-structures
		// ---------------------------------------------------------------

		//! Universal Manifest support, advertised inside the requirements
		//! bag. Its PRESENCE is how a client learns this server will accept
		//! a manifest address in place of a bare asset url, so a deployment
		//! that cannot resolve one must not send it.
		//!
		//! `requestedFacets` / `requestedClaims` are how a deployment
		//! declares the app-, game- or service-specific elements it
		//! understands. A server never projects a facet it did not ask for.
		//! See Teleport/docs/protocol/avatar_manifest.rst.
		struct AvatarManifestRequirements
		{
			//! Accepted @context values, e.g. "https://universalmanifest.net/ns/v0.3".
			std::vector<std::string> accepted;
			//! Pointer names that may hold the avatar asset, in precedence order.
			std::vector<std::string> avatarPointers;
			std::vector<std::string> requestedFacets;
			std::vector<std::string> requestedClaims;
			std::optional<uint32_t>  requiredTrustTier;
			//! A manifest is a small JSON document; this is deliberately
			//! orders of magnitude below maxFileBytes.
			std::optional<uint64_t>  maxBytes;
			//! Base url a bare UMID is resolved against.
			std::optional<std::string> resolver;
			json                     extra;
		};

		//! Free-form per-policy requirements bag. First-class fields are
		//! the ones every reference implementation understands; the
		//! `extra` map carries any additional keys the server set so they
		//! are preserved on the wire after a round-trip.
		struct AvatarRequirements
		{
			std::vector<std::string> formats;
			std::optional<uint64_t>  maxFileBytes;
			std::optional<uint32_t>  maxTriangles;
			std::optional<float>     maxHeightM;
			std::optional<float>     maxWidthM;
			std::optional<uint16_t>  maxTextures;
			std::optional<uint64_t>  maxTexturePixels;
			std::optional<std::string> skeleton;
			std::vector<std::string> licenceTagsAllowed;
			//! First-class rather than left to `extra` so the C++ server can
			//! construct a manifest-accepting policy without hand-building
			//! json. Absent means manifests are not accepted.
			std::optional<AvatarManifestRequirements> manifest;
			json                     extra;
		};

		struct AvatarProofPolicy
		{
			bool                     required = false;
			std::vector<std::string> acceptedSchemes;
		};

		struct AvatarProofOffer
		{
			std::string scheme;
			std::string value;
		};

		struct AvatarDeclared
		{
			std::string             format;
			std::optional<uint64_t> fileBytes;
			std::optional<uint32_t> triangles;
		};

		//! A manifest address offered in place of a direct asset url.
		//! Either an absolute https `url` or a `umid` resolved against the
		//! resolver named in the policy; `pointer` optionally names which
		//! pointer in the manifest holds the avatar.
		struct AvatarManifestOffer
		{
			std::optional<std::string> url;
			std::optional<std::string> umid;
			std::optional<std::string> pointer;
		};

		// ---------------------------------------------------------------
		// Messages
		// ---------------------------------------------------------------

		//! server -> client: avatar-policy
		struct AvatarPolicy
		{
			avs::uid                       policyId = 0;
			std::string                    requirement = "optional";
			bool                           defaultAvailable = false;
			AvatarRequirements             requirements;
			AvatarProofPolicy              proof;
			std::optional<uint32_t>        fetchTimeoutMs;
		};

		//! client -> server: avatar-offer
		struct AvatarOffer
		{
			avs::uid                       policyId = 0;
			bool                           haveAvatar = false;
			std::optional<std::string>     url;
			std::optional<std::string>     contentHash;
			//! When present, this is resolved to an asset url and takes
			//! precedence over `url`. `contentHash` and `declared` continue
			//! to describe the ASSET, not the manifest.
			std::optional<AvatarManifestOffer> manifest;
			std::optional<AvatarDeclared>  declared;
			std::optional<AvatarProofOffer> proof;
			std::optional<bool>            allowRelay;
		};

		//! server -> client: avatar-result
		struct AvatarResult
		{
			avs::uid                 policyId = 0;
			std::string              status = "rejected";
			avs::uid                 nodeUid = 0;
			bool                     usingDefault = false;
			//! "relay" (the default) — peers were given the owner's own url;
			//! "import" — the server re-hosted the asset. Informational: it
			//! says nothing about any other client's avatar.
			std::string              delivery = "relay";
			std::vector<std::string> reasons;
			//! The Universal Manifest receipt, present only when a manifest
			//! was evaluated. A conformant evaluator owes the subject an
			//! honest record of what it did with their document.
			std::optional<ManifestReceipt> manifest;
		};

		//! server -> client: avatar-revoke
		struct AvatarRevoke
		{
			avs::uid    policyId = 0;
			std::string reason;
		};

		// There are deliberately no peer-facing avatar messages. A client is
		// only ever told about its own avatar; another client's arrives as an
		// ordinary node carrying a mesh pointer, through the geometry
		// pipeline (plans/avatars_plan.md §2.2).

		// JSON codecs are implemented in TeleportCore/AvatarsJson.cpp.
		// Declared here so argument-dependent lookup in nlohmann::json's
		// templated get<T>() / assignment finds them in every TU that
		// includes this header.
		void to_json(json &j, const AvatarManifestRequirements &m);
		void from_json(const json &j, AvatarManifestRequirements &m);
		void to_json(json &j, const AvatarManifestOffer &m);
		void from_json(const json &j, AvatarManifestOffer &m);
		void to_json(json &j, const AvatarRequirements &r);
		void from_json(const json &j, AvatarRequirements &r);
		void to_json(json &j, const AvatarProofPolicy &p);
		void from_json(const json &j, AvatarProofPolicy &p);
		void to_json(json &j, const AvatarProofOffer &p);
		void from_json(const json &j, AvatarProofOffer &p);
		void to_json(json &j, const AvatarDeclared &d);
		void from_json(const json &j, AvatarDeclared &d);
		void to_json(json &j, const AvatarPolicy &p);
		void from_json(const json &j, AvatarPolicy &p);
		void to_json(json &j, const AvatarOffer &o);
		void from_json(const json &j, AvatarOffer &o);
		void to_json(json &j, const AvatarResult &r);
		void from_json(const json &j, AvatarResult &r);
		void to_json(json &j, const AvatarRevoke &r);
		void from_json(const json &j, AvatarRevoke &r);
	}
}
