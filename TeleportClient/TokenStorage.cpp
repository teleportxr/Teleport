#include "TokenStorage.h"

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#else
#include <libsecret/secret.h>
#endif
#include <fstream>
#include <vector>

#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#ifdef _WIN32
#include "Config.h"
#include <filesystem>
#endif

namespace
{
#if defined(__APPLE__)
	constexpr const char *kKeychainService = "co.simul.teleport";
#elif !defined(_WIN32)
	const SecretSchema &TeleportTokenSchema()
	{
		static const SecretSchema schema = {"co.simul.teleport.GoogleRefreshToken",
											SECRET_SCHEMA_NONE,
											{{"account", SECRET_SCHEMA_ATTRIBUTE_STRING}, {nullptr, (SecretSchemaAttributeType)0}},
											// Reserved padding required by the libsecret ABI.
											0,
											0,
											0,
											0,
											0,
											0,
											0,
											0};
		return schema;
	}
#endif
}

bool TokenStorage::SaveSecret(const std::string &key, const std::string &value)
{
#ifdef _WIN32
	return SaveTokenWindows(key, value);
#elif defined(__APPLE__)
	return SaveTokenMacOS(key, value);
#else
	return SaveTokenLinux(key, value);
#endif
}

std::string TokenStorage::LoadSecret(const std::string &key)
{
#ifdef _WIN32
	return LoadTokenWindows(key);
#elif defined(__APPLE__)
	return LoadTokenMacOS(key);
#else
	return LoadTokenLinux(key);
#endif
}

bool TokenStorage::DeleteSecret(const std::string &key)
{
#ifdef _WIN32
	return DeleteTokenWindows(key);
#elif defined(__APPLE__)
	return DeleteTokenMacOS(key);
#else
	return DeleteTokenLinux(key);
#endif
}

#ifdef _WIN32
namespace
{
	//! Secrets live beside the other config files, not in the working directory.
	std::string SecretFilePath(const std::string &key)
	{
		std::filesystem::path dir = std::filesystem::path(teleport::client::Config::GetInstance().GetStorageFolder()) / "config";
		std::error_code		  ec;
		std::filesystem::create_directories(dir, ec);
		return (dir / (key + ".bin")).string();
	}
}

bool TokenStorage::SaveTokenWindows(const std::string &key, const std::string &token)
{
	// Using Windows Data Protection API (DPAPI)
	DATA_BLOB dataIn, dataOut;
	dataIn.pbData = (BYTE *)token.c_str();
	dataIn.cbData = (DWORD)token.length() + 1;

	if (!CryptProtectData(&dataIn, L"TeleportSecret", NULL, NULL, NULL, 0, &dataOut))
	{
		return false;
	}

	std::ofstream file(SecretFilePath(key), std::ios::binary);
	if (!file)
	{
		LocalFree(dataOut.pbData);
		return false;
	}

	file.write((char *)dataOut.pbData, dataOut.cbData);
	LocalFree(dataOut.pbData);
	return true;
}

std::string TokenStorage::LoadTokenWindows(const std::string &key)
{
	// Read encrypted data
	std::ifstream file(SecretFilePath(key), std::ios::binary | std::ios::ate);
	if (!file)
		return "";

	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(size);
	if (!file.read(buffer.data(), size))
		return "";

	// Decrypt using DPAPI
	DATA_BLOB dataIn, dataOut;
	dataIn.pbData = (BYTE *)buffer.data();
	dataIn.cbData = (DWORD)size;

	if (!CryptUnprotectData(&dataIn, NULL, NULL, NULL, NULL, 0, &dataOut))
		return "";

	std::string token((char *)dataOut.pbData);
	LocalFree(dataOut.pbData);
	return token;
}

