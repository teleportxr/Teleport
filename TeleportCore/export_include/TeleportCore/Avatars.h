#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "libavstream/common_exports.h"

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
		//! keys MUST be ignored by the peer.
		struct SignalingCapabilities
		{
			//! True if the client can fetch peer avatars directly from
			//! their host (relay mode); false means the server must
			//! import-and-stream peer avatars through the geometry pipe.
			bool avatarRelay = false;
		};

		inline void to_json(json &j, const SignalingCapabilities &c)
		{
			j = json{ { "avatar_relay", c.avatarRelay } };
		}

		inline void from_json(const json &j, SignalingCapabilities &c)
		{
			c = SignalingCapabilities{};
			if (j.is_object() && j.contains("avatar_relay") && j.at("avatar_relay").is_boolean())
				c.avatarRelay = j.at("avatar_relay").get<bool>();
		}

		// ---------------------------------------------------------------
		// Shared sub-structures
		// ---------------------------------------------------------------

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
			std::string              delivery = "import";
			std::vector<std::string> reasons;
		};

		//! server -> client: avatar-revoke
		struct AvatarRevoke
		{
			avs::uid    policyId = 0;
			std::string reason;
		};

		//! server -> peer client: peer-avatar (relay mode)
		struct PeerAvatar
		{
			avs::uid                        peerClientId = 0;
			avs::uid                        peerNodeUid = 0;
			std::optional<std::string>      url;
			std::optional<std::string>      contentHash;
			std::optional<std::string>      format;
			std::optional<AvatarProofOffer> proof;
			bool                            revoked = false;
		};

		//! peer client -> server: peer-avatar-failed (relay mode)
		struct PeerAvatarFailed
		{
			avs::uid    peerNodeUid = 0;
			std::string reason;
		};

		// JSON codecs are implemented in TeleportCore/AvatarsJson.cpp.
		// Declared here so argument-dependent lookup in nlohmann::json's
		// templated get<T>() / assignment finds them in every TU that
		// includes this header.
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
		void to_json(json &j, const PeerAvatar &p);
		void from_json(const json &j, PeerAvatar &p);
		void to_json(json &j, const PeerAvatarFailed &p);
		void from_json(const json &j, PeerAvatarFailed &p);
	}
}
