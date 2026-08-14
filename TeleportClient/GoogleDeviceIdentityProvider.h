#pragma once
#include "IdentityProvider.h"
#include <atomic>
#include <ctime>
#include <functional>
#include <mutex>
#include <string>

namespace teleport
{
	namespace client
	{
		//! Google sign-in for a device with no browser of its own: the OAuth 2.0 device
		//! authorization grant. The user is shown a short code and a URL to open on a phone or
		//! another computer, while this client polls for the result.
		//!
		//! Reports itself as "google" like the loopback provider, because it produces the same
		//! Google identity; only one of the two is ever registered in a given client. It keeps
		//! its refresh token under a separate key, since the token belongs to a different
		//! (limited-input) client id and cannot be used by the other provider.
		class GoogleDeviceIdentityProvider : public IdentityProvider
		{
		public:
			GoogleDeviceIdentityProvider();
			~GoogleDeviceIdentityProvider() override;

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

			void SetSignInPromptHandler(SignInPromptHandler handler) override;

		private:
			bool RequestDeviceCode(std::string &deviceCode, std::string &userCode, std::string &verificationUrl, int &intervalSeconds, int &expiresInSeconds);
			bool PollForToken(const std::string &deviceCode, int intervalSeconds, int expiresInSeconds);
			bool RefreshAccessToken();
			bool FetchUserInfo(IdentityProfile &profile);
			void SetError(const std::string &e);

			std::string clientId;
			std::string clientSecret;
			std::string accessToken;
			std::string refreshToken;
			std::time_t tokenExpiry = 0;

			mutable std::mutex errorMutex;
			std::string		   lastError;
			//! Set once at registration, before any worker thread exists; read from the worker.
			SignInPromptHandler promptHandler;

			std::atomic<bool> credentialsRevoked{false};
			std::atomic<bool> cancelled{false};
			std::atomic<bool> signInInProgress{false};
		};
	}
}
