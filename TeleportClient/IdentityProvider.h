#pragma once
#include <cstdint>
#include <string>

namespace teleport
{
	namespace client
	{
		//! A provider-agnostic description of who the user is. Google is only one possible source
		//! of this information, so nothing here is specific to any one identity provider.
		struct IdentityProfile
		{
			//! Identity provider name, e.g. "google" or "guest". Matches IdentityProvider::GetName().
			std::string provider;
			//! Stable identifier for this user, unique within the provider. Never reused.
			std::string subject;
			//! Human-readable name. May be empty.
			std::string displayName;
			//! Kept on the client only: not sent to servers.
			std::string email;
			//! Optional URL of the user's picture, as supplied by the provider.
			std::string pictureUrl;
			//! Unix time at which the provider last confirmed this profile.
			int64_t verifiedUnixTime = 0;

			bool IsValid() const
			{
				return !provider.empty() && !subject.empty();
			}
			void Clear()
			{
				*this = IdentityProfile();
			}
		};

		enum class SignInState : uint8_t
		{
			//! No identity: the client works normally, and servers are told nothing.
			SignedOut,
			//! Restoring a remembered sign-in in the background. No user interaction.
			Restoring,
			//! An interactive sign-in is in progress, probably in the user's browser.
			WaitingForUser,
			SignedIn,
			//! The last attempt failed. The reason is in Identity::GetLastError().
			Failed
		};

		//! Interface for a source of identity. Implementations must be usable from a worker thread,
		//! and must not touch the GUI or any rendering state.
		class IdentityProvider
		{
		public:
			virtual ~IdentityProvider() {}
			//! Stable name used in storage and on the wire, e.g. "google".
			virtual const char *GetName() const = 0;
			//! Name to show the user, e.g. "Google".
			virtual const char *GetDisplayName() const = 0;
			//! False if this provider cannot be used in this build or on this platform.
			virtual bool IsAvailable() const
			{
				return true;
			}
			//! True if signing in requires user interaction, and so may only happen on request.
			virtual bool RequiresInteraction() const
			{
				return true;
			}
			//! Restore a previous sign-in without any user interaction. Must never open a browser.
			//! May use the network, so it is always called from a worker thread.
			virtual bool RestoreSilent(IdentityProfile &profile) = 0;
			//! Sign in, interacting with the user. May block for minutes; always called on a worker thread.
			virtual bool SignInInteractive(IdentityProfile &profile) = 0;
			//! Abandon an interactive sign-in in progress. Called from the main thread.
			virtual void CancelSignIn() {}
			//! Discard all stored credentials for this provider.
			virtual void SignOut() = 0;
			//! True only when the provider knows the stored credentials are gone for good, as
			//! opposed to a failure that might just be a lost network. A remembered sign-in is
			//! only discarded when this is true.
			virtual bool CredentialsRevoked() const
			{
				return false;
			}
			//! Description of the most recent failure, for the GUI.
			virtual std::string GetLastError() const
			{
				return std::string();
			}
		};
	}
}
