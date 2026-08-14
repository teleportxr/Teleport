#include "TeleportServer/DefaultAvatarManifestResolver.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <curl/curl.h>

#include "TeleportServer/ManifestVerify.h"

#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Redact.h"

using nlohmann::json;

namespace teleport
{
	namespace server
	{
		namespace
		{
			//! libcurl is initialised once per process and deliberately never
			//! cleaned up: libavstream shares the handle, so tearing it down
			//! here would pull the rug from under the streaming pipeline.
			//! Same reasoning as TeleportClient/OAuthHttp.cpp.
			void EnsureCurlInitialised()
			{
				static std::once_flag once;
				std::call_once(once, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });
			}

			struct FetchState
			{
				std::string body;
				uint64_t    maxBytes = 0;
				bool        tooLarge = false;
			};

			size_t WriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
			{
				FetchState *state = static_cast<FetchState *>(userdata);
				const size_t total = size * nmemb;
				if (state->body.size() + total > state->maxBytes)
				{
					// Returning short aborts the transfer mid-stream, so a
					// hostile origin cannot trickle gigabytes at us.
					state->tooLarge = true;
					return 0;
				}
				state->body.append(ptr, total);
				return total;
			}

			//! Set when the SSRF check refused a peer, so the caller can tell
			//! a blocked address apart from an ordinary connection failure.
			struct AddressGuard
			{
				bool blocked = false;
			};

