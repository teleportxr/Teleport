#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "TeleportServer/IAvatarManifestResolver.h"
#include "TeleportServer/ManifestCache.h"

namespace teleport
{
	namespace server
	{
		//! Result of one HTTPS GET. Pulled out as a seam so tests can drive
		//! the six evaluation stages without a network.
		struct ManifestFetchResult
		{
			bool        ok = false;
			//! One of the shared transport reason codes on failure.
			std::string reason;
			std::string body;
			std::string finalUrl;
		};

		//! Fetches a url subject to a byte cap and a wall-clock budget.
		using ManifestFetchFn = std::function<ManifestFetchResult(
			const std::string &url, uint64_t maxBytes, uint32_t timeoutMs)>;

		//! The library's own manifest resolver: address → HTTPS GET → the
		//! Universal Manifest six-stage evaluation sequence → asset url.
		//!
		//! Mirrors teleport-nodejs/manifest/resolver.js.
		//!
		//! The fetch deserves particular care. It is a server-side HTTPS
		//! request to a url the client chose, which is the same SSRF exposure
		//! the asset fetch has. CurlFetch installs a socket-level peer check
		//! rather than trusting CURLOPT_FOLLOWLOCATION, because a redirect is
		//! resolved inside libcurl where a scheme or host allow-list applied
		//! beforehand cannot see it.
		//! Namespace-scope rather than nested in the resolver so that the
		//! constructor below can default-argument it: a nested type's default
		//! member initialisers are not available inside the enclosing class's
		//! own definition.
		struct AvatarManifestResolverOptions
		{
			std::string              resolverBase = "https://myum.net/";
			std::vector<std::string> allowedSchemes = { "https" };
			//! Schemes an avatar ASSET url may use. Deliberately separate
			//! from the manifest's: they are different fetches, made by
			//! different components, at different times. This exists only
			//! to fail fast — the authoritative check is the host's
			//! IAvatarValidator when the asset is actually fetched.
			std::vector<std::string> assetAllowedSchemes = { "https" };
			//! A manifest is a small JSON document. A generous cap here
			//! would just be a free amplification primitive.
			uint64_t                 maxBytes = 256 * 1024;
			//! Separate from and additional to the asset fetch budget.
			uint32_t                 timeoutMs = 5000;
			uint32_t                 maxRedirects = 3;
			int64_t                  clockSkewSeconds = 60;
			//! Operations this server performs on a projected facet.
			std::vector<std::string> requiredScope = { "read" };
			//! Stated so a subject's consent can be scoped to it.
			std::string              purpose;
		};

		class DefaultAvatarManifestResolver : public IAvatarManifestResolver
		{
		public:
			using Options = AvatarManifestResolverOptions;

			explicit DefaultAvatarManifestResolver(const Options &options = Options());

			//! Substitute the transport, for tests.
			void SetFetcher(ManifestFetchFn fn)
			{
				fetch = std::move(fn);
			}

			void Resolve(
				const core::AvatarManifestOffer        &address,
				const core::AvatarManifestRequirements &requirements,
				Callback                                cb) override;

			//! Turn a manifest address into the url to GET. Exposed for testing.
			std::string AddressToUrl(const core::AvatarManifestOffer &address,
				const core::AvatarManifestRequirements &requirements) const;

			ManifestCache &Cache()
			{
				return cache;
			}

		private:
			AvatarManifestResolution Evaluate(
				const core::AvatarManifestOffer        &address,
				const core::AvatarManifestRequirements &requirements);

			Options         options;
			ManifestFetchFn fetch;
			ManifestCache   cache;
		};

		//! libcurl-backed HTTPS GET with the SSRF guards described above.
		//! Refuses private, loopback, link-local and metadata-service peers at
		//! connect time — which covers redirects and DNS rebinding, since the
		//! check runs on the socket rather than on the url.
		ManifestFetchResult CurlFetch(const std::string &url, uint64_t maxBytes, uint32_t timeoutMs,
			uint32_t maxRedirects, const std::vector<std::string> &allowedSchemes);

		//! True for addresses a server-side fetch must never be pointed at.
		//! Mirrors isBlockedIp in teleport-nodejs/client/avatar_validator.js.
		//! Fails closed: anything unparseable is blocked.
		//!
		//! Takes `const void *` rather than `const sockaddr *` deliberately.
		//! Naming `struct sockaddr` inside a namespace declares a NEW,
		//! incomplete type at namespace scope rather than referring to the
		//! global one, and pulling <winsock2.h> into a public header to avoid
		//! that would be a poor trade. Callers pass a sockaddr / sockaddr_in /
		//! sockaddr_in6; the implementation reads sa_family first.
		bool IsBlockedAddress(const void *address, size_t addressLength);
	}
}
