// JSON read/write for the avatar-negotiation messages declared in
// TeleportCore/Avatars.h. Kept out of the header so that translation
// units that only need the struct definitions don't pull in the full
// nlohmann/json template instantiations.

#include "TeleportCore/Avatars.h"

namespace teleport
{
	namespace core
	{
		// Small helpers ------------------------------------------------

		namespace
		{
			template <typename T>
			void putOpt(json &j, const char *key, const std::optional<T> &v)
			{
				if (v.has_value())
					j[key] = *v;
			}

			template <typename T>
			void getOpt(const json &j, const char *key, std::optional<T> &v)
			{
				v.reset();
				if (j.is_object() && j.contains(key) && !j.at(key).is_null())
					v = j.at(key).get<T>();
			}
		}

		// AvatarRequirements ------------------------------------------

		void to_json(json &j, const AvatarRequirements &r)
		{
			// Start from the extras bag so caller-supplied keys survive a
			// round-trip; first-class keys overwrite them below.
			j = r.extra.is_object() ? r.extra : json::object();
			if (!r.formats.empty())
				j["formats"] = r.formats;
			putOpt(j, "max_file_bytes", r.maxFileBytes);
			putOpt(j, "max_triangles", r.maxTriangles);
			putOpt(j, "max_height_m", r.maxHeightM);
			putOpt(j, "max_width_m", r.maxWidthM);
			putOpt(j, "max_textures", r.maxTextures);
			putOpt(j, "max_texture_pixels", r.maxTexturePixels);
			putOpt(j, "skeleton", r.skeleton);
			if (!r.licenceTagsAllowed.empty())
				j["licence_tags_allowed"] = r.licenceTagsAllowed;
		}

		void from_json(const json &j, AvatarRequirements &r)
		{
			r = AvatarRequirements{};
			if (!j.is_object())
				return;
			// Capture every key in `extra`; the typed fields below also
			// read from these so a parsed-then-emitted requirements blob
			// is byte-equivalent.
			r.extra = j;
			if (j.contains("formats") && j.at("formats").is_array())
				r.formats = j.at("formats").get<std::vector<std::string>>();
			getOpt(j, "max_file_bytes", r.maxFileBytes);
			getOpt(j, "max_triangles", r.maxTriangles);
			getOpt(j, "max_height_m", r.maxHeightM);
			getOpt(j, "max_width_m", r.maxWidthM);
			getOpt(j, "max_textures", r.maxTextures);
			getOpt(j, "max_texture_pixels", r.maxTexturePixels);
			getOpt(j, "skeleton", r.skeleton);
			if (j.contains("licence_tags_allowed") && j.at("licence_tags_allowed").is_array())
				r.licenceTagsAllowed = j.at("licence_tags_allowed").get<std::vector<std::string>>();
		}

		// AvatarProofPolicy / AvatarProofOffer / AvatarDeclared -------

		void to_json(json &j, const AvatarProofPolicy &p)
		{
			j = json{ { "required", p.required } };
			if (!p.acceptedSchemes.empty())
				j["accepted_schemes"] = p.acceptedSchemes;
		}

		void from_json(const json &j, AvatarProofPolicy &p)
		{
			p = AvatarProofPolicy{};
			if (!j.is_object())
				return;
			if (j.contains("required") && j.at("required").is_boolean())
				p.required = j.at("required").get<bool>();
			if (j.contains("accepted_schemes") && j.at("accepted_schemes").is_array())
				p.acceptedSchemes = j.at("accepted_schemes").get<std::vector<std::string>>();
		}

		void to_json(json &j, const AvatarProofOffer &p)
		{
			j = json{ { "scheme", p.scheme }, { "value", p.value } };
		}

		void from_json(const json &j, AvatarProofOffer &p)
		{
			p = AvatarProofOffer{};
			if (!j.is_object())
				return;
			if (j.contains("scheme")) p.scheme = j.at("scheme").get<std::string>();
			if (j.contains("value"))  p.value  = j.at("value").get<std::string>();
		}

		void to_json(json &j, const AvatarDeclared &d)
		{
			j = json{ { "format", d.format } };
			putOpt(j, "file_bytes", d.fileBytes);
			putOpt(j, "triangles", d.triangles);
		}

		void from_json(const json &j, AvatarDeclared &d)
		{
			d = AvatarDeclared{};
			if (!j.is_object())
				return;
			if (j.contains("format")) d.format = j.at("format").get<std::string>();
			getOpt(j, "file_bytes", d.fileBytes);
			getOpt(j, "triangles", d.triangles);
		}

		// AvatarPolicy ------------------------------------------------

		void to_json(json &j, const AvatarPolicy &p)
		{
			j = json{
				{ "policy_id",         p.policyId },
				{ "requirement",       p.requirement },
				{ "default_available", p.defaultAvailable },
				{ "requirements",      p.requirements },
				{ "proof",             p.proof }
			};
			putOpt(j, "fetch_timeout_ms", p.fetchTimeoutMs);
		}

