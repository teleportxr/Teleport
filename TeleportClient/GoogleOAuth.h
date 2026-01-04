#include <string>
#include <ctime>
#include <mutex>
#include <atomic>
#include <condition_variable> 

class GoogleOAuthPKCE
{
public:
    GoogleOAuthPKCE();

    ~GoogleOAuthPKCE();

    static bool TryAuthenticate();
    static std::string GetIdentity();

    const std::string& GetAccessToken() const
    {
        return accessToken;
    }

    const std::string& GetRefreshToken() const
    {
        return refreshToken;
    }

    bool GetUserInfo(std::string& email, std::string& name, std::string& pictureUrl);
    std::string GetUserEmail();

private:
    bool RefreshAccessToken();
    bool IsTokenExpired() const;
    bool EnsureValidToken();
    bool Authenticate();
    void generateCodeVerifier();
    void generateCodeChallenge();
    std::string base64UrlEncode(const unsigned char* input, size_t length);
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
    void startLocalServer();
    void openBrowser(const std::string& url);
    bool exchangeCodeForTokens();

    std::string clientId;
    std::string clientSecret;
    std::string authCode;
    std::string accessToken;
    std::string refreshToken;
    std::string codeVerifier;
    std::string codeChallenge;
    std::time_t m_tokenExpiry = 0;
    // Other members same as before
    std::time_t tokenExpiry = 0;
    
    std::string authError;
    bool authCodeReceived = false;
    bool authTimeoutOccurred = false;
    std::mutex mutex;
    std::condition_variable cv;
    // Flag to track if authentication has completed
    std::atomic<bool> authCompleted{false};
};