#include "TokenStorage.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <fstream>
#include <vector>


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
    bool TokenStorage::SaveTokenMacOS(const std::string& token)
    {
        // Using Keychain API
        // Implementation would use Security.framework to store in Keychain
        // Example pseudo-code:
        /*
        SecKeychainItemRef itemRef = NULL;
        OSStatus status = SecKeychainAddGenericPassword(
            NULL,                // default keychain
            14,                  // service name length
            "GoogleAuthApp",     // service name
            14,                  // account name length
            "RefreshToken",      // account name
            token.length(),      // password length
            token.c_str(),       // password data
            &itemRef             // item reference
        );
        return status == errSecSuccess;
        */
        return false; // Replace with actual implementation  
    }
    
    std::string TokenStorage::LoadTokenMacOS()
    {
        // Using Keychain API to retrieve the token
        // Similar to SaveTokenMacOS but with SecKeychainFindGenericPassword
        return ""; // Replace with actual implementation
    }
#else
    bool TokenStorage::SaveTokenLinux(const std::string& token)
    {
        // Using libsecret (GNOME) or KWallet (KDE)
        // Example for libsecret (pseudo-code):
        /*
        SecretSchema schema = { "com.yourapp.token", SECRET_SCHEMA_NONE, { } };
        secret_password_store_sync(&schema, SECRET_COLLECTION_DEFAULT,
                                 "Google Refresh Token", token.c_str(),
                                 NULL, &error, 
                                 "application", "GoogleAuthApp", NULL);
        */
        return false; // Replace with actual implementation
    }
    
    std::string TokenStorage::LoadTokenLinux()
    {
        // Using libsecret or KWallet to retrieve the token
        return ""; // Replace with actual implementation
    }
#endif