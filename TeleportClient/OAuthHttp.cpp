#include "OAuthHttp.h"
#include <curl/curl.h>
#include <mutex>

using nlohmann::json;

namespace
{
	size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *userp)
	{
		userp->append((char *)contents, size * nmemb);
		return size * nmemb;
	}

	//! Parse a reply that has already been fetched, applying the OAuth error convention.
	bool ParseReply(const std::string &readBuffer, json &responseOut, std::string &errorOut)
	{
		try
		{
			responseOut = json::parse(readBuffer);
		}
		catch (const std::exception &e)
		{
			errorOut = std::string("Malformed response: ") + e.what();
			return false;
		}
		if (responseOut.contains("error"))
		{
			errorOut = responseOut.value("error_description", responseOut.value("error", "unknown error"));
			return false;
		}
		return true;
	}
}

void teleport::client::oauth::EnsureCurlInitialised()
{
	static std::once_flag flag;
	std::call_once(flag, []() { curl_global_init(CURL_GLOBAL_ALL); });
}

std::string teleport::client::oauth::UrlEncode(const std::string &s)
{
	EnsureCurlInitialised();
	CURL *curl = curl_easy_init();
	if (!curl)
		return s;
	char *escaped = curl_easy_escape(curl, s.c_str(), (int)s.length());
	std::string result = escaped ? std::string(escaped) : s;
	if (escaped)
		curl_free(escaped);
	curl_easy_cleanup(curl);
	return result;
}

bool teleport::client::oauth::PostForm(const std::string &url, const std::string &postFields, json &responseOut, std::string &errorOut)
{
	EnsureCurlInitialised();
	CURL *curl = curl_easy_init();
	if (!curl)
	{
		errorOut = "Could not initialise curl.";
		return false;
	}
	std::string readBuffer;
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK)
	{
		errorOut = curl_easy_strerror(res);
		return false;
	}
	return ParseReply(readBuffer, responseOut, errorOut);
}

bool teleport::client::oauth::GetWithBearer(const std::string &url, const std::string &accessToken, json &responseOut, std::string &errorOut)
{
	EnsureCurlInitialised();
	CURL *curl = curl_easy_init();
	if (!curl)
	{
		errorOut = "Could not initialise curl.";
		return false;
	}
	std::string		   readBuffer;
	struct curl_slist *headers = nullptr;
	headers					   = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
	CURLcode res = curl_easy_perform(curl);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK)
	{
		errorOut = curl_easy_strerror(res);
		return false;
	}
	return ParseReply(readBuffer, responseOut, errorOut);
}

std::string teleport::client::oauth::ErrorCode(const json &response)
{
	if (response.is_object() && response.contains("error") && response["error"].is_string())
		return response["error"].get<std::string>();
	return std::string();
}
