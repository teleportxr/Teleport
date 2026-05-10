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

#if !defined(_WIN32)
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#endif

namespace
{
#if defined(__APPLE__)
    constexpr const char *kKeychainService = "co.simul.teleport";
    constexpr const char *kKeychainAccount = "GoogleRefreshToken";
#elif !defined(_WIN32)
    const SecretSchema &TeleportTokenSchema()
    {
        static const SecretSchema schema = {
            "co.simul.teleport.GoogleRefreshToken",
            SECRET_SCHEMA_NONE,
            {
                { "account", SECRET_SCHEMA_ATTRIBUTE_STRING },
                { nullptr, (SecretSchemaAttributeType)0 }
            },
            // Reserved padding required by the libsecret ABI.
            0, 0, 0, 0, 0, 0, 0, 0
        };
        return schema;
    }
#endif
}


bool TokenStorage::SaveRefreshToken(const std::string& refreshToken)
    {
#ifdef _WIN32
        return SaveTokenWindows(refreshToken);
#elif defined(__APPLE__)
        return SaveTokenMacOS(refreshToken);
#else
        return SaveTokenLinux(refreshToken);
#endif
    }

    std::string TokenStorage::LoadRefreshToken()
    {
#ifdef _WIN32
        return LoadTokenWindows();
#elif defined(__APPLE__)
        return LoadTokenMacOS();
#else
        return LoadTokenLinux();
#endif
    }

#ifdef _WIN32
    bool TokenStorage::SaveTokenWindows(const std::string& token)
    {
        // Using Windows Data Protection API (DPAPI)
        DATA_BLOB dataIn, dataOut;
        dataIn.pbData = (BYTE*)token.c_str();
        dataIn.cbData = (DWORD)token.length() + 1;
        
        if (!CryptProtectData(&dataIn, L"GoogleRefreshToken", 
                              NULL, NULL, NULL, 0, &dataOut))
        {
            return false;
        }
        
        // Save encrypted data to file or registry
        std::ofstream file("token.bin", std::ios::binary);
        if (!file)
        {
            LocalFree(dataOut.pbData);
            return false;
        }
        
        file.write((char*)dataOut.pbData, dataOut.cbData);
        LocalFree(dataOut.pbData);
        return true;
    }
    
    std::string TokenStorage::LoadTokenWindows()
    {
        // Read encrypted data
        std::ifstream file("token.bin", std::ios::binary | std::ios::ate);
        if (!file)
            return "";
            
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<char> buffer(size);
        if (!file.read(buffer.data(), size))
            return "";
            
        // Decrypt using DPAPI
        DATA_BLOB dataIn, dataOut;
        dataIn.pbData = (BYTE*)buffer.data();
        dataIn.cbData = (DWORD)size;
        
        if (!CryptUnprotectData(&dataIn, NULL, NULL, NULL, NULL, 0, &dataOut))
            return "";
            
        std::string token((char*)dataOut.pbData);
        LocalFree(dataOut.pbData);
        return token;
    }
#elif defined(__APPLE__)
    namespace
    {
        CFDictionaryRef BuildKeychainQuery(CFDataRef passwordData)
        {
            CFStringRef service = CFStringCreateWithCString(nullptr, kKeychainService, kCFStringEncodingUTF8);
            CFStringRef account = CFStringCreateWithCString(nullptr, kKeychainAccount, kCFStringEncodingUTF8);

            const void *keys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecAttrAccessible, kSecValueData };
            const void *vals[] = { kSecClassGenericPassword, service, account, kSecAttrAccessibleAfterFirstUnlock, passwordData };
            const CFIndex count = passwordData ? 5 : 4;

            CFDictionaryRef query = CFDictionaryCreate(nullptr, keys, vals, count,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFRelease(service);
            CFRelease(account);
            return query;
        }
    }

    bool TokenStorage::SaveTokenMacOS(const std::string& token)
    {
        CFDataRef pw = CFDataCreate(nullptr, reinterpret_cast<const UInt8 *>(token.data()),
                                    static_cast<CFIndex>(token.size()));
        CFDictionaryRef addQuery = BuildKeychainQuery(pw);
        OSStatus status = SecItemAdd(addQuery, nullptr);
        CFRelease(addQuery);

        if (status == errSecDuplicateItem)
        {
            CFDictionaryRef matchQuery = BuildKeychainQuery(nullptr);
            const void *updKeys[] = { kSecValueData };
            const void *updVals[] = { pw };
            CFDictionaryRef update = CFDictionaryCreate(nullptr, updKeys, updVals, 1,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            status = SecItemUpdate(matchQuery, update);
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

    std::string TokenStorage::LoadTokenMacOS()
    {
        CFStringRef service = CFStringCreateWithCString(nullptr, kKeychainService, kCFStringEncodingUTF8);
        CFStringRef account = CFStringCreateWithCString(nullptr, kKeychainAccount, kCFStringEncodingUTF8);

        const void *keys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecReturnData, kSecMatchLimit };
        const void *vals[] = { kSecClassGenericPassword, service, account, kCFBooleanTrue, kSecMatchLimitOne };
        CFDictionaryRef query = CFDictionaryCreate(nullptr, keys, vals, 5,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFRelease(service);
        CFRelease(account);

        CFTypeRef result = nullptr;
        OSStatus status = SecItemCopyMatching(query, &result);
        CFRelease(query);

        if (status == errSecItemNotFound)
            return "";
        if (status != errSecSuccess || !result)
        {
            TELEPORT_CERR << "TokenStorage: Keychain lookup failed (OSStatus " << status << ")" << std::endl;
            if (result) CFRelease(result);
            return "";
        }

        CFDataRef data = static_cast<CFDataRef>(result);
        std::string token(reinterpret_cast<const char *>(CFDataGetBytePtr(data)),
                          static_cast<size_t>(CFDataGetLength(data)));
        CFRelease(result);
        return token;
    }
#else
    bool TokenStorage::SaveTokenLinux(const std::string& token)
    {
        GError *error = nullptr;
        gboolean ok = secret_password_store_sync(
            &TeleportTokenSchema(),
            SECRET_COLLECTION_DEFAULT,
            "Teleport Google Refresh Token",
            token.c_str(),
            nullptr,
            &error,
            "account", "GoogleRefreshToken",
            nullptr);

        if (!ok)
        {
            TELEPORT_CERR << "TokenStorage: libsecret store failed: "
                          << (error ? error->message : "unknown error") << std::endl;
            if (error) g_error_free(error);
            return false;
        }
        return true;
    }

    std::string TokenStorage::LoadTokenLinux()
    {
        GError *error = nullptr;
        gchar *raw = secret_password_lookup_sync(
            &TeleportTokenSchema(),
            nullptr,
            &error,
            "account", "GoogleRefreshToken",
            nullptr);

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
#endif