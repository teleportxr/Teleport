#include "Identity.h"
#include "GuestIdentityProvider.h"
#include "IdentityStore.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#include <ctime>
// Google sign-in relies on curl/openssl and a local HTTP redirect server,
// which are not available in the Android client build.
#if !defined(PLATFORM_ANDROID)
#include "GoogleIdentityProvider.h"
#endif

using namespace teleport;
using namespace client;

Identity teleport::client::identity;

namespace
{
	//! A remembered profile older than this is revalidated in the background. It stays usable
	//! meanwhile: revalidation only downgrades the state if the provider rejects the credentials.
	constexpr int64_t kRevalidateSeconds = 7 * 24 * 60 * 60;
}

Identity::~Identity()
{
	Shutdown();
}

void Identity::RegisterProvider(std::shared_ptr<IdentityProvider> provider)
{
	if (provider && provider->IsAvailable())
		providers.push_back(provider);
}

const std::vector<std::shared_ptr<IdentityProvider>> &Identity::GetProviders() const
{
	return providers;
}

IdentityProvider *Identity::FindProvider(const std::string &name) const
{
	for (const auto &p : providers)
	{
		if (name == p->GetName())
			return p.get();
	}
	return nullptr;
}

void Identity::Init()
{
	if (providers.empty())
	{
#if !defined(PLATFORM_ANDROID)
		RegisterProvider(std::make_shared<GoogleIdentityProvider>());
#endif
		// Last, so that a real identity is always preferred when restoring.
		RegisterProvider(std::make_shared<GuestIdentityProvider>());
	}

	IdentityProfile cached;
	if (IdentityStore::Load(cached))
	{
		IdentityProvider *provider = FindProvider(cached.provider);
		if (provider)
		{
			// Trust the cache immediately: no network, no browser, no waiting.
			{
				std::lock_guard<std::mutex> lock(profileMutex);
				profile = cached;
			}
			state = SignInState::SignedIn;
			TELEPORT_LOG("Identity restored from cache: {}", GetDisplayText());
			const int64_t age = (int64_t)std::time(nullptr) - cached.verifiedUnixTime;
			if (age > kRevalidateSeconds || cached.verifiedUnixTime == 0)
				StartWorker(provider, false);
			return;
		}
		// The cached profile names a provider this build does not have.
		IdentityStore::Clear();
	}

	// Nothing cached: a provider may still hold stored credentials from a previous install of
	// the config folder. Ask each in turn, on the worker thread, without user interaction.
	if (!providers.empty())
	{
		state = SignInState::Restoring;
		StartWorker(nullptr, false);
	}
}

void Identity::StartWorker(IdentityProvider *provider, bool interactive)
{
	if (workerRunning)
		return;
	if (worker.joinable())
		worker.join();
	workerProvider	  = provider;
	workerInteractive = interactive;
	workerFinished	  = false;
	workerSucceeded	  = false;
	workerProfile.Clear();
	workerRunning = true;
	worker		  = std::thread(
		 [this]()
		 {
			 bool			   ok = false;
			 IdentityProvider *p  = workerProvider.load();
			 if (p)
			 {
				 ok = workerInteractive ? p->SignInInteractive(workerProfile) : p->RestoreSilent(workerProfile);
			 }
			 else
			 {
				 // Silent restore across all providers, best first.
				 for (const auto &candidate : providers)
				 {
					 if (candidate->RestoreSilent(workerProfile))
					 {
						 workerProvider = candidate.get();
						 ok			   = true;
						 break;
					 }
				 }
			 }
			 workerSucceeded = ok;
			 workerFinished	 = true;
		 });
}

void Identity::Update()
{
	if (!workerFinished)
		return;
	if (worker.joinable())
		worker.join();
	workerRunning = false;
	workerFinished = false;

	const bool succeeded = workerSucceeded.load();
	if (succeeded && workerProfile.IsValid())
	{
		{
			std::lock_guard<std::mutex> lock(profileMutex);
			profile = workerProfile;
		}
		state = SignInState::SignedIn;
		IdentityStore::Save(workerProfile);
		TELEPORT_LOG("Signed in: {}", GetDisplayText());
	}
	else
	{
		IdentityProvider *p		= workerProvider.load();
		std::string		  error = p ? p->GetLastError() : std::string();
		{
			std::lock_guard<std::mutex> lock(errorMutex);
			lastError = error;
		}
		if (workerInteractive)
		{
			state = SignInState::Failed;
			TELEPORT_WARN("Sign-in failed: {}", error.empty() ? "no reason given" : error);
		}
		else if (state == SignInState::SignedIn)
		{
			// Background revalidation of a remembered sign-in failed. Only give up the identity
			// if the provider says the credentials are actually gone: a failure to reach the
			// provider must not sign the user out.
			if (p && p->CredentialsRevoked())
			{
				TELEPORT_WARN("Remembered sign-in is no longer valid: {}", error.empty() ? "credentials revoked" : error);
				SignOut();
			}
			else
			{
				TELEPORT_WARN("Could not revalidate identity ({}); keeping the remembered sign-in.", error.empty() ? "no reason given" : error);
			}
		}
		else
		{
			state = SignInState::SignedOut;
		}
	}
	workerProvider = nullptr;
}

bool Identity::SignIn(const std::string &providerName)
{
	if (workerRunning)
		return false;
	IdentityProvider *provider = FindProvider(providerName);
	if (!provider)
	{
		TELEPORT_WARN("No such identity provider: {}", providerName);
		return false;
	}
	{
		std::lock_guard<std::mutex> lock(errorMutex);
		lastError.clear();
	}
	state = provider->RequiresInteraction() ? SignInState::WaitingForUser : SignInState::Restoring;
	StartWorker(provider, true);
	return true;
}

void Identity::CancelSignIn()
{
	if (!workerRunning)
		return;
	if (IdentityProvider *p = workerProvider.load())
		p->CancelSignIn();
}

void Identity::SignOut()
{
	std::string providerName;
	{
		std::lock_guard<std::mutex> lock(profileMutex);
		providerName = profile.provider;
		profile.Clear();
	}
	if (IdentityProvider *provider = FindProvider(providerName))
		provider->SignOut();
	IdentityStore::Clear();
	state = SignInState::SignedOut;
}

void Identity::Shutdown()
{
	CancelSignIn();
	if (worker.joinable())
		worker.join();
	workerRunning  = false;
	workerFinished = false;
}

SignInState Identity::GetState() const
{
	return state.load();
}

IdentityProfile Identity::GetProfile() const
{
	std::lock_guard<std::mutex> lock(profileMutex);
	return profile;
}

bool Identity::IsSignedIn() const
{
	return state.load() == SignInState::SignedIn;
}

std::string Identity::GetLastError() const
{
	std::lock_guard<std::mutex> lock(errorMutex);
	return lastError;
}

std::string Identity::GetDisplayText() const
{
	switch (state.load())
	{
	case SignInState::SignedIn:
	{
		IdentityProfile p = GetProfile();
		std::string		name = p.displayName.empty() ? (p.email.empty() ? p.subject : p.email) : p.displayName;
		for (const auto &provider : providers)
		{
			if (p.provider == provider->GetName())
				return name + " (" + provider->GetDisplayName() + ")";
		}
		return name;
	}
	case SignInState::Restoring:
		return "Restoring sign-in...";
	case SignInState::WaitingForUser:
		// Deliberately not "in your browser": the device flow sends the user to another machine.
		return "Waiting for you to complete sign-in...";
	case SignInState::Failed:
		return "Sign-in failed";
	default:
		return "Not signed in";
	}
}
