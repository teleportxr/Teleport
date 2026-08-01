#pragma once
#include <string>

//! Stores small secrets (refresh tokens) in the operating system's credential store:
//! DPAPI on Windows, Keychain on macOS, libsecret on Linux. Secrets are keyed so that
//! several identity providers can each keep their own credentials.
class TokenStorage
{
public:
	static bool		   SaveSecret(const std::string &key, const std::string &value);
	static std::string LoadSecret(const std::string &key);
	static bool		   DeleteSecret(const std::string &key);

private:
#ifdef _WIN32
	static bool		   SaveTokenWindows(const std::string &key, const std::string &token);
	static std::string LoadTokenWindows(const std::string &key);
	static bool		   DeleteTokenWindows(const std::string &key);
#elif defined(__APPLE__)
	static bool		   SaveTokenMacOS(const std::string &key, const std::string &token);
	static std::string LoadTokenMacOS(const std::string &key);
	static bool		   DeleteTokenMacOS(const std::string &key);
#else
	static bool		   SaveTokenLinux(const std::string &key, const std::string &token);
	static std::string LoadTokenLinux(const std::string &key);
	static bool		   DeleteTokenLinux(const std::string &key);
#endif
};
