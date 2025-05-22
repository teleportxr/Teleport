#pragma once
#include <string>
class TokenStorage
{
public:
    static bool SaveRefreshToken(const std::string& refreshToken);

    static std::string LoadRefreshToken();

private:
#ifdef _WIN32
    static bool SaveTokenWindows(const std::string& token);
    
    static std::string LoadTokenWindows();
#elif defined(__APPLE__)
    static bool SaveTokenMacOS(const std::string& token);
    
    static std::string LoadTokenMacOS();
#else
    static bool SaveTokenLinux(const std::string& token);
    
    static std::string LoadTokenLinux();
#endif
};