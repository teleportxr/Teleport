#pragma once
#include "IdentityProvider.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace teleport
{
	namespace client
	{
		//! Who the user is, and how they came to be signed in. Owns the available identity
		//! providers, of which Google is only one.
		//!
		//! Identity work never runs on the calling thread beyond reading the cached profile:
		//! Init() restores a remembered sign-in without touching the network or the browser, and
		//! interactive sign-in happens on a worker thread whose result is applied in Update().
		class Identity
		{
		public:
			~Identity();
			//! Register the providers and restore any remembered sign-in. Returns immediately.
			void Init();
			//! Call once per frame from the main thread to apply completed background work.
			void Update();
			//! Stop any sign-in in progress and join the worker.
			void Shutdown();

			//! Begin an interactive sign-in with the named provider. Only ever called in response
			//! to explicit user action. Returns false if a sign-in is already running.
			bool SignIn(const std::string &providerName);
			//! Abandon a sign-in in progress.
			void CancelSignIn();
			//! Forget the current identity, including stored credentials.
			void SignOut();

			SignInState GetState() const;
			//! A copy, because the profile may be replaced by Update() while others hold it.
			IdentityProfile GetProfile() const;
			bool			IsSignedIn() const;
			//! Text for the GUI, e.g. "Roderick (Google)" or "Not signed in".
			std::string GetDisplayText() const;
			std::string GetLastError() const;

			//! The out-of-band step a sign-in in progress is waiting on — visit this URL,
			//! enter this code — or an empty prompt when there is none. Populated from the
			//! worker thread, so a client with no console can relay it (see the `identity`
			//! command in HeadlessClient) rather than the user having to read a log.
			SignInPrompt GetSignInPrompt() const;
			//! Record an out-of-band instruction. Called by providers via the handler
			//! Identity installs on each of them, from the worker thread.
			void SetSignInPrompt(const SignInPrompt &prompt);

			//! Providers that can be offered to the user, in registration order.
			const std::vector<std::shared_ptr<IdentityProvider>> &GetProviders() const;
			void RegisterProvider(std::shared_ptr<IdentityProvider> provider);

		private:
			IdentityProvider *FindProvider(const std::string &name) const;
			//! Run work on the single worker thread. Any previous worker must have finished.
			void StartWorker(IdentityProvider *provider, bool interactive);

			std::vector<std::shared_ptr<IdentityProvider>> providers;
			mutable std::mutex							  profileMutex;
			IdentityProfile								  profile;
			std::atomic<SignInState>					  state{SignInState::SignedOut};

			std::thread			  worker;
			std::atomic<bool>	  workerRunning{false};
			std::atomic<bool>	  workerFinished{false};
			std::atomic<bool>	  workerSucceeded{false};
			IdentityProfile					workerProfile;
			//! Also written by the worker when it picks a provider for a silent restore.
			std::atomic<IdentityProvider *> workerProvider{nullptr};
			bool							workerInteractive = false;
			mutable std::mutex	  errorMutex;
			std::string			  lastError;
			mutable std::mutex	  promptMutex;
			SignInPrompt		  signInPrompt;
		};
		extern Identity identity;
	}
}
