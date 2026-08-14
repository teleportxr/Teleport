// Tests for the provider-agnostic identity manager: that a sign-in is remembered across
// sessions without contacting the provider, and that signing out forgets it.
#include <catch2/catch_test_macros.hpp>

#include "TeleportClient/Config.h"
#include "TeleportClient/Identity.h"
#include "TeleportClient/IdentityStore.h"
#include <chrono>
#include <filesystem>
#include <thread>

using namespace teleport::client;

namespace
{
	//! Stands in for Google (or any other provider). Counts the calls the manager makes, so a
	//! test can prove that a remembered sign-in costs the provider nothing.
	class FakeProvider : public IdentityProvider
	{
	public:
		const char *GetName() const override
		{
			return "fake";
		}
		const char *GetDisplayName() const override
		{
			return "Fake";
		}
		bool RestoreSilent(IdentityProfile &profile) override
		{
			restoreSilentCalls++;
			if (!hasStoredCredentials)
				return false;
			Fill(profile);
			return true;
		}
		bool SignInInteractive(IdentityProfile &profile) override
		{
			interactiveCalls++;
			if (!interactiveSucceeds)
				return false;
			hasStoredCredentials = true;
			Fill(profile);
			return true;
		}
		void SignOut() override
		{
			signOutCalls++;
			hasStoredCredentials = false;
		}
		bool CredentialsRevoked() const override
		{
			return revoked;
		}

		void Fill(IdentityProfile &profile) const
		{
			profile.provider		 = GetName();
			profile.subject			 = "subject-1234";
			profile.displayName		 = "Test User";
			profile.email			 = "test@example.com";
			profile.verifiedUnixTime = (int64_t)std::time(nullptr);
		}

		int	 restoreSilentCalls	  = 0;
		int	 interactiveCalls	  = 0;
		int	 signOutCalls		  = 0;
		bool hasStoredCredentials = false;
		bool interactiveSucceeds  = true;
		bool revoked			  = false;
	};

