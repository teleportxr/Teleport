#include "GoogleIdentityProvider.h"
#include "OAuthHttp.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#include "TokenStorage.h"

#include <chrono>
#include <httplib.h>
#include <mutex>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <random>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

using namespace teleport;
using namespace client;
using nlohmann::json;

// Supplied by the build system from the environment or a .env file, so that no credential is
// readable in the installed application's config folder. Defaulted here so the file still
// compiles in a build that does not define them; Google sign-in is then simply unavailable.
#ifndef TELEPORT_GOOGLE_CLIENT_ID
#define TELEPORT_GOOGLE_CLIENT_ID ""
#endif
#ifndef TELEPORT_GOOGLE_CLIENT_SECRET
#define TELEPORT_GOOGLE_CLIENT_SECRET ""
#endif

namespace
{
	constexpr const char *kBuildTimeClientId	 = TELEPORT_GOOGLE_CLIENT_ID;
	//! Empty for a "Desktop app" client id, which is the arrangement PKCE is designed for.
	constexpr const char *kBuildTimeClientSecret = TELEPORT_GOOGLE_CLIENT_SECRET;
	//! Must match a redirect URI registered for the client id above.
	constexpr int		  kRedirectPort		= 8085;
	constexpr const char *kRedirectUri		= "http://localhost:8085/callback";
	constexpr const char *kTokenEndpoint	= "https://oauth2.googleapis.com/token";
	constexpr const char *kUserInfoEndpoint	= "https://www.googleapis.com/oauth2/v3/userinfo";
	//! Where the refresh token lives in the OS credential store.
	constexpr const char *kSecretKey		= "GoogleRefreshToken";
	constexpr auto		  kSignInTimeout	= std::chrono::minutes(2);

}

GoogleIdentityProvider::GoogleIdentityProvider()
{
	LoadClientConfig();
	oauth::EnsureCurlInitialised();
}

//! The OAuth client credentials are supplied at build time (see TeleportClient/CMakeLists.txt,
//! which reads TELEPORT_GOOGLE_CLIENT_ID and TELEPORT_GOOGLE_CLIENT_SECRET from the environment
//! or from an uncommitted .env file). They are deliberately not read from any file the user can
//! open. Note that a client secret compiled into a distributed binary is obfuscated rather than
//! protected: a client id registered as a "Desktop app" needs no secret at all, which is why
//! that registration is the one to use.
void GoogleIdentityProvider::LoadClientConfig()
{
	clientId	 = kBuildTimeClientId;
	clientSecret = kBuildTimeClientSecret;
	if (clientId.empty())
	{
		TELEPORT_WARN("No Google client id was configured at build time; Google sign-in is unavailable.");
	}
}

GoogleIdentityProvider::~GoogleIdentityProvider()
{
	StopLocalServer();
}

bool GoogleIdentityProvider::IsAvailable() const
{
	return !clientId.empty();
}

void GoogleIdentityProvider::SetError(const std::string &e)
{
	std::lock_guard<std::mutex> lock(errorMutex);
	lastError = e;
}

std::string GoogleIdentityProvider::GetLastError() const
{
	std::lock_guard<std::mutex> lock(errorMutex);
	return lastError;
}

bool GoogleIdentityProvider::RestoreSilent(IdentityProfile &profile)
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

bool GoogleIdentityProvider::SignInInteractive(IdentityProfile &profile)
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

	cancelled		 = false;
	authCode.clear();
	authCodeReceived = false;
	SetError("");

	GenerateCodeVerifier();
	GenerateCodeChallenge();
	StartLocalServer();

	// access_type=offline with prompt=consent so that Google returns a refresh token, which is
	// what lets every later launch skip the browser entirely. The consent screen is only seen
	// here, on an explicitly requested sign-in.
	std::string authUrl = "https://accounts.google.com/o/oauth2/auth?client_id=" + clientId + "&redirect_uri=" + kRedirectUri +
						  "&response_type=code&scope=openid%20email%20profile&code_challenge=" + codeChallenge +
						  "&code_challenge_method=S256&access_type=offline&prompt=consent";
	OpenBrowser(authUrl);

	bool gotCode = false;
	{
		std::unique_lock<std::mutex> lock(mutex);
		gotCode = cv.wait_for(lock, kSignInTimeout, [this] { return authCodeReceived || cancelled.load(); });
		gotCode = gotCode && authCodeReceived;
	}
	StopLocalServer();

	if (!gotCode)
	{
		if (cancelled)
			SetError("Sign-in cancelled.");
		else if (GetLastError().empty())
			SetError("Sign-in timed out.");
		return false;
	}
	if (!ExchangeCodeForTokens())
		return false;
	if (!refreshToken.empty())
	{
		// Remembered here so that later launches need neither the browser nor this exchange.
		if (!TokenStorage::SaveSecret(kSecretKey, refreshToken))
			TELEPORT_WARN("Google sign-in succeeded but the refresh token could not be stored; sign-in will not be remembered.");
	}
	return FetchUserInfo(profile);
}

void GoogleIdentityProvider::CancelSignIn()
{
	cancelled = true;
	{
		std::lock_guard<std::mutex> lock(mutex);
		cv.notify_all();
	}
	StopLocalServer();
}

void GoogleIdentityProvider::SignOut()
{
	TokenStorage::DeleteSecret(kSecretKey);
	refreshToken.clear();
	accessToken.clear();
	tokenExpiry = 0;
}

bool GoogleIdentityProvider::RefreshAccessToken()
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
		// A revoked or expired grant will never succeed again: forget it rather than retrying on
		// every launch. Any other failure — no network, Google unreachable — leaves the stored
		// credentials alone, so a sign-in survives being offline.
		if (oauth::ErrorCode(response) == "invalid_grant")
		{
			TELEPORT_WARN("Stored Google credentials have been revoked; signing out.");
			credentialsRevoked = true;
			SignOut();
		}
		return false;
	}
	credentialsRevoked = false;
	accessToken = response.value("access_token", "");
	if (response.contains("expires_in"))
		tokenExpiry = std::time(nullptr) + response["expires_in"].get<int>();
	return !accessToken.empty();
}

bool GoogleIdentityProvider::ExchangeCodeForTokens()
{
	// Under PKCE the code verifier takes the place of a client secret; a client id registered as
	// a "Web application" nevertheless demands the secret as well, so send it when we have one.
	std::string postFields = "code=" + oauth::UrlEncode(authCode) + "&client_id=" + oauth::UrlEncode(clientId) +
							 "&code_verifier=" + oauth::UrlEncode(codeVerifier) + "&redirect_uri=" + oauth::UrlEncode(kRedirectUri) +
							 "&grant_type=authorization_code";
	if (!clientSecret.empty())
		postFields += "&client_secret=" + oauth::UrlEncode(clientSecret);

	json		response;
	std::string error;
	if (!oauth::PostForm(kTokenEndpoint, postFields, response, error))
	{
		if (clientSecret.empty() && error.find("client_secret") != std::string::npos)
		{
			// Actionable: this is a registration problem, not something the user did wrong.
			error += " This Google client id is registered as a Web application. Either register a Desktop app "
					 "client id, or set TELEPORT_GOOGLE_CLIENT_SECRET when building.";
		}
		SetError(error);
		TELEPORT_WARN("Google token exchange failed: {}", error);
		return false;
	}
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
	return true;
}

bool GoogleIdentityProvider::FetchUserInfo(IdentityProfile &profile)
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

void GoogleIdentityProvider::GenerateCodeVerifier()
{
	std::string						charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
	std::random_device				rd;
	std::mt19937					gen(rd());
	std::uniform_int_distribution<> dis(0, (int)charset.size() - 1);

	codeVerifier.clear();
	for (int i = 0; i < 96; ++i) // Must be between 43 and 128 characters.
	{
		codeVerifier += charset[dis(gen)];
	}
}

void GoogleIdentityProvider::GenerateCodeChallenge()
{
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256(reinterpret_cast<const unsigned char *>(codeVerifier.c_str()), codeVerifier.length(), hash);

	// Base64url encode the hash.
	BIO		*bio, *b64;
	BUF_MEM *bufferPtr;
	b64 = BIO_new(BIO_f_base64());
	bio = BIO_new(BIO_s_mem());
	bio = BIO_push(b64, bio);
	BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
	BIO_write(bio, hash, (int)SHA256_DIGEST_LENGTH);
	BIO_flush(bio);
	BIO_get_mem_ptr(bio, &bufferPtr);
	std::string base64(bufferPtr->data, bufferPtr->length);
	BIO_free_all(bio);

	std::replace(base64.begin(), base64.end(), '+', '-');
	std::replace(base64.begin(), base64.end(), '/', '_');
	base64.erase(std::remove(base64.begin(), base64.end(), '='), base64.end());
	codeChallenge = base64;
}

void GoogleIdentityProvider::StartLocalServer()
{
	std::shared_ptr<httplib::Server> s = std::make_shared<httplib::Server>();
	{
		std::lock_guard<std::mutex> lock(serverMutex);
		server = s;
	}

	s->Get("/callback",
		   [this](const httplib::Request &req, httplib::Response &res)
		   {
			   if (req.has_param("code"))
			   {
				   {
					   std::lock_guard<std::mutex> lock(mutex);
					   authCode			= req.get_param_value("code");
					   authCodeReceived = true;
				   }
				   cv.notify_all();
				   res.set_content("<html><head><title>Signed in</title></head>"
								   "<body style='font-family: Arial, sans-serif; text-align: center; padding-top: 50px;'>"
								   "<h1>Signed in</h1>"
								   "<p>You can close this window and return to Teleport.</p>"
								   "<script>window.setTimeout(function() { window.close(); }, 3000);</script>"
								   "</body></html>",
								   "text/html");
			   }
			   else if (req.has_param("error"))
			   {
				   SetError(req.get_param_value("error"));
				   {
					   std::lock_guard<std::mutex> lock(mutex);
					   authCodeReceived = false;
				   }
				   cancelled = true;
				   cv.notify_all();
				   res.set_content("<html><head><title>Sign-in failed</title></head>"
								   "<body style='font-family: Arial, sans-serif; text-align: center; padding-top: 50px;'>"
								   "<h1>Sign-in failed</h1><p>" +
									   req.get_param_value("error") + "</p><p>You can close this window.</p></body></html>",
								   "text/html");
			   }
			   else
			   {
				   res.status = 400;
				   res.set_content("Bad Request: Missing authorization code", "text/plain");
			   }
		   });
	// Answer the browser's favicon request rather than logging it as a failure.
	s->Get("/favicon.ico", [](const httplib::Request &, httplib::Response &res) { res.status = 204; });

	// The shared pointer is captured by value, so the server outlives this scope safely even
	// though the thread is detached.
	std::thread([s]() { s->listen("localhost", kRedirectPort); }).detach();
	// Don't send the user to Google until the loopback endpoint can answer the redirect.
	for (int i = 0; i < 200 && !s->is_running(); i++)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void GoogleIdentityProvider::StopLocalServer()
{
	std::shared_ptr<httplib::Server> s;
	{
		std::lock_guard<std::mutex> lock(serverMutex);
		s = server;
		server.reset();
	}
	if (s)
		s->stop();
}

void GoogleIdentityProvider::OpenBrowser(const std::string &url) const
{
#ifdef _WIN32
	ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
	int r = system(("open \"" + url + "\"").c_str());
	(void)r;
#else
	int r = system(("xdg-open \"" + url + "\"").c_str());
	(void)r;
#endif
}
