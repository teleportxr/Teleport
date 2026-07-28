#pragma once

#include <string>

// Redaction helpers for avatar URLs and proofs (plans/avatars_plan.md §8).
// An avatar URL may carry a bearer token or user identifier and a proof
// may encode user attributes, so any log line mentioning either must pass
// through these helpers. Mirrored by teleport-nodejs/utils/redact.js and
// teleport-web-client/src/log/redact.ts.

namespace teleport
{
	namespace core
	{
		//! Reduce a URL to scheme + host: "https://host.example/...".
		//! Anything unparseable is replaced wholesale so a malformed URL
		//! can never leak into a log.
		inline std::string RedactUrl(const std::string &url)
		{
			if (url.empty())
				return "<no-url>";
			const size_t schemeEnd = url.find("://");
			if (schemeEnd == std::string::npos || schemeEnd == 0)
			{
				// Server-relative paths ("/avatars/abc.glb") are not absolute
				// URLs but name our own resources, so they are safe — and
				// useful — to log. Any query or fragment is still stripped in
				// case it carries a credential. "//host/path" is protocol-
				// relative, i.e. a host, so it is not treated as a path.
				if (url[0] == '/' && url.compare(0, 2, "//") != 0)
					return url.substr(0, url.find_first_of("?#"));
				return "<invalid-url>";
			}
			const size_t hostStart = schemeEnd + 3;
			if (hostStart >= url.size())
				return "<invalid-url>";
			// Host ends at the first path/query/fragment delimiter. Any
			// userinfo before an '@' is part of what we must not echo.
			size_t hostEnd = url.find_first_of("/?#", hostStart);
			if (hostEnd == std::string::npos)
				hostEnd = url.size();
			std::string host = url.substr(hostStart, hostEnd - hostStart);
			const size_t at = host.rfind('@');
			if (at != std::string::npos)
				host = host.substr(at + 1);
			if (host.empty())
				return "<invalid-url>";
			return url.substr(0, schemeEnd) + "://" + host + "/...";
		}

		//! Describe a proof value without echoing it.
		inline std::string RedactProof(const std::string &value, const std::string &scheme = "proof")
		{
			return "<" + (scheme.empty() ? std::string("proof") : scheme) + " " + std::to_string(value.size()) + " bytes>";
		}
	}
}
