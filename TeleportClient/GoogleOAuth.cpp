#include "GoogleOAuth.h"
#include <curl/curl.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <string>

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <httplib.h>


GoogleOAuthPKCE auth;

GoogleOAuthPKCE::GoogleOAuthPKCE()
{
	curl_global_init(CURL_GLOBAL_ALL);
}

GoogleOAuthPKCE::~GoogleOAuthPKCE()
{
	curl_global_cleanup();
}

bool GoogleOAuthPKCE::Authenticate()
{
	// Generate code verifier and challenge
	generateCodeVerifier();
	generateCodeChallenge();

	// Start local server to handle redirect
	startLocalServer();

	// Generate authentication URL with code challenge
	std::string authUrl = "https://accounts.google.com/o/oauth2/auth?"
						  "client_id=" +
						  clientId + "&redirect_uri=http://localhost:8085/callback" + "&response_type=code" + "&scope=email%20profile" +
						  "&code_challenge=" + codeChallenge + "&code_challenge_method=S256" + "&access_type=offline";

	// Open browser with auth URL
	openBrowser(authUrl);

	// Wait for authentication callback
	{
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { 
            return authCodeReceived || authTimeoutOccurred; 
        });
        
        if (authTimeoutOccurred) {
            std::cerr << "Authentication timed out" << std::endl;
            return false;
        }
	}

	// Exchange auth code for tokens using code verifier
	return exchangeCodeForTokens();
}
std::string GoogleOAuthPKCE::GetIdentity()
{
    return auth.GetUserEmail();
}

void GoogleOAuthPKCE::generateCodeVerifier()
{
	// Generate a random string of 43-128 chars
	std::string						charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
	std::random_device				rd;
	std::mt19937					gen(rd());
	std::uniform_int_distribution<> dis(0, (int)charset.size() - 1);

	codeVerifier.clear();
	for (int i = 0; i < 96; ++i) // Choose a length between 43-128
	{
		codeVerifier += charset[dis(gen)];
	}
}

void GoogleOAuthPKCE::generateCodeChallenge()
{
	// SHA256 hash of the code verifier
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256(reinterpret_cast<const unsigned char *>(codeVerifier.c_str()), codeVerifier.length(), hash);

	// Base64URL encode the hash
	codeChallenge = base64UrlEncode(hash, SHA256_DIGEST_LENGTH);
}

std::string GoogleOAuthPKCE::base64UrlEncode(const unsigned char *input, size_t length)
{
	BIO		*bio, *b64;
	BUF_MEM *bufferPtr;

	b64 = BIO_new(BIO_f_base64());
	bio = BIO_new(BIO_s_mem());
	bio = BIO_push(b64, bio);

	BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
	BIO_write(bio, input, (int)length);
	BIO_flush(bio);
	BIO_get_mem_ptr(bio, &bufferPtr);

	std::string base64(bufferPtr->data, bufferPtr->length);
	BIO_free_all(bio);

	// Convert to base64url format
	std::string base64url = base64;
	std::replace(base64url.begin(), base64url.end(), '+', '-');
	std::replace(base64url.begin(), base64url.end(), '/', '_');
	base64url.erase(std::remove(base64url.begin(), base64url.end(), '='), base64url.end());

	return base64url;
}
size_t GoogleOAuthPKCE::WriteCallback(void *contents, size_t size, size_t nmemb, std::string *userp)
{
	userp->append((char *)contents, size * nmemb);
	return size * nmemb;
}
    std::shared_ptr<httplib::Server> server;

