#include "GoogleDeviceIdentityProvider.h"
#include "OAuthHttp.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#include "TokenStorage.h"

#include <chrono>
#include <iostream>
#include <thread>

// Supplied by the build system from the environment or a .env file; see
// TeleportClient/CMakeLists.txt. A device client id is registered in the Google Cloud console
// as a "TV and Limited Input device", which is a different client to the desktop one.
#ifndef TELEPORT_GOOGLE_DEVICE_CLIENT_ID
#define TELEPORT_GOOGLE_DEVICE_CLIENT_ID ""
#endif
#ifndef TELEPORT_GOOGLE_DEVICE_CLIENT_SECRET
#define TELEPORT_GOOGLE_DEVICE_CLIENT_SECRET ""
#endif

using namespace teleport;
using namespace client;
using nlohmann::json;

namespace
{
	constexpr const char *kBuildTimeClientId	 = TELEPORT_GOOGLE_DEVICE_CLIENT_ID;
	constexpr const char *kBuildTimeClientSecret = TELEPORT_GOOGLE_DEVICE_CLIENT_SECRET;
	constexpr const char *kDeviceCodeEndpoint	 = "https://oauth2.googleapis.com/device/code";
	constexpr const char *kTokenEndpoint		 = "https://oauth2.googleapis.com/token";
	constexpr const char *kUserInfoEndpoint		 = "https://www.googleapis.com/oauth2/v3/userinfo";
	constexpr const char *kGrantType			 = "urn:ietf:params:oauth:grant-type:device_code";
	//! Only email, profile and openid are permitted for the limited-input device flow.
	constexpr const char *kScope				 = "email profile";
	//! Separate from the desktop client's token: a refresh token is tied to its client id.
	constexpr const char *kSecretKey			 = "GoogleDeviceRefreshToken";
	//! Used if Google omits the interval, and as the increment demanded by a "slow_down".
	constexpr int		  kDefaultIntervalSeconds = 5;
}

GoogleDeviceIdentityProvider::GoogleDeviceIdentityProvider()
{
	clientId	 = kBuildTimeClientId;
	clientSecret = kBuildTimeClientSecret;
	oauth::EnsureCurlInitialised();
	if (clientId.empty())
	{
		TELEPORT_WARN("No Google device client id was configured at build time; Google sign-in is unavailable.");
	}
}

GoogleDeviceIdentityProvider::~GoogleDeviceIdentityProvider()
{
	cancelled = true;
}

bool GoogleDeviceIdentityProvider::IsAvailable() const
{
	return !clientId.empty();
}

void GoogleDeviceIdentityProvider::SetSignInPromptHandler(SignInPromptHandler handler)
{
	promptHandler = handler;
}

void GoogleDeviceIdentityProvider::SetError(const std::string &e)
{
	std::lock_guard<std::mutex> lock(errorMutex);
	lastError = e;
}

std::string GoogleDeviceIdentityProvider::GetLastError() const
{
	std::lock_guard<std::mutex> lock(errorMutex);
	return lastError;
}

bool GoogleDeviceIdentityProvider::RestoreSilent(IdentityProfile &profile)
{
	refreshToken = TokenStorage::LoadSecret(kSecretKey);
	if (refreshToken.empty())
		return false;
	if (!RefreshAccessToken())
		return false;
	if (!FetchUserInfo(profile))
		return false;
	TELEPORT_LOG("Google identity restored for {}", profile.displayName.empty() ? profile.subject : profile.displayName);
	return true;
}

bool GoogleDeviceIdentityProvider::SignInInteractive(IdentityProfile &profile)
{
	if (signInInProgress.exchange(true))
	{
		SetError("A sign-in is already in progress.");
		return false;
	}
	struct Guard
	{
		std::atomic<bool> &flag;
		~Guard()
		{
			flag = false;
		}
	} guard{signInInProgress};

	cancelled = false;
	SetError("");

	std::string deviceCode, userCode, verificationUrl;
	int			intervalSeconds = kDefaultIntervalSeconds;
	int			expiresInSeconds = 0;
	if (!RequestDeviceCode(deviceCode, userCode, verificationUrl, intervalSeconds, expiresInSeconds))
		return false;

	// Tell the user where to go. There is no browser here, so this is the whole point of the flow.
	// Always logged, because a service has no terminal in front of anyone; the handler is how a
	// client that is not a terminal — the GUI, or a machine client polling `identity` — gets it.
	TELEPORT_LOG("To sign in, visit {} on another device and enter the code: {}", verificationUrl, userCode);
	if (promptHandler)
	{
		SignInPrompt prompt;
		prompt.verificationUrl	= verificationUrl;
		prompt.userCode			= userCode;
		prompt.expiresInSeconds = expiresInSeconds;
		promptHandler(prompt);
	}

	if (!PollForToken(deviceCode, intervalSeconds, expiresInSeconds))
		return false;
	if (!refreshToken.empty())
	{
		// Remembered here so that later runs need no code and no second device.
		if (!TokenStorage::SaveSecret(kSecretKey, refreshToken))
			TELEPORT_WARN("Google sign-in succeeded but the refresh token could not be stored; sign-in will not be remembered.");
	}
	return FetchUserInfo(profile);
}

