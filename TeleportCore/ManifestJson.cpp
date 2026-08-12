// Universal Manifest envelope handling: parsing, projection accessors,
// consent gating and receipt composition. Mirrors
// teleport-nodejs/protocol/manifest.js, teleport-nodejs/manifest/consent.js
// and teleport-nodejs/manifest/receipt.js.
//
// Kept out of the header so translation units that only need the struct
// definitions don't pull in the full nlohmann/json template instantiations,
// matching AvatarsJson.cpp.

#include "TeleportCore/Manifest.h"

#include <algorithm>
#include <array>
#include <ctime>

namespace teleport
{
	namespace core
	{
		namespace
		{
			// Envelope members required by the v0.3 schema. `signature` is
			// included: an unsigned manifest cannot be verified, and this
			// implementation does not accept one.
			const std::array<const char *, 8> kRequiredMembers = {
				"@context", "@id", "@type", "manifestVersion", "subject", "issuedAt", "expiresAt", "signature"
			};

			constexpr const char *kStatusProcessed		= "processed";
			constexpr const char *kStatusOpaque			= "opaque";
			constexpr const char *kStatusConsentDenied	= "consent-denied";
			constexpr const char *kStatusConsentMissing	= "consent-missing";
			constexpr const char *kStatusNotProjected	= "not-projected";

			std::string StringMember(const json &j, const char *key)
			{
				if (!j.is_object() || !j.contains(key) || !j.at(key).is_string())
					return {};
				return j.at(key).get<std::string>();
			}

			std::string LowerCase(std::string s)
			{
				std::transform(s.begin(), s.end(), s.begin(),
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				return s;
			}

			//! Every consent entry that applies to any of `refs`. v0.3 entries
			//! reference a facet by facetRef, v0.1 entries by name.
			std::vector<const json *> ConsentsForRefs(const json &manifest, const std::vector<std::string> &refs)
			{
				std::vector<const json *> found;
				if (!manifest.is_object() || !manifest.contains("consents") || !manifest.at("consents").is_array())
					return found;
				for (const auto &consent : manifest.at("consents"))
				{
					if (!consent.is_object())
						continue;
					const std::string facetRef = StringMember(consent, "facetRef");
					const std::string name     = StringMember(consent, "name");
					const bool matches =
						(!facetRef.empty() && std::find(refs.begin(), refs.end(), facetRef) != refs.end()) ||
						(!name.empty()     && std::find(refs.begin(), refs.end(), name)     != refs.end());
					if (matches)
						found.push_back(&consent);
				}
				return found;
			}

			//! Evaluate one consent entry. Returns true to permit.
			bool EvaluateConsent(const json &consent, int64_t nowUnix,
				const std::vector<std::string> &requiredScope, const std::string &purpose,
				std::string &reasonOut, std::string &warningOut)
			{
				int64_t when = 0;

				// A withdrawn consent is treated as absent, not as a denial
				// that can be argued with.
				const std::string withdrawnAt = StringMember(consent, "withdrawnAt");
				if (!withdrawnAt.empty() && ParseRfc3339(withdrawnAt, when) && when <= nowUnix)
				{
					reasonOut = "withdrawn";
					return false;
				}

				const std::string grantedAt = StringMember(consent, "grantedAt");
				if (!grantedAt.empty() && ParseRfc3339(grantedAt, when) && when > nowUnix)
				{
					reasonOut = "not_yet_granted";
					return false;
				}

				const std::string expiresAt = StringMember(consent, "expiresAt");
				if (!expiresAt.empty())
				{
					if (!ParseRfc3339(expiresAt, when) || when <= nowUnix)
					{
						reasonOut = "expired";
						return false;
					}
				}

				// v0.1 flat permission.
				if (consent.contains("value") && consent.at("value").is_string())
				{
					const std::string value = LowerCase(consent.at("value").get<std::string>());
					if (value == "allowed")
						return true;
					if (value == "restricted")
					{
						warningOut = "consent_restricted";
						return true;
					}
					reasonOut = "denied";
					return false;
				}

				// v0.3 scope/purpose.
				if (!consent.contains("scope"))
				{
					// v0.3 makes scope required; a consent without one cannot
					// be shown to permit anything.
					reasonOut = "scope_not_permitted";
					return false;
				}
				std::vector<std::string> scope;
				if (consent.at("scope").is_array())
				{
					for (const auto &s : consent.at("scope"))
					{
						if (s.is_string())
							scope.push_back(LowerCase(s.get<std::string>()));
					}
				}
				else if (consent.at("scope").is_string())
				{
					scope.push_back(LowerCase(consent.at("scope").get<std::string>()));
				}
				for (const auto &needed : requiredScope)
				{
					if (std::find(scope.begin(), scope.end(), LowerCase(needed)) == scope.end())
					{
						reasonOut = "scope_not_permitted";
						return false;
					}
				}

				if (!purpose.empty() && consent.contains("purpose"))
				{
					std::vector<std::string> declared;
					if (consent.at("purpose").is_array())
					{
						for (const auto &p : consent.at("purpose"))
						{
							if (p.is_string())
								declared.push_back(p.get<std::string>());
						}
					}
					else if (consent.at("purpose").is_string())
					{
						declared.push_back(consent.at("purpose").get<std::string>());
					}
					if (std::find(declared.begin(), declared.end(), purpose) == declared.end())
					{
						reasonOut = "purpose_mismatch";
						return false;
					}
				}

				return true;
			}

			//! Shared body of GateFacet/GateReference once the applicable
			//! consents are known. Conjunctive: all must pass.
			std::string GateAgainst(const std::vector<const json *> &applicable, int64_t nowUnix,
				const std::vector<std::string> &requiredScope, const std::string &purpose,
				std::string &reasonOut, std::vector<std::string> &warningsOut)
			{
				for (const json *consent : applicable)
				{
					std::string reason;
					std::string warning;
					if (!EvaluateConsent(*consent, nowUnix, requiredScope, purpose, reason, warning))
					{
						reasonOut = reason;
						return kStatusConsentDenied;
					}
					if (!warning.empty() &&
						std::find(warningsOut.begin(), warningsOut.end(), warning) == warningsOut.end())
					{
						warningsOut.push_back(warning);
					}
				}
				return kStatusProcessed;
			}
		}

		// Arrive -------------------------------------------------------

		std::vector<std::string> ManifestTypeList(const json &value)
		{
			std::vector<std::string> out;
			if (value.is_string())
			{
				out.push_back(value.get<std::string>());
			}
			else if (value.is_array())
			{
				for (const auto &t : value)
				{
					if (t.is_string())
						out.push_back(t.get<std::string>());
				}
			}
			return out;
		}

		bool ManifestHasType(const json &value, const std::string &wanted)
		{
			const auto list = ManifestTypeList(value);
			return std::find(list.begin(), list.end(), wanted) != list.end();
		}

		bool ParseManifest(const std::string &text, const std::vector<std::string> &acceptedContexts,
			json &manifestOut, std::string &reasonOut)
		{
			json doc = json::parse(text, nullptr, /*allow_exceptions*/ false);
			if (doc.is_discarded() || !doc.is_object())
			{
				reasonOut = "manifest_malformed";
				return false;
			}

			for (const char *member : kRequiredMembers)
			{
				if (!doc.contains(member) || doc.at(member).is_null())
				{
					reasonOut = "manifest_malformed";
					return false;
				}
			}

			if (!ManifestHasType(doc.at("@type"), "um:Manifest"))
			{
				reasonOut = "manifest_malformed";
				return false;
			}

			// The accept list is the deployment's statement of which versions
			// it has been tested against. A manifest outside it is well-formed
			// but not something this server is willing to interpret.
			const std::vector<std::string> accepted =
				acceptedContexts.empty() ? std::vector<std::string>{ kManifestContextV03 } : acceptedContexts;
			bool contextOk = false;
			for (const auto &c : ManifestTypeList(doc.at("@context")))
			{
				if (std::find(accepted.begin(), accepted.end(), c) != accepted.end())
				{
					contextOk = true;
					break;
				}
			}
			if (!contextOk)
			{
				reasonOut = "manifest_context_not_accepted";
				return false;
			}

			manifestOut = std::move(doc);
			reasonOut.clear();
			return true;
		}

		// Pointers -----------------------------------------------------

		std::string PointerName(const json &pointer)
		{
			if (!pointer.is_object())
				return {};
			const std::string name = StringMember(pointer, "name");
			if (!name.empty())
				return name;
			return StringMember(pointer, "label");
		}

		std::string PointerTarget(const json &pointer)
		{
			if (!pointer.is_object())
				return {};
			const std::string target = StringMember(pointer, "target");
			if (!target.empty())
				return target;
			return StringMember(pointer, "url");
		}

		bool PointerMatches(const json &pointer, const std::string &name)
		{
			if (!pointer.is_object() || name.empty())
				return false;
			if (PointerName(pointer) == name)
				return true;
			return pointer.contains("@type") && ManifestHasType(pointer.at("@type"), name);
		}

		const json *FindPointer(const json &manifest, const std::vector<std::string> &names)
		{
			if (!manifest.is_object() || !manifest.contains("pointers") || !manifest.at("pointers").is_array())
				return nullptr;
			for (const auto &name : names)
			{
				if (name.empty())
					continue;
				for (const auto &pointer : manifest.at("pointers"))
				{
					if (PointerMatches(pointer, name))
						return &pointer;
				}
			}
			return nullptr;
		}

		// Facets -------------------------------------------------------

		std::string FacetName(const json &facet)
		{
			if (!facet.is_object())
				return {};
			const std::string name = StringMember(facet, "name");
			if (!name.empty())
				return name;
			return StringMember(facet, "@id");
		}

		std::vector<std::string> FacetRefs(const json &facet)
		{
			std::vector<std::string> refs;
			const std::string id = StringMember(facet, "@id");
			if (!id.empty())
				refs.push_back(id);
			const std::string name = FacetName(facet);
			if (!name.empty() && std::find(refs.begin(), refs.end(), name) == refs.end())
				refs.push_back(name);
			return refs;
		}

		bool FacetMatches(const json &facet, const std::string &name)
		{
			if (!facet.is_object() || name.empty())
				return false;
			const auto refs = FacetRefs(facet);
			if (std::find(refs.begin(), refs.end(), name) != refs.end())
				return true;
			// A facet's payload carries its own semantic type, e.g.
			// "xr:AvatarProfile"; let a deployment request by that too.
			if (facet.contains("@type") && ManifestHasType(facet.at("@type"), name))
				return true;
			if (facet.contains("entity") && facet.at("entity").is_object() &&
				facet.at("entity").contains("@type") && ManifestHasType(facet.at("entity").at("@type"), name))
			{
				return true;
			}
			return false;
		}

		bool IsSealedFacet(const json &facet)
		{
			if (!facet.is_object())
				return false;
			if (!StringMember(facet, "encryptionProfile").empty())
				return true;
			if (!facet.contains("entity") || !facet.at("entity").is_object())
				return false;
			const json &entity = facet.at("entity");
			return entity.contains("ciphertext") && entity.at("ciphertext").is_string() &&
				   entity.contains("protected") && entity.at("protected").is_string();
		}

		// Consent ------------------------------------------------------

		std::string GateFacet(const json &manifest, const json &facet, int64_t nowUnix,
			const std::vector<std::string> &requiredScope, const std::string &purpose,
			std::string &reasonOut, std::vector<std::string> &warningsOut)
		{
			reasonOut.clear();
			if (IsSealedFacet(facet))
				return kStatusOpaque;

			const auto applicable = ConsentsForRefs(manifest, FacetRefs(facet));
			if (applicable.empty())
				return kStatusConsentMissing;

			return GateAgainst(applicable, nowUnix, requiredScope, purpose, reasonOut, warningsOut);
		}

		std::string GateReference(const json &manifest, const std::string &name, int64_t nowUnix,
			const std::vector<std::string> &requiredScope, const std::string &purpose,
			std::string &reasonOut, std::vector<std::string> &warningsOut)
		{
			reasonOut.clear();
			const auto applicable = ConsentsForRefs(manifest, { name });
			if (applicable.empty())
				return kStatusProcessed;

			return GateAgainst(applicable, nowUnix, requiredScope, purpose, reasonOut, warningsOut);
		}

		// Compose ------------------------------------------------------

		void ComposeOutcome(ManifestReceipt &receipt, bool fatal)
		{
			if (fatal)
			{
				receipt.outcome = "rejected";
				return;
			}

			const bool degraded = std::any_of(receipt.facetStatuses.begin(), receipt.facetStatuses.end(),
				[](const ManifestFacetStatus &f)
				{
					return f.status == kStatusConsentDenied || f.status == kStatusConsentMissing ||
						   f.status == kStatusNotProjected || f.status == kStatusOpaque;
				});

			if (degraded)
				receipt.outcome = "accepted-partial";
			else if (!receipt.warnings.empty())
				receipt.outcome = "accepted-with-warnings";
			else
				receipt.outcome = "accepted";
		}

		uint32_t ManifestTrustTier(const json &value)
		{
			if (!value.is_number())
				return 0;
			const double n = value.get<double>();
			return n > 0 ? static_cast<uint32_t>(n) : 0u;
		}

		// Time ---------------------------------------------------------

		// Reads exactly `digits` decimal characters at `pos` and advances it.
		// RFC 3339 fixes the width of every field, so a short or padded field
		// is malformed - which sscanf's "%2d" would have silently accepted.
		static bool ReadFixedDigits(const std::string &text, size_t &pos, size_t digits, int &valueOut)
		{
			if (pos + digits > text.size())
				return false;
			int value = 0;
			for (size_t i = 0; i < digits; i++)
			{
				const char c = text[pos + i];
				if (c < '0' || c > '9')
					return false;
				value = value * 10 + (c - '0');
			}
			pos += digits;
			valueOut = value;
			return true;
		}

		static bool ReadSeparator(const std::string &text, size_t &pos, char expected)
		{
			if (pos >= text.size() || text[pos] != expected)
				return false;
			pos++;
			return true;
		}

