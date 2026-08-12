#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

//! Universal Manifest (https://universalmanifest.net) envelope handling.
//!
//! A Universal Manifest is a portable, signed, consented JSON-LD document.
//! Teleport uses one as an indirection in front of an avatar: rather than
//! offering an asset url, a client offers a manifest address, and the server
//! reads the asset url out of the manifest along with whatever app-specific
//! facets that deployment understands.
//!
//! Mirrors teleport-nodejs/protocol/manifest.js. Target version is v0.3, the
//! stable line, tolerating v0.4 and unknown members.
//!
//! The manifest itself is deliberately kept as a `json` rather than being
//! unpacked into structs. Two reasons, both load-bearing rather than
//! stylistic: unknown members must be preserved exactly (they are covered by
//! the signature, so losing one turns a valid document into an invalid one),
//! and the same member is spelled differently across spec versions — v0.1
//! pointers are `{name, url}` while v0.3 pointers are `{@type, target}`, and
//! both are seen in the wild. Free accessor functions absorb that variance in
//! one place. Only the two things that cross a boundary — the signature we
//! verify and the receipt we emit — are typed.

namespace teleport
{
	namespace core
	{
		using json = nlohmann::json;

		//! @context values this implementation was written against. A
		//! deployment narrows or widens the accepted set through
		//! avatar-policy.requirements.manifest.accepted.
		constexpr const char *kManifestContextV03 = "https://universalmanifest.net/ns/v0.3";
		constexpr const char *kManifestContextV04 = "https://universalmanifest.net/ns/v0.4";

		//! The pointer name the published Universal Manifest XR fixtures use
		//! for an avatar asset. Used when a deployment names none.
		constexpr const char *kDefaultAvatarPointer = "portableIdentity.avatar";

		//! The highest trust tier this implementation can actually verify.
		//! Tier 0 is signature-only, which is what ManifestVerify does. Tier 1
		//! needs verifiable presentations or cross-DID binding, tier 2 a ZKP
		//! profile, tier 3 a multi-party ceremony. Claiming support for any of
		//! them would be worse than admitting we have none.
		constexpr uint32_t kSupportedTrustTier = 0;

		//! Signature Profile A. The only profile supported: JCS-RFC8785
		//! canonicalisation of the manifest minus `signature`, Ed25519.
		struct ManifestSignature
		{
			std::string                algorithm;			//!< must be "Ed25519"
			std::string                canonicalization;	//!< must be "JCS-RFC8785"
			std::string                keyRef;
			std::string                value;				//!< base64url
			std::optional<std::string> publicKeySpkiB64;
		};

		//! One entry of a receipt's per-facet record.
		struct ManifestFacetStatus
		{
			std::string name;
			//! processed | opaque | consent-denied | consent-missing | not-projected
			std::string status;
			std::string reason;
		};

		//! The structured receipt a conformant evaluator owes the subject: an
		//! honest record of what it did with their document, including the
		//! facets it chose not to read. A compact projection of this travels
		//! back to the client as avatar-result.manifest.
		struct ManifestReceipt
		{
			std::string                      manifestId;
			//! accepted | accepted-with-warnings | accepted-partial | rejected
			std::string                      outcome = "rejected";
			//! valid | invalid | unsupported-profile
			std::string                      signatureCheck = "unsupported-profile";
			//! fresh | expired | stale
			std::string                      freshnessCheck = "expired";
			std::vector<ManifestFacetStatus> facetStatuses;
			std::vector<std::string>         warnings;

			void AddFacet(const std::string &name, const std::string &status, const std::string &reason = {})
			{
				facetStatuses.push_back(ManifestFacetStatus{ name, status, reason });
			}
		};

		//! Stage 1, Arrive: structural check only. No signature, no freshness,
		//! no consent. `reason` is set to one of the avatar-result reason codes
		//! on failure. The document is left untouched on success — every member
		//! survives, including the ones we do not understand.
		bool ParseManifest(const std::string &text, const std::vector<std::string> &acceptedContexts,
			json &manifestOut, std::string &reasonOut);

		//! @type / @context may each be a bare string or an array of them.
		std::vector<std::string> ManifestTypeList(const json &value);
		bool                     ManifestHasType(const json &value, const std::string &wanted);

		//! Pointer accessors, tolerating both the v0.1 ({name, url}) and v0.3
		//! ({@type, target}) shapes.
		std::string PointerName(const json &pointer);
		std::string PointerTarget(const json &pointer);
		bool        PointerMatches(const json &pointer, const std::string &name);

		//! First pointer matching any of `names`, tried in order so a caller
		//! can express precedence. Returns nullptr when none matches.
		const json *FindPointer(const json &manifest, const std::vector<std::string> &names);

		//! Facet accessors. A facet is identified for consent purposes by its
		//! @id where it has one and by its name otherwise.
		std::string              FacetName(const json &facet);
		std::vector<std::string> FacetRefs(const json &facet);
		bool                     FacetMatches(const json &facet, const std::string &name);

		//! A facet whose payload is a JWE we have no key for. Acknowledged in
		//! the receipt as `opaque` and skipped — never grounds to reject the
		//! manifest.
		bool IsSealedFacet(const json &facet);

		//! Stage 4, Consent. Default-deny: a facet with no consent entry
		//! naming it is `consent-missing` and its data MUST NOT be processed.
		//! Returns a receipt facet status. `nowUnix` is seconds since epoch.
		std::string GateFacet(const json &manifest, const json &facet, int64_t nowUnix,
			const std::vector<std::string> &requiredScope, const std::string &purpose,
			std::string &reasonOut, std::vector<std::string> &warningsOut);

		//! Gate a named reference that is not a facet — the avatar pointer.
		//! An unstated reference is permitted: consent in UM is defined over
		//! facets, and demanding one per pointer would reject every manifest
		//! in the wild. Where the subject HAS spoken about it, we obey.
		std::string GateReference(const json &manifest, const std::string &name, int64_t nowUnix,
			const std::vector<std::string> &requiredScope, const std::string &purpose,
			std::string &reasonOut, std::vector<std::string> &warningsOut);

		//! Stage 5, Compose. `fatal` marks something that stopped evaluation
		//! outright; otherwise the outcome is graded by what happened to the
		//! facets, because partial acceptance is the normal case rather than
		//! an error.
		void ComposeOutcome(ManifestReceipt &receipt, bool fatal);

		//! Trust tiers are raise-only: a facet floor can lift the requirement
		//! above the manifest's, never below it.
		uint32_t ManifestTrustTier(const json &value);

		//! Parse an RFC 3339 timestamp to seconds since epoch. Returns false
		//! when it cannot be read, which callers treat as expired.
		bool ParseRfc3339(const std::string &text, int64_t &unixSecondsOut);

		// JSON codecs, implemented in TeleportCore/ManifestJson.cpp. Declared
		// here so ADL finds them in every TU that includes this header.
		void to_json(json &j, const ManifestSignature &s);
		void from_json(const json &j, ManifestSignature &s);
		void to_json(json &j, const ManifestFacetStatus &f);
		void from_json(const json &j, ManifestFacetStatus &f);
		void to_json(json &j, const ManifestReceipt &r);
		void from_json(const json &j, ManifestReceipt &r);
	}
}