bool GoogleDeviceIdentityProvider::RequestDeviceCode(std::string &deviceCode,
													 std::string &userCode,
													 std::string &verificationUrl,
													 int		 &intervalSeconds,
													 int		 &expiresInSeconds)
{
	std::string postFields = "client_id=" + oauth::UrlEncode(clientId) + "&scope=" + oauth::UrlEncode(kScope);
	json		response;
	std::string error;
	if (!oauth::PostForm(kDeviceCodeEndpoint, postFields, response, error))
	{
		SetError(error);
		TELEPORT_WARN("Google device code request failed: {}", error);
		return false;
	}
	deviceCode		= response.value("device_code", "");
	userCode		= response.value("user_code", "");
	// Google's response field is verification_url; the RFC calls it verification_uri.
	verificationUrl = response.value("verification_url", response.value("verification_uri", ""));
	intervalSeconds = response.value("interval", kDefaultIntervalSeconds);
	expiresInSeconds = response.value("expires_in", 1800);
	if (deviceCode.empty() || userCode.empty() || verificationUrl.empty())
	{
		SetError("Google returned an incomplete device code response.");
		return false;
	}
	return true;
}

bool GoogleDeviceIdentityProvider::PollForToken(const std::string &deviceCode, int intervalSeconds, int expiresInSeconds)
{
	std::string postFields = "client_id=" + oauth::UrlEncode(clientId) + "&device_code=" + oauth::UrlEncode(deviceCode) +
							 "&grant_type=" + oauth::UrlEncode(kGrantType);
	if (!clientSecret.empty())
		postFields += "&client_secret=" + oauth::UrlEncode(clientSecret);

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(expiresInSeconds);
	if (intervalSeconds < 1)
		intervalSeconds = kDefaultIntervalSeconds;

	while (!cancelled)
	{
		// Wait first: the user cannot possibly have finished before the first interval.
		for (int i = 0; i < intervalSeconds * 10 && !cancelled; i++)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		if (cancelled)
			break;
		if (std::chrono::steady_clock::now() > deadline)
		{
			SetError("The sign-in code expired before it was used.");
			return false;
		}

		json		response;
		std::string error;
		if (oauth::PostForm(kTokenEndpoint, postFields, response, error))
		{
			accessToken = response.value("access_token", "");
			if (response.contains("refresh_token"))
				refreshToken = response["refresh_token"].get<std::string>();
			if (response.contains("expires_in"))
				tokenExpiry = std::time(nullptr) + response["expires_in"].get<int>();
			if (accessToken.empty())
			{
				SetError("Google returned no access token.");
				return false;
			}
			credentialsRevoked = false;
			return true;
		}

		const std::string code = oauth::ErrorCode(response);
		if (code == "authorization_pending")
			continue;
		if (code == "slow_down")
		{
			// Required behaviour: back off rather than keep to the original interval.
			intervalSeconds += kDefaultIntervalSeconds;
			continue;
		}
		if (code == "access_denied")
		{
			SetError("Sign-in was refused.");
			return false;
		}
		if (code == "expired_token")
		{
			SetError("The sign-in code expired before it was used.");
			return false;
		}
		if (clientSecret.empty() && error.find("client_secret") != std::string::npos)
		{
			error += " Set TELEPORT_GOOGLE_DEVICE_CLIENT_SECRET when building: Google requires the secret "
					 "on the device flow's polling request.";
		}
		SetError(error);
		TELEPORT_WARN("Google device sign-in failed: {}", error);
		return false;
	}
	SetError("Sign-in cancelled.");
	return false;
}

bool GoogleDeviceIdentityProvider::RefreshAccessToken()
{
	if (refreshToken.empty())
	{
		SetError("No stored credentials.");
		return false;
	}
	std::string postFields = "client_id=" + oauth::UrlEncode(clientId) + "&refresh_token=" + oauth::UrlEncode(refreshToken) + "&grant_type=refresh_token";
	if (!clientSecret.empty())
		postFields += "&client_secret=" + oauth::UrlEncode(clientSecret);

	json		response;
	std::string error;
	if (!oauth::PostForm(kTokenEndpoint, postFields, response, error))
	{
		SetError(error);
		TELEPORT_WARN("Google token refresh failed: {}", error);
		// Only a definitive rejection discards the sign-in; anything else may just be the network.
		if (oauth::ErrorCode(response) == "invalid_grant")
		{
			TELEPORT_WARN("Stored Google credentials have been revoked; signing out.");
			credentialsRevoked = true;
			SignOut();
		}
		return false;
	}
	credentialsRevoked = false;
	accessToken		   = response.value("access_token", "");
	if (response.contains("expires_in"))
		tokenExpiry = std::time(nullptr) + response["expires_in"].get<int>();
	return !accessToken.empty();
}

bool GoogleDeviceIdentityProvider::FetchUserInfo(IdentityProfile &profile)
{
	if (accessToken.empty())
	{
		SetError("No access token.");
		return false;
	}
	json		response;
	std::string error;
	if (!oauth::GetWithBearer(kUserInfoEndpoint, accessToken, response, error))
	{
		SetError(error);
		return false;
	}
	profile.provider	= GetName();
	// "sub" is the stable Google account id. The email address can change; the subject cannot.
	profile.subject		= response.value("sub", "");
	profile.displayName = response.value("name", "");
	profile.email		= response.value("email", "");
	profile.pictureUrl	= response.value("picture", "");
	profile.verifiedUnixTime = (int64_t)std::time(nullptr);
	if (!profile.IsValid())
	{
		SetError("Google returned no account identifier.");
		return false;
	}
	return true;
}

void GoogleDeviceIdentityProvider::CancelSignIn()
{
	cancelled = true;
}

void GoogleDeviceIdentityProvider::SignOut()
{
	TokenStorage::DeleteSecret(kSecretKey);
	refreshToken.clear();
	accessToken.clear();
	tokenExpiry = 0;
}
