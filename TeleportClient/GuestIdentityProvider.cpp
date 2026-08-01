#include "GuestIdentityProvider.h"
#include <ctime>
#include <random>
#include <sstream>

using namespace teleport;
using namespace client;

namespace
{
	std::string GenerateSubject()
	{
		std::random_device						rd;
		std::mt19937_64							gen(rd());
		std::uniform_int_distribution<uint64_t> dis;
		std::ostringstream						str;
		str << std::hex << dis(gen) << dis(gen);
		return str.str();
	}
}

bool GuestIdentityProvider::RestoreSilent(IdentityProfile &profile)
{
	// A guest has no credentials to restore: the identity manager's profile cache is what makes
	// a guest the same guest next time. Nothing to do here, and nothing to prompt the user for.
	(void)profile;
	return false;
}

bool GuestIdentityProvider::SignInInteractive(IdentityProfile &profile)
{
	// Nothing to ask the user, and nothing to ask a server: the identity is made locally.
	profile.provider		 = GetName();
	profile.subject			 = GenerateSubject();
	profile.displayName		 = "Guest";
	profile.verifiedUnixTime = (int64_t)std::time(nullptr);
	return true;
}

void GuestIdentityProvider::SignOut()
{
	// The cached profile is cleared by the identity manager; a guest keeps nothing else.
}