void GoogleOAuthPKCE::startLocalServer()
{
    // Create a server pointer that can be shared between threads
    server = std::make_shared<httplib::Server>();
    
    // Handle the OAuth callback
    server->Get("/callback", [this](const httplib::Request& req, httplib::Response& res) {
        // Check if the authorization code is in the query parameters
        if (req.has_param("code")) {
            authCode = req.get_param_value("code");
            authCodeReceived = true;
            authCompleted = true;
            
            // Notify the waiting thread
            cv.notify_one();
            
            // Return a success page to the user
            res.set_content(
                "<html><head><title>Authentication Successful</title></head>"
                "<body style='font-family: Arial, sans-serif; text-align: center; padding-top: 50px;'>"
                "<h1>Authentication Successful!</h1>"
                "<p>You can now close this window and return to the application.</p>"
                "<script>window.setTimeout(function() { window.close(); }, 3000);</script>"
                "</body></html>",
                "text/html");
        } 
        else if (req.has_param("error")) {
            // Handle authentication errors
            std::string error = req.get_param_value("error");
            authError = error;
            authCodeReceived = false;
            authCompleted = true;
            
            // Notify the waiting thread
            cv.notify_one();
            
            // Return an error page to the user
            res.set_content(
                "<html><head><title>Authentication Error</title></head>"
                "<body style='font-family: Arial, sans-serif; text-align: center; padding-top: 50px;'>"
                "<h1>Authentication Failed</h1>"
                "<p>Error: " + error + "</p>"
                "<p>Please close this window and try again.</p>"
                "</body></html>",
                "text/html");
        } 
        else {
            // Invalid request
            res.status = 400;
            res.set_content("Bad Request: Missing authorization code", "text/plain");
        }
    });

    // Add a route to serve a favicon to prevent additional requests
    server->Get("/favicon.ico", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204; // No content
    });

    // Flag to indicate when server is ready
    std::atomic<bool> serverReady{false};
    
    // Start the server in a separate thread
    std::thread serverThread([ &serverReady]() {
        // Signal that we're ready to accept connections
        serverReady = true;
        
        // Start the server (this will block until server->stop() is called)
        server->listen("localhost", 8085);
        
        // When we get here, the server has been stopped
    });
    
    // Detach the server thread so it runs independently
    serverThread.detach();
    
    // Wait for the server to be ready
    while (!serverReady) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Start a separate thread to handle timeout
    std::thread timeoutThread([this]() {
        // Wait for either authentication completion or timeout
        const auto timeout = std::chrono::minutes(2);
        const auto start = std::chrono::steady_clock::now();
        
        while (!authCompleted) {
            // Check if we've timed out
            if (std::chrono::steady_clock::now() - start > timeout) {
                std::cerr << "Authentication timed out" << std::endl;
                authTimeoutOccurred = true;
                cv.notify_one(); // Notify waiting threads
                break;
            }
            
            // Sleep a bit to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Stop the server (whether auth completed or timed out)
        server->stop();
    });
    
    timeoutThread.detach();
}

void GoogleOAuthPKCE::openBrowser(const std::string &url)
{
	// Platform-specific browser opening
#ifdef _WIN32
	ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
	system(("open \"" + url + "\"").c_str());
#else
	system(("xdg-open \"" + url + "\"").c_str());
#endif
}

bool GoogleOAuthPKCE::exchangeCodeForTokens()
{
	CURL *curl = curl_easy_init();
	if (!curl)
		return false;

	std::string readBuffer;
	std::string postFields = "code=" + authCode + "&client_id=" + clientId 
        + "&client_secret=" + clientSecret +  // Add the client secret
        + "&code_verifier=" + codeVerifier
							+ "&redirect_uri=http://localhost:8085/callback"
                            + "&grant_type=authorization_code";

	curl_easy_setopt(curl, CURLOPT_URL, "https://oauth2.googleapis.com/token");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK)
		return false;

	// Parse JSON response
	try
	{
		auto json	  = nlohmann::json::parse(readBuffer);
		accessToken = json["access_token"];

		if (json.contains("refresh_token"))
			refreshToken = json["refresh_token"];
            
		if (json.contains("expires_in"))
            m_tokenExpiry = std::time(nullptr) + json["expires_in"].get<int>();
		return true;
	}
	catch (const std::exception &e)
	{
		std::cerr << "Failed to parse JSON response: " << e.what() << std::endl;
		return false;
	}
}
#include "TokenStorage.h"
bool GoogleOAuthPKCE::TryAuthenticate()
{
	// Get client ID (should be no client secret needed with PKCE)
    auth.clientSecret="";
    auth.clientId="368274457072-gu640ehfoh1kqsh5bt7bneui21ka5e1k.apps.googleusercontent.com";

	// Try to load existing refresh token
	auth.refreshToken = TokenStorage::LoadRefreshToken();
	if (auth.refreshToken.empty())
	{
		// No saved token, authenticate from scratch
		if (!auth.Authenticate())
		{
			std::cerr << "Authentication failed" << std::endl;
			return false;
		}

		// Save the new refresh token
		TokenStorage::SaveRefreshToken(auth.GetRefreshToken());
	}
	else
	{
		auth.RefreshAccessToken();
	}

	std::cout << "Authentication successful!" << std::endl;
	return true;
}