bool TokenStorage::DeleteTokenWindows(const std::string &key)
{
	std::error_code ec;
	std::filesystem::remove(SecretFilePath(key), ec);
	return !ec;
}
#elif defined(__APPLE__)
namespace
{
	CFDictionaryRef BuildKeychainQuery(const std::string &key, CFDataRef passwordData)
	{
		CFStringRef service = CFStringCreateWithCString(nullptr, kKeychainService, kCFStringEncodingUTF8);
		CFStringRef account = CFStringCreateWithCString(nullptr, key.c_str(), kCFStringEncodingUTF8);

		const void	 *keys[] = {kSecClass, kSecAttrService, kSecAttrAccount, kSecAttrAccessible, kSecValueData};
		const void	 *vals[] = {kSecClassGenericPassword, service, account, kSecAttrAccessibleAfterFirstUnlock, passwordData};
		const CFIndex count	 = passwordData ? 5 : 4;

		CFDictionaryRef query = CFDictionaryCreate(nullptr, keys, vals, count, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFRelease(service);
		CFRelease(account);
		return query;
	}
}

bool TokenStorage::SaveTokenMacOS(const std::string &key, const std::string &token)
{
	CFDataRef		pw		 = CFDataCreate(nullptr, reinterpret_cast<const UInt8 *>(token.data()), static_cast<CFIndex>(token.size()));
	CFDictionaryRef addQuery = BuildKeychainQuery(key, pw);
	OSStatus		status	 = SecItemAdd(addQuery, nullptr);
	CFRelease(addQuery);

	if (status == errSecDuplicateItem)
	{
		CFDictionaryRef matchQuery = BuildKeychainQuery(key, nullptr);
		const void	   *updKeys[]  = {kSecValueData};
		const void	   *updVals[]  = {pw};
		CFDictionaryRef update	   = CFDictionaryCreate(nullptr, updKeys, updVals, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		status					   = SecItemUpdate(matchQuery, update);
		CFRelease(matchQuery);
		CFRelease(update);
	}
	CFRelease(pw);

	if (status != errSecSuccess)
	{
		TELEPORT_CERR << "TokenStorage: Keychain save failed (OSStatus " << status << ")" << std::endl;
		return false;
	}
	return true;
}

std::string TokenStorage::LoadTokenMacOS(const std::string &key)
{
	CFStringRef service = CFStringCreateWithCString(nullptr, kKeychainService, kCFStringEncodingUTF8);
	CFStringRef account = CFStringCreateWithCString(nullptr, key.c_str(), kCFStringEncodingUTF8);

	const void	   *keys[] = {kSecClass, kSecAttrService, kSecAttrAccount, kSecReturnData, kSecMatchLimit};
	const void	   *vals[] = {kSecClassGenericPassword, service, account, kCFBooleanTrue, kSecMatchLimitOne};
	CFDictionaryRef query  = CFDictionaryCreate(nullptr, keys, vals, 5, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFRelease(service);
	CFRelease(account);

	CFTypeRef result = nullptr;
	OSStatus  status = SecItemCopyMatching(query, &result);
	CFRelease(query);

	if (status == errSecItemNotFound)
		return "";
	if (status != errSecSuccess || !result)
	{
		TELEPORT_CERR << "TokenStorage: Keychain lookup failed (OSStatus " << status << ")" << std::endl;
		if (result) CFRelease(result);
		return "";
	}

	CFDataRef	data = static_cast<CFDataRef>(result);
	std::string token(reinterpret_cast<const char *>(CFDataGetBytePtr(data)), static_cast<size_t>(CFDataGetLength(data)));
	CFRelease(result);
	return token;
}

bool TokenStorage::DeleteTokenMacOS(const std::string &key)
{
	CFDictionaryRef query  = BuildKeychainQuery(key, nullptr);
	OSStatus		status = SecItemDelete(query);
	CFRelease(query);
	return status == errSecSuccess || status == errSecItemNotFound;
}
#else
bool TokenStorage::SaveTokenLinux(const std::string &key, const std::string &token)
{
	GError	*error = nullptr;
	gboolean ok	   = secret_password_store_sync(&TeleportTokenSchema(),
												SECRET_COLLECTION_DEFAULT,
												"Teleport Identity Token",
												token.c_str(),
												nullptr,
												&error,
												"account",
												key.c_str(),
												nullptr);

	if (!ok)
	{
		TELEPORT_CERR << "TokenStorage: libsecret store failed: " << (error ? error->message : "unknown error") << std::endl;
		if (error) g_error_free(error);
		return false;
	}
	return true;
}

std::string TokenStorage::LoadTokenLinux(const std::string &key)
{
	GError *error = nullptr;
	gchar  *raw	  = secret_password_lookup_sync(&TeleportTokenSchema(), nullptr, &error, "account", key.c_str(), nullptr);

	if (error)
	{
		TELEPORT_WARN("TokenStorage: libsecret lookup failed: {}", error->message);
		g_error_free(error);
		return "";
	}
	if (!raw)
		return "";

	std::string token(raw);
	secret_password_free(raw);
	return token;
}

bool TokenStorage::DeleteTokenLinux(const std::string &key)
{
	GError	*error = nullptr;
	gboolean ok	   = secret_password_clear_sync(&TeleportTokenSchema(), nullptr, &error, "account", key.c_str(), nullptr);
	if (error)
	{
		TELEPORT_WARN("TokenStorage: libsecret clear failed: {}", error->message);
		g_error_free(error);
		return false;
	}
	return ok != FALSE;
}
#endif