			std::string LowerCase(std::string s)
			{
				std::transform(s.begin(), s.end(), s.begin(),
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				return s;
			}

			std::string UrlScheme(const std::string &url)
			{
				const size_t colon = url.find(':');
				if (colon == std::string::npos)
					return {};
				return LowerCase(url.substr(0, colon));
			}

			//! Percent-encode a path segment so a UMID cannot escape into the
			//! surrounding url. Everything outside the RFC 3986 unreserved set
			//! is escaped.
			std::string EncodePathSegment(const std::string &s)
			{
				static const char *hex = "0123456789ABCDEF";
				std::string out;
				out.reserve(s.size());
				for (const unsigned char c : s)
				{
					const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
											(c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
					if (unreserved)
					{
						out.push_back(static_cast<char>(c));
					}
					else
					{
						out.push_back('%');
						out.push_back(hex[c >> 4]);
						out.push_back(hex[c & 0x0F]);
					}
				}
				return out;
			}

			//! Resolve a possibly-relative asset reference against the
			//! manifest url, so a manifest may point at an asset hosted
			//! beside it. Only the cases a manifest actually uses are handled:
			//! absolute, protocol-relative, root-relative and plain relative.
			std::string ResolveRelativeUrl(const std::string &base, const std::string &reference)
			{
				if (reference.empty())
					return {};
				if (reference.find("://") != std::string::npos)
					return reference;

				const size_t schemeEnd = base.find("://");
				if (schemeEnd == std::string::npos)
					return reference;
				const size_t authorityStart = schemeEnd + 3;
				const size_t pathStart = base.find('/', authorityStart);
				const std::string origin = pathStart == std::string::npos ? base : base.substr(0, pathStart);

				if (reference.rfind("//", 0) == 0)
					return base.substr(0, schemeEnd + 1) + reference;
				if (reference[0] == '/')
					return origin + reference;

				std::string directory = pathStart == std::string::npos ? origin + "/" : base.substr(0, base.rfind('/') + 1);
				return directory + reference;
			}

			int64_t NowUnix()
			{
				return static_cast<int64_t>(std::time(nullptr));
			}

			std::string StringMember(const json &j, const char *key)
			{
				if (!j.is_object() || !j.contains(key) || !j.at(key).is_string())
					return {};
				return j.at(key).get<std::string>();
			}
		}

		bool IsBlockedAddress(const void *addressBytes, size_t addressLength)
		{
			if (!addressBytes || addressLength < sizeof(::sockaddr))
				return true;

			const ::sockaddr *address = static_cast<const ::sockaddr *>(addressBytes);

			if (address->sa_family == AF_INET && addressLength >= sizeof(::sockaddr_in))
			{
				const ::sockaddr_in *v4 = reinterpret_cast<const ::sockaddr_in *>(address);
				const uint32_t host = ntohl(v4->sin_addr.s_addr);
				const uint8_t o0 = static_cast<uint8_t>(host >> 24);
				const uint8_t o1 = static_cast<uint8_t>((host >> 16) & 0xFF);
				const uint8_t o2 = static_cast<uint8_t>((host >> 8) & 0xFF);

				if (o0 == 0)								return true;	// "this" network
				if (o0 == 10)								return true;	// RFC1918
				if (o0 == 127)								return true;	// loopback
				if (o0 == 169 && o1 == 254)					return true;	// link-local + cloud metadata
				if (o0 == 172 && o1 >= 16 && o1 <= 31)		return true;	// RFC1918
				if (o0 == 192 && o1 == 168)					return true;	// RFC1918
				if (o0 == 192 && o1 == 0 && o2 == 0)		return true;	// IETF protocol assignments
				if (o0 == 198 && (o1 == 18 || o1 == 19))	return true;	// benchmarking
				if (o0 >= 224)								return true;	// multicast + reserved
				return false;
			}

			if (address->sa_family == AF_INET6 && addressLength >= sizeof(::sockaddr_in6))
			{
				const ::sockaddr_in6 *v6 = reinterpret_cast<const ::sockaddr_in6 *>(address);
				const uint8_t *b = v6->sin6_addr.s6_addr;

				bool allZero = true;
				for (int i = 0; i < 16; i++)
				{
					if (b[i] != 0)
					{
						allZero = false;
						break;
					}
				}
				if (allZero)												return true;	// ::
				// ::1
				bool loopback = b[15] == 1;
				for (int i = 0; i < 15 && loopback; i++)
					loopback = b[i] == 0;
				if (loopback)												return true;
				if (b[0] == 0xFE && (b[1] & 0xC0) == 0x80)					return true;	// fe80::/10 link-local
				if ((b[0] & 0xFE) == 0xFC)									return true;	// fc00::/7 ULA
				if (b[0] == 0xFF)											return true;	// multicast
				// ::ffff:0:0/96 v4-mapped would otherwise bypass the v4 list.
				{
					bool mapped = b[10] == 0xFF && b[11] == 0xFF;
					for (int i = 0; i < 10 && mapped; i++)
						mapped = b[i] == 0;
					if (mapped)
					{
						::sockaddr_in inner{};
						inner.sin_family = AF_INET;
						std::memcpy(&inner.sin_addr.s_addr, b + 12, 4);
						return IsBlockedAddress(&inner, sizeof(inner));
					}
				}
				return false;
			}

			// Unix sockets and anything else: fail closed.
			return true;
		}

		namespace
		{
			//! CURLOPT_OPENSOCKETFUNCTION gives us the resolved address before
			//! the connection is made, which is where the SSRF check belongs:
			//! it sees redirect hops and it sees what DNS actually returned,
			//! so a rebind between check and connect cannot slip past.
			curl_socket_t OpenSocketCallback(void *clientp, curlsocktype /*purpose*/, struct curl_sockaddr *address)
			{
				AddressGuard *guard = static_cast<AddressGuard *>(clientp);
				if (IsBlockedAddress(&address->addr, static_cast<size_t>(address->addrlen)))
				{
					if (guard)
						guard->blocked = true;
					return CURL_SOCKET_BAD;
				}
				return socket(address->family, address->socktype, address->protocol);
			}
		}

		ManifestFetchResult CurlFetch(const std::string &url, uint64_t maxBytes, uint32_t timeoutMs,
			uint32_t maxRedirects, const std::vector<std::string> &allowedSchemes)
		{
			ManifestFetchResult result;

			const std::string scheme = UrlScheme(url);
			if (std::find(allowedSchemes.begin(), allowedSchemes.end(), scheme) == allowedSchemes.end())
			{
				result.reason = "scheme_not_allowed";
				return result;
			}

			EnsureCurlInitialised();
			CURL *curl = curl_easy_init();
			if (!curl)
			{
				result.reason = "manifest_unresolvable";
				return result;
			}

			FetchState state;
			state.maxBytes = maxBytes;
			AddressGuard guard;

			std::string schemeList;
			for (const auto &s : allowedSchemes)
				schemeList += (schemeList.empty() ? "" : ",") + s;

			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, static_cast<long>(maxRedirects));
			curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeoutMs));
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state);
			curl_easy_setopt(curl, CURLOPT_OPENSOCKETFUNCTION, OpenSocketCallback);
			curl_easy_setopt(curl, CURLOPT_OPENSOCKETDATA, &guard);
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "teleportxr-manifest-resolver/1");
			curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
			curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