		void from_json(const json &j, AvatarPolicy &p)
		{
			p = AvatarPolicy{};
			if (!j.is_object())
				return;
			if (j.contains("policy_id"))         p.policyId         = j.at("policy_id").get<avs::uid>();
			if (j.contains("requirement"))       p.requirement      = j.at("requirement").get<std::string>();
			if (j.contains("default_available")) p.defaultAvailable = j.at("default_available").get<bool>();
			if (j.contains("requirements"))      p.requirements     = j.at("requirements").get<AvatarRequirements>();
			if (j.contains("proof"))             p.proof            = j.at("proof").get<AvatarProofPolicy>();
			getOpt(j, "fetch_timeout_ms", p.fetchTimeoutMs);
		}

		// AvatarOffer -------------------------------------------------

		void to_json(json &j, const AvatarOffer &o)
		{
			j = json{
				{ "policy_id",   o.policyId },
				{ "have_avatar", o.haveAvatar }
			};
			putOpt(j, "url",          o.url);
			putOpt(j, "content_hash", o.contentHash);
			if (o.declared.has_value())
				j["declared"] = *o.declared;
			if (o.proof.has_value())
				j["proof"] = *o.proof;
			putOpt(j, "allow_relay", o.allowRelay);
		}

		void from_json(const json &j, AvatarOffer &o)
		{
			o = AvatarOffer{};
			if (!j.is_object())
				return;
			if (j.contains("policy_id"))   o.policyId   = j.at("policy_id").get<avs::uid>();
			if (j.contains("have_avatar")) o.haveAvatar = j.at("have_avatar").get<bool>();
			getOpt(j, "url",          o.url);
			getOpt(j, "content_hash", o.contentHash);
			if (j.contains("declared") && !j.at("declared").is_null())
				o.declared = j.at("declared").get<AvatarDeclared>();
			if (j.contains("proof") && !j.at("proof").is_null())
				o.proof = j.at("proof").get<AvatarProofOffer>();
			getOpt(j, "allow_relay", o.allowRelay);
		}

		// AvatarResult ------------------------------------------------

		void to_json(json &j, const AvatarResult &r)
		{
			j = json{
				{ "policy_id",     r.policyId },
				{ "status",        r.status },
				{ "node_uid",      r.nodeUid },
				{ "using_default", r.usingDefault },
				{ "delivery",      r.delivery },
				{ "reasons",       r.reasons }
			};
		}

		void from_json(const json &j, AvatarResult &r)
		{
			r = AvatarResult{};
			if (!j.is_object())
				return;
			if (j.contains("policy_id"))     r.policyId     = j.at("policy_id").get<avs::uid>();
			if (j.contains("status"))        r.status       = j.at("status").get<std::string>();
			if (j.contains("node_uid"))      r.nodeUid      = j.at("node_uid").get<avs::uid>();
			if (j.contains("using_default")) r.usingDefault = j.at("using_default").get<bool>();
			if (j.contains("delivery"))      r.delivery     = j.at("delivery").get<std::string>();
			if (j.contains("reasons") && j.at("reasons").is_array())
				r.reasons = j.at("reasons").get<std::vector<std::string>>();
		}

		// AvatarRevoke ------------------------------------------------

		void to_json(json &j, const AvatarRevoke &r)
		{
			j = json{ { "policy_id", r.policyId }, { "reason", r.reason } };
		}

		void from_json(const json &j, AvatarRevoke &r)
		{
			r = AvatarRevoke{};
			if (!j.is_object())
				return;
			if (j.contains("policy_id")) r.policyId = j.at("policy_id").get<avs::uid>();
			if (j.contains("reason"))    r.reason   = j.at("reason").get<std::string>();
		}

		// PeerAvatar --------------------------------------------------

		void to_json(json &j, const PeerAvatar &p)
		{
			j = json{
				{ "peer_client_id", p.peerClientId },
				{ "peer_node_uid",  p.peerNodeUid },
				{ "revoked",        p.revoked }
			};
			putOpt(j, "url",          p.url);
			putOpt(j, "content_hash", p.contentHash);
			putOpt(j, "format",       p.format);
			if (p.proof.has_value())
				j["proof"] = *p.proof;
		}

		void from_json(const json &j, PeerAvatar &p)
		{
			p = PeerAvatar{};
			if (!j.is_object())
				return;
			if (j.contains("peer_client_id")) p.peerClientId = j.at("peer_client_id").get<avs::uid>();
			if (j.contains("peer_node_uid"))  p.peerNodeUid  = j.at("peer_node_uid").get<avs::uid>();
			if (j.contains("revoked"))        p.revoked      = j.at("revoked").get<bool>();
			getOpt(j, "url",          p.url);
			getOpt(j, "content_hash", p.contentHash);
			getOpt(j, "format",       p.format);
			if (j.contains("proof") && !j.at("proof").is_null())
				p.proof = j.at("proof").get<AvatarProofOffer>();
		}

		// PeerAvatarFailed --------------------------------------------

		void to_json(json &j, const PeerAvatarFailed &p)
		{
			j = json{
				{ "peer_node_uid", p.peerNodeUid },
				{ "reason",        p.reason }
			};
		}

		void from_json(const json &j, PeerAvatarFailed &p)
		{
			p = PeerAvatarFailed{};
			if (!j.is_object())
				return;
			if (j.contains("peer_node_uid")) p.peerNodeUid = j.at("peer_node_uid").get<avs::uid>();
			if (j.contains("reason"))        p.reason      = j.at("reason").get<std::string>();
		}
	}
}
