#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace teleport
{
	namespace client
	{
		//! The small amount of HTTP that OAuth needs, shared by the identity providers so that
		//! error handling and curl lifetime are dealt with in exactly one place.
		namespace oauth
		{
			//! curl is also initialised by libavstream's HTTPUtil; this does it once, and never
			//! calls curl_global_cleanup(), which would pull the rug from under any other user.
			void EnsureCurlInitialised();

			std::string UrlEncode(const std::string &s);

			//! POST a form and parse the JSON reply. Returns false for transport failures,
			//! non-JSON replies, and OAuth "error" responses alike, with a readable message in
			//! errorOut. responseOut is still filled in for an OAuth error response, so callers
			//! that care about the error code (device-flow polling) can read it.
			bool PostForm(const std::string &url, const std::string &postFields, nlohmann::json &responseOut, std::string &errorOut);

			//! GET with an OAuth bearer token, parsing the JSON reply.
			bool GetWithBearer(const std::string &url, const std::string &accessToken, nlohmann::json &responseOut, std::string &errorOut);

			//! The machine-readable OAuth error code from a response, e.g. "authorization_pending".
			std::string ErrorCode(const nlohmann::json &response);
		}
	}
}