	//! Drive Update() as the render loop would, until the state settles or we give up.
	bool PumpUntil(Identity &identity, SignInState expected)
	{
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (std::chrono::steady_clock::now() < deadline)
		{
			identity.Update();
			if (identity.GetState() == expected)
				return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		return false;
	}

	//! Point Config at a scratch folder so the test never touches the user's real identity.
	struct ScratchStorage
	{
		ScratchStorage()
		{
			folder = (std::filesystem::temp_directory_path() / "teleport_identity_test").string();
			std::filesystem::remove_all(folder);
			std::filesystem::create_directories(std::filesystem::path(folder) / "config");
			Config::GetInstance().SetStorageFolder(folder.c_str());
			IdentityStore::Clear();
		}
		~ScratchStorage()
		{
			std::filesystem::remove_all(folder);
		}
		std::string folder;
	};
}

TEST_CASE("A user who has never signed in ends up signed out, with no interaction", "[identity]")
{
	ScratchStorage				  storage;
	Identity					  identity;
	std::shared_ptr<FakeProvider> fake = std::make_shared<FakeProvider>();
	identity.RegisterProvider(fake);

	identity.Init();
	REQUIRE(PumpUntil(identity, SignInState::SignedOut));
	// The manager may ask the provider to restore silently, but must never sign in interactively.
	CHECK(fake->interactiveCalls == 0);
	CHECK_FALSE(identity.IsSignedIn());
}

TEST_CASE("An interactive sign-in is remembered, and costs the provider nothing next time", "[identity]")
{
	ScratchStorage storage;
	{
		Identity					  identity;
		std::shared_ptr<FakeProvider> fake = std::make_shared<FakeProvider>();
		identity.RegisterProvider(fake);
		identity.Init();
		REQUIRE(PumpUntil(identity, SignInState::SignedOut));

		REQUIRE(identity.SignIn("fake"));
		REQUIRE(PumpUntil(identity, SignInState::SignedIn));
		CHECK(fake->interactiveCalls == 1);
		CHECK(identity.GetProfile().subject == "subject-1234");
		CHECK(identity.GetProfile().provider == "fake");
	}

	// A new session: the profile must come straight back from the cache, synchronously, without
	// the provider being consulted at all.
	Identity					  restored;
	std::shared_ptr<FakeProvider> fake = std::make_shared<FakeProvider>();
	restored.RegisterProvider(fake);
	restored.Init();
	CHECK(restored.GetState() == SignInState::SignedIn);
	CHECK(restored.GetProfile().displayName == "Test User");
	CHECK(fake->restoreSilentCalls == 0);
	CHECK(fake->interactiveCalls == 0);
}

TEST_CASE("The email address is remembered locally but kept out of the profile sent to servers", "[identity]")
{
	ScratchStorage				  storage;
	Identity					  identity;
	std::shared_ptr<FakeProvider> fake = std::make_shared<FakeProvider>();
	identity.RegisterProvider(fake);
	identity.Init();
	REQUIRE(PumpUntil(identity, SignInState::SignedOut));
	REQUIRE(identity.SignIn("fake"));
	REQUIRE(PumpUntil(identity, SignInState::SignedIn));

	// DiscoveryService sends provider, subject and displayName only; the email stays here.
	IdentityProfile profile = identity.GetProfile();
	CHECK(profile.email == "test@example.com");
	CHECK_FALSE(profile.subject.empty());
}

TEST_CASE("Signing out forgets the identity for good", "[identity]")
{
	ScratchStorage storage;
	{
		Identity					  identity;
		std::shared_ptr<FakeProvider> fake = std::make_shared<FakeProvider>();
		identity.RegisterProvider(fake);
		identity.Init();
		REQUIRE(PumpUntil(identity, SignInState::SignedOut));
		REQUIRE(identity.SignIn("fake"));
		REQUIRE(PumpUntil(identity, SignInState::SignedIn));

		identity.SignOut();
		CHECK(identity.GetState() == SignInState::SignedOut);
		CHECK(fake->signOutCalls == 1);
		CHECK(identity.GetProfile().provider.empty());
	}

	IdentityProfile cached;
	CHECK_FALSE(IdentityStore::Load(cached));

	Identity					  restarted;
	std::shared_ptr<FakeProvider> fake = std::make_shared<FakeProvider>();
	restarted.RegisterProvider(fake);
	restarted.Init();
	REQUIRE(PumpUntil(restarted, SignInState::SignedOut));
	CHECK_FALSE(restarted.IsSignedIn());
}

namespace
{
	//! Write a remembered profile that is old enough for the manager to revalidate it.
	void SaveStaleProfile()
	{
		IdentityProfile stale;
		stale.provider		   = "fake";
		stale.subject		   = "subject-1234";
		stale.displayName	   = "Test User";
		stale.verifiedUnixTime = (int64_t)std::time(nullptr) - (int64_t)(30 * 24 * 60 * 60);
		REQUIRE(IdentityStore::Save(stale));
	}
}

TEST_CASE("Being unable to reach the provider does not sign the user out", "[identity]")
{
	ScratchStorage storage;
	SaveStaleProfile();

	Identity					  identity;
	std::shared_ptr<FakeProvider> fake = std::make_shared<FakeProvider>();
	// No stored credentials to restore from, and not revoked: this stands for being offline.
	fake->hasStoredCredentials = false;
	fake->revoked			   = false;
	identity.RegisterProvider(fake);

	identity.Init();
	// Signed in from the cache straight away, and it must stay that way.
	CHECK(identity.GetState() == SignInState::SignedIn);
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < deadline)
	{
		identity.Update();
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	CHECK(fake->restoreSilentCalls == 1);
	CHECK(identity.GetState() == SignInState::SignedIn);
	CHECK(identity.GetProfile().subject == "subject-1234");
	CHECK(fake->signOutCalls == 0);
}

TEST_CASE("Revoked credentials do sign the user out", "[identity]")
{
	ScratchStorage storage;
	SaveStaleProfile();

	Identity					  identity;
	std::shared_ptr<FakeProvider> fake = std::make_shared<FakeProvider>();
	fake->hasStoredCredentials		   = false;
	fake->revoked					   = true;
	identity.RegisterProvider(fake);

	identity.Init();
	CHECK(identity.GetState() == SignInState::SignedIn);
	REQUIRE(PumpUntil(identity, SignInState::SignedOut));
	CHECK(fake->signOutCalls == 1);

	IdentityProfile cached;
	CHECK_FALSE(IdentityStore::Load(cached));
}

TEST_CASE("A failed sign-in reports failure rather than pretending to succeed", "[identity]")
{
	ScratchStorage				  storage;
	Identity					  identity;
	std::shared_ptr<FakeProvider> fake = std::make_shared<FakeProvider>();
	fake->interactiveSucceeds		   = false;
	identity.RegisterProvider(fake);
	identity.Init();
	REQUIRE(PumpUntil(identity, SignInState::SignedOut));

	REQUIRE(identity.SignIn("fake"));
	REQUIRE(PumpUntil(identity, SignInState::Failed));
	CHECK_FALSE(identity.IsSignedIn());

	IdentityProfile cached;
	CHECK_FALSE(IdentityStore::Load(cached));
}
