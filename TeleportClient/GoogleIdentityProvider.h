#pragma once
#include "IdentityProvider.h"
#include <atomic>
#include <condition_variable>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>

namespace httplib
{
	class Server;
}

namespace teleport
{
	namespace client
	{
		//! Google sign-in, using OAuth 2.0 with PKCE and a loopback redirect. One implementation
		//! of IdentityProvider among several: nothing outside this class is Google-specific.
		//! The refresh token is kept in the OS credential store, so a sign-in survives restarts
		//! and the browser is only used for the initial, user-initiated sign-in.
		class GoogleIdentityProvider : public IdentityProvider
		{
		public:
			GoogleIdentityProvider();
			~GoogleIdentityProvider() override;

			const char *GetName() const override
			{
				return "google";
			}
			const char *GetDisplayName() const override
			{
				return "Google";
			}
			bool IsAvailable() const override;
			bool RestoreSilent(IdentityProfile &profile) override;
			bool SignInInteractive(IdentityProfile &profile) override;
			void CancelSignIn() override;
			void SignOut() override;
			bool CredentialsRevoked() const override
			{
				return credentialsRevoked.load();
			}

			std::string GetLastError() const override;

		private:
			void LoadClientConfig();
			bool RefreshAccessToken();
			bool ExchangeCodeForTokens();
			bool FetchUserInfo(IdentityProfile &profile);
			void GenerateCodeVerifier();
			void GenerateCodeChallenge();
			void StartLocalServer();
			void StopLocalServer();
			void OpenBrowser(const std::string &url) const;
			void SetError(const std::string &e);

			std::string clientId;
			//! Empty for a "Desktop app" client id, which is the arrangement PKCE is designed for.
			std::string clientSecret;
			std::string authCode;
			std::string accessToken;
			std::string refreshToken;
			std::string codeVerifier;
			std::string codeChallenge;
			std::time_t tokenExpiry = 0;

			mutable std::mutex		errorMutex;
			std::string				lastError;
			std::mutex				serverMutex;
			std::shared_ptr<httplib::Server> server;

			std::mutex				mutex;
			std::condition_variable cv;
			bool					authCodeReceived = false;
			std::atomic<bool>		credentialsRevoked{false};
			std::atomic<bool>		cancelled{false};
			std::atomic<bool>		signInInProgress{false};
		};
	}
}
