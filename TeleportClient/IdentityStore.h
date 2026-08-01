#pragma once
#include "IdentityProvider.h"

namespace teleport
{
	namespace client
	{
		//! Caches the signed-in profile in <storage>/config/identity.json, so that a remembered
		//! sign-in can be restored at startup with no network access and no user interaction.
		//! Nothing secret is kept here: refresh tokens go to TokenStorage.
		class IdentityStore
		{
		public:
			static bool Save(const IdentityProfile &profile);
			static bool Load(IdentityProfile &profile);
			static void Clear();

		private:
			static std::string GetFilename();
		};
	}
}