bool GoogleOAuthPKCE::RefreshAccessToken()
{
    // Check if we have a refresh token
    if (refreshToken.empty())
    {
        std::cerr << "No refresh token available" << std::endl;
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl)
        return false;

    std::string readBuffer;
    
    // Set up refresh token request
    std::string postFields = "client_id=" + clientId +
        "&client_secret=" + clientSecret +
        "&refresh_token=" + refreshToken +
        "&grant_type=refresh_token";

    curl_easy_setopt(curl, CURLOPT_URL, "https://oauth2.googleapis.com/token");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return false;

    // Parse JSON response
    try
    {
        auto json = nlohmann::json::parse(readBuffer);
        
        if (json.contains("error"))
        {
            std::cerr << "Error refreshing token: " << json["error_description"].get<std::string>() << std::endl;
            return false;
        }
        
        accessToken = json["access_token"];
        
        if (json.contains("expires_in"))
            m_tokenExpiry = std::time(nullptr) + json["expires_in"].get<int>();
            
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error parsing refresh token response: " << e.what() << std::endl;
        return false;
    }
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp)
{
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool GoogleOAuthPKCE::IsTokenExpired() const
{
    // Allow a 5-minute buffer before expiration
    return std::time(nullptr) > (m_tokenExpiry - 300);
}

bool GoogleOAuthPKCE::EnsureValidToken()
{
    if (IsTokenExpired() && !refreshToken.empty())
    {
        return RefreshAccessToken();
    }
    return !accessToken.empty() && !IsTokenExpired();
}

// Add a method to get user information
bool GoogleOAuthPKCE::GetUserInfo(std::string& email, std::string& name, std::string& pictureUrl)
{
    // Make sure we have an access token
    if (accessToken.empty())
    {
        std::cerr << "No access token available" << std::endl;
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl)
    {
        std::cerr << "Failed to initialize curl" << std::endl;
        return false;
    }

    std::string readBuffer;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());

    // Set up the request to Google's UserInfo endpoint
    curl_easy_setopt(curl, CURLOPT_URL, "https://www.googleapis.com/oauth2/v3/userinfo");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    
    // Clean up
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        return false;
    }

    // Parse the JSON response
    try
    {
        auto json = nlohmann::json::parse(readBuffer);
        
        // Check for errors in the response
        if (json.contains("error"))
        {
            std::cerr << "Error: " << json["error_description"].get<std::string>() << std::endl;
            return false;
        }
        
        // Extract user information
        if (json.contains("email"))
            email = json["email"].get<std::string>();
        
        if (json.contains("name"))
            name = json["name"].get<std::string>();
        
        if (json.contains("picture"))
            pictureUrl = json["picture"].get<std::string>();
        
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error parsing user info response: " << e.what() << std::endl;
        return false;
    }
}

// Alternative simpler version that just returns the email
std::string GoogleOAuthPKCE::GetUserEmail()
{
    std::string email, name, picture;
    if (GetUserInfo(email, name, picture))
        return email;
    return "";
}