#if defined(CURLOPT_PROTOCOLS_STR)
			curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, schemeList.c_str());
			curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, schemeList.c_str());
#else
			// Older libcurl: the bitmask form. Kept so the resolver still
			// builds against the vendored curl if it lags.
			long protocols = 0;
			for (const auto &s : allowedSchemes)
			{
				if (s == "https") protocols |= CURLPROTO_HTTPS;
				else if (s == "http") protocols |= CURLPROTO_HTTP;
			}
			curl_easy_setopt(curl, CURLOPT_PROTOCOLS, protocols);
			curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, protocols);
#endif

			const CURLcode code = curl_easy_perform(curl);
			long status = 0;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
			char *effective = nullptr;
			curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective);
			if (effective)
				result.finalUrl = effective;
			curl_easy_cleanup(curl);

			if (guard.blocked)
			{
				result.reason = "ssrf_blocked";
				return result;
			}
			if (state.tooLarge)
			{
				result.reason = "manifest_too_large";
				return result;
			}
			if (code == CURLE_OPERATION_TIMEDOUT)
			{
				result.reason = "fetch_timeout";
				return result;
			}
			if (code == CURLE_TOO_MANY_REDIRECTS)
			{
				result.reason = "too_many_redirects";
				return result;
			}
			if (code != CURLE_OK)
			{
				result.reason = "manifest_unresolvable";
				return result;
			}
			if (status != 200)
			{
				result.reason = "http_" + std::to_string(status);
				return result;
			}

			result.ok = true;
			result.body = std::move(state.body);
			return result;
		}

		DefaultAvatarManifestResolver::DefaultAvatarManifestResolver(const Options &opts)
			: options(opts)
		{
			const uint32_t maxRedirects = options.maxRedirects;
			const std::vector<std::string> schemes = options.allowedSchemes;
			fetch = [maxRedirects, schemes](const std::string &url, uint64_t maxBytes, uint32_t timeoutMs)
			{
				return CurlFetch(url, maxBytes, timeoutMs, maxRedirects, schemes);
			};
		}

		std::string DefaultAvatarManifestResolver::AddressToUrl(
			const core::AvatarManifestOffer &address, const core::AvatarManifestRequirements &requirements) const
		{
			if (address.url.has_value() && !address.url->empty())
				return *address.url;
			if (!address.umid.has_value() || address.umid->empty())
				return {};

			std::string base = requirements.resolver.value_or(options.resolverBase);
			if (base.empty())
				base = options.resolverBase;
			if (base.back() != '/')
				base += '/';

			// The resolver contract accepts a UMID either url-encoded or
			// prefixed with `b64u:`; a UMID already carrying the prefix keeps
			// it, and only the payload after it is escaped.
			const std::string &umid = *address.umid;
			static const std::string b64uPrefix = "b64u:";
			if (umid.rfind(b64uPrefix, 0) == 0)
				return base + b64uPrefix + EncodePathSegment(umid.substr(b64uPrefix.size()));
			return base + EncodePathSegment(umid);
		}

		void DefaultAvatarManifestResolver::Resolve(
			const core::AvatarManifestOffer        &address,
			const core::AvatarManifestRequirements &requirements,
			Callback                                cb)
		{
			AvatarManifestResolution resolution = Evaluate(address, requirements);
			if (cb)
				cb(resolution);
		}

		AvatarManifestResolution DefaultAvatarManifestResolver::Evaluate(
			const core::AvatarManifestOffer &address, const core::AvatarManifestRequirements &requirements)
		{
			AvatarManifestResolution out;
			const int64_t now = NowUnix();

			out.manifestUrl = AddressToUrl(address, requirements);
			if (out.manifestUrl.empty())
			{
				out.reasons = { "manifest_unresolvable" };
				return out;
			}

			const std::string scheme = UrlScheme(out.manifestUrl);
			if (std::find(options.allowedSchemes.begin(), options.allowedSchemes.end(), scheme) == options.allowedSchemes.end())
			{
				out.reasons = { "scheme_not_allowed" };
				return out;
			}

			// A cached evaluation is reusable only while the manifest itself
			// is still valid.
			AvatarManifestResolution cached;
			if (cache.Get(out.manifestUrl, now, cached))
				return cached;

			// A policy may tighten the byte cap but never loosen it.
			const uint64_t maxBytes = requirements.maxBytes.has_value()
				? std::min<uint64_t>(*requirements.maxBytes, options.maxBytes)
				: options.maxBytes;

			const ManifestFetchResult fetched = fetch(out.manifestUrl, maxBytes, options.timeoutMs);
			if (!fetched.ok)
			{
				out.reasons = { fetched.reason.empty() ? "manifest_unresolvable" : fetched.reason };
				TELEPORT_INTERNAL_COUT(Default, "avatar manifest {} rejected: {}",
					core::RedactUrl(out.manifestUrl), out.reasons.front());
				return out;
			}

			// Stage 1 — Arrive.
			json manifest;
			std::string reason;
			if (!core::ParseManifest(fetched.body, requirements.accepted, manifest, reason))
			{
				out.reasons = { reason };
				return out;
			}

			out.receipt.manifestId = StringMember(manifest, "@id");
			out.subject            = StringMember(manifest, "subject");

			// Stage 2 — Verify. Authenticity and freshness only: `subject` is
			// recorded but never compared to the connecting client, which is
			// the deferred ownership-proof work.
			const ManifestVerifyResult verified = VerifyManifest(manifest, now, options.clockSkewSeconds);
			out.receipt.signatureCheck = verified.signatureCheck;
			out.receipt.freshnessCheck = verified.freshnessCheck;
			if (verified.signatureCheck != "valid" || verified.freshnessCheck != "fresh")
			{
				out.reasons = verified.reasons.empty() ? std::vector<std::string>{ "manifest_signature_invalid" } : verified.reasons;
				core::ComposeOutcome(out.receipt, true);
				return out;
			}

			// Trust tier, raise-only. The policy floor and the manifest floor
			// both apply; an unsupported tier is never quietly downgraded.
			uint32_t manifestTier = requirements.requiredTrustTier.value_or(0);
			if (manifest.contains("requiredTrustTier"))
				manifestTier = std::max(manifestTier, core::ManifestTrustTier(manifest.at("requiredTrustTier")));
			if (manifestTier > core::kSupportedTrustTier)
			{
				out.reasons = { "manifest_trust_tier_unsupported" };
				core::ComposeOutcome(out.receipt, true);
				return out;
			}

			// Stage 3 — Project. The avatar pointer first: without it there is
			// no avatar and nothing else matters.
			std::vector<std::string> pointerNames;
			auto addName = [&pointerNames](const std::string &name)
			{
				if (!name.empty() && std::find(pointerNames.begin(), pointerNames.end(), name) == pointerNames.end())
					pointerNames.push_back(name);
			};
			addName(address.pointer.value_or(std::string()));
			for (const auto &name : requirements.avatarPointers)
				addName(name);
			addName(core::kDefaultAvatarPointer);

			const json *pointer = core::FindPointer(manifest, pointerNames);
			if (!pointer)
			{
				out.reasons = { "manifest_no_avatar_pointer" };
				core::ComposeOutcome(out.receipt, true);
				return out;
			}

			// Stage 4 — Consent, for the pointer.
			std::string pointerName = core::PointerName(*pointer);
			if (pointerName.empty() && !pointerNames.empty())
				pointerName = pointerNames.front();

			std::string gateReason;
			std::vector<std::string> warnings;
			const std::string pointerStatus = core::GateReference(manifest, pointerName, now,
				options.requiredScope, options.purpose, gateReason, warnings);
			if (pointerStatus != "processed")
			{
				out.receipt.AddFacet(pointerName, pointerStatus, gateReason);
				out.reasons = { "manifest_consent_missing" };
				core::ComposeOutcome(out.receipt, true);
				return out;
			}
			for (const auto &w : warnings)
			{
				if (std::find(out.receipt.warnings.begin(), out.receipt.warnings.end(), w) == out.receipt.warnings.end())
					out.receipt.warnings.push_back(w);
			}

			out.avatarUrl = ResolveRelativeUrl(out.manifestUrl, core::PointerTarget(*pointer));
			const std::string assetScheme = UrlScheme(out.avatarUrl);
			if (out.avatarUrl.empty() ||
				std::find(options.assetAllowedSchemes.begin(), options.assetAllowedSchemes.end(), assetScheme) == options.assetAllowedSchemes.end())
			{
				out.avatarUrl.clear();
				out.reasons = { "manifest_no_avatar_pointer" };
				core::ComposeOutcome(out.receipt, true);
				return out;
			}

			// Stages 3 and 4 for the app-specific facets the deployment asked
			// for. Failures here are never fatal: a subject withholding a
			// loadout facet should still get their avatar.
			json facets = json::array();
			if (manifest.contains("facets") && manifest.at("facets").is_array())
			{
				for (const auto &facet : manifest.at("facets"))
				{
					const std::string name = core::FacetName(facet);
					const bool requested = std::any_of(requirements.requestedFacets.begin(), requirements.requestedFacets.end(),
						[&facet](const std::string &wanted) { return core::FacetMatches(facet, wanted); });
					if (!requested)
					{
						// Present in the manifest, deliberately not read.
						// A different statement from absent, and the receipt
						// says so.
						out.receipt.AddFacet(name, "not-projected");
						continue;
					}

					uint32_t facetTier = manifestTier;
					if (facet.is_object() && facet.contains("requiredTrustTier"))
						facetTier = std::max(facetTier, core::ManifestTrustTier(facet.at("requiredTrustTier")));
					if (facetTier > core::kSupportedTrustTier)
					{
						out.receipt.AddFacet(name, "not-projected", "trustTierUnsupported");
						continue;
					}

					std::string facetReason;
					std::vector<std::string> facetWarnings;
					const std::string status = core::GateFacet(manifest, facet, now,
						options.requiredScope, options.purpose, facetReason, facetWarnings);
					out.receipt.AddFacet(name, status, facetReason);
					for (const auto &w : facetWarnings)
					{
						if (std::find(out.receipt.warnings.begin(), out.receipt.warnings.end(), w) == out.receipt.warnings.end())
							out.receipt.warnings.push_back(w);
					}
					if (status == "processed")
					{
						json entry = json::object();
						entry["name"] = name;
						entry["entity"] = (facet.is_object() && facet.contains("entity")) ? facet.at("entity") : json();
						facets.push_back(entry);
					}
				}
			}

			json claims = json::array();
			if (!requirements.requestedClaims.empty() && manifest.contains("claims") && manifest.at("claims").is_array())
			{
				for (const auto &claim : manifest.at("claims"))
				{
					const std::string name = StringMember(claim, "name");
					if (!name.empty() &&
						std::find(requirements.requestedClaims.begin(), requirements.requestedClaims.end(), name) != requirements.requestedClaims.end())
					{
						claims.push_back(claim);
					}
				}
			}

			// Stages 5 and 6 — Compose and Receipt.
			core::ComposeOutcome(out.receipt, false);

			out.projection = json::object();
			out.projection["subject"] = out.subject;
			out.projection["pointer"] = pointerName;
			out.projection["facets"]  = facets;
			out.projection["claims"]  = claims;

			int64_t expires = 0;
			if (core::ParseRfc3339(StringMember(manifest, "expiresAt"), expires))
				out.expiresAtUnix = expires;
			out.ok = true;

			if (out.expiresAtUnix > now)
				cache.Set(out.manifestUrl, out, fetched.body.size());

			return out;
		}
	}
}