		bool ParseRfc3339(const std::string &text, int64_t &unixSecondsOut)
		{
			// Deliberately hand-rolled rather than std::get_time: the manifest
			// times are always UTC RFC 3339 and this avoids both the locale
			// dependence and the timezone handling of the standard parsers,
			// neither of which we want deciding whether a manifest is expired.
			// Reading the digits directly rather than with sscanf also keeps
			// MSVC from deprecating the whole function (C4996).
			int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
			size_t pos = 0;
			if (!ReadFixedDigits(text, pos, 4, year) || !ReadSeparator(text, pos, '-')
				|| !ReadFixedDigits(text, pos, 2, month) || !ReadSeparator(text, pos, '-')
				|| !ReadFixedDigits(text, pos, 2, day) || !ReadSeparator(text, pos, 'T')
				|| !ReadFixedDigits(text, pos, 2, hour) || !ReadSeparator(text, pos, ':')
				|| !ReadFixedDigits(text, pos, 2, minute) || !ReadSeparator(text, pos, ':')
				|| !ReadFixedDigits(text, pos, 2, second))
				return false;
			if (month < 1 || month > 12 || day < 1 || day > 31)
				return false;

			// Days from the civil epoch (Howard Hinnant's algorithm).
			int y = year;
			y -= month <= 2;
			const int era = (y >= 0 ? y : y - 399) / 400;
			const unsigned yoe = static_cast<unsigned>(y - era * 400);
			const unsigned doy = static_cast<unsigned>((153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1);
			const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
			const int64_t days = static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;

			unixSecondsOut = days * 86400 + hour * 3600 + minute * 60 + second;

			// A trailing offset shifts the instant; 'Z' and a bare timestamp
			// are both taken as UTC.
			const size_t plus = text.find_last_of("+-");
			if (plus != std::string::npos && plus > 10)
			{
				int offHour = 0, offMinute = 0;
				size_t offPos = plus + 1;
				if (ReadFixedDigits(text, offPos, 2, offHour) && ReadSeparator(text, offPos, ':')
					&& ReadFixedDigits(text, offPos, 2, offMinute))
				{
					const int64_t offset = offHour * 3600 + offMinute * 60;
					unixSecondsOut += (text[plus] == '+') ? -offset : offset;
				}
			}
			return true;
		}

		// Codecs -------------------------------------------------------

		void to_json(json &j, const ManifestSignature &s)
		{
			j = json{
				{ "algorithm",        s.algorithm },
				{ "canonicalization", s.canonicalization },
				{ "keyRef",           s.keyRef },
				{ "value",            s.value }
			};
			if (s.publicKeySpkiB64.has_value())
				j["publicKeySpkiB64"] = *s.publicKeySpkiB64;
		}

		void from_json(const json &j, ManifestSignature &s)
		{
			s = ManifestSignature{};
			if (!j.is_object())
				return;
			s.algorithm        = StringMember(j, "algorithm");
			s.canonicalization = StringMember(j, "canonicalization");
			s.keyRef           = StringMember(j, "keyRef");
			s.value            = StringMember(j, "value");
			const std::string inlineKey = StringMember(j, "publicKeySpkiB64");
			if (!inlineKey.empty())
				s.publicKeySpkiB64 = inlineKey;
		}

		void to_json(json &j, const ManifestFacetStatus &f)
		{
			j = json{ { "name", f.name }, { "status", f.status } };
			if (!f.reason.empty())
				j["reason"] = f.reason;
		}

		void from_json(const json &j, ManifestFacetStatus &f)
		{
			f = ManifestFacetStatus{};
			if (!j.is_object())
				return;
			f.name   = StringMember(j, "name");
			f.status = StringMember(j, "status");
			f.reason = StringMember(j, "reason");
		}

		//! The compact, snake_case projection that travels on
		//! avatar-result.manifest. The full receipt stays server-side.
		void to_json(json &j, const ManifestReceipt &r)
		{
			json facets = json::array();
			for (const auto &f : r.facetStatuses)
				facets.push_back(json{ { "name", f.name }, { "status", f.status } });
			j = json{
				{ "manifest_id",     r.manifestId },
				{ "outcome",         r.outcome },
				{ "signature_check", r.signatureCheck },
				{ "freshness_check", r.freshnessCheck },
				{ "facets",          facets }
			};
		}

		void from_json(const json &j, ManifestReceipt &r)
		{
			r = ManifestReceipt{};
			if (!j.is_object())
				return;
			r.manifestId = StringMember(j, "manifest_id");
			if (!StringMember(j, "outcome").empty())
				r.outcome = StringMember(j, "outcome");
			if (!StringMember(j, "signature_check").empty())
				r.signatureCheck = StringMember(j, "signature_check");
			if (!StringMember(j, "freshness_check").empty())
				r.freshnessCheck = StringMember(j, "freshness_check");
			if (j.contains("facets") && j.at("facets").is_array())
			{
				for (const auto &f : j.at("facets"))
				{
					if (f.is_object())
						r.facetStatuses.push_back(f.get<ManifestFacetStatus>());
				}
			}
		}
	}
}
