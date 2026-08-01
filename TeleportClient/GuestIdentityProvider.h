#pragma once
#include "IdentityProvider.h"

namespace teleport
{
	namespace client
	{
		//! Identity for a user who has not signed in with any external provider. The subject is a
		//! random identifier generated once and remembered, so a guest is at least recognisable as
		//! the same guest across sessions, without any account or network access.
		class GuestIdentityProvider : public IdentityProvider
		{
		public:
			const char *GetName() const override
			{
				return "guest";
			}
			const char *GetDisplayName() const override
			{
				return "Guest";
			}
			bool RequiresInteraction() const override
			{
				return false;
			}
			bool RestoreSilent(IdentityProfile &profile) override;
			bool SignInInteractive(IdentityProfile &profile) override;
			void SignOut() override;
		};
	}
}
