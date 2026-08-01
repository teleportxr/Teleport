#include "IdentityStore.h"
#include "Config.h"
#include "Platform/Core/FileLoader.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#include <filesystem>
#include <nlohmann/json.hpp>

using namespace teleport;
using namespace client;
using nlohmann::json;

std::string IdentityStore::GetFilename()
{
	return (std::filesystem::path(Config::GetInstance().GetStorageFolder()) / "config" / "identity.json").string();
}

bool IdentityStore::Save(const IdentityProfile &profile)
{
	auto *fileLoader = platform::core::FileLoader::GetFileLoader();
	if (!fileLoader)
		return false;
	if (!profile.IsValid())
	{
		Clear();
		return true;
	}
	json j = {{"provider", profile.provider},
			  {"subject", profile.subject},
			  {"displayName", profile.displayName},
			  {"email", profile.email},
			  {"pictureUrl", profile.pictureUrl},
			  {"verifiedUnixTime", profile.verifiedUnixTime}};
	std::string str = j.dump(1, '\t');
	std::string filename = GetFilename();
	fileLoader->Save(str.data(), (uint32_t)str.length(), filename.c_str(), true);
	return true;
}

bool IdentityStore::Load(IdentityProfile &profile)
{
	auto *fileLoader = platform::core::FileLoader::GetFileLoader();
	if (!fileLoader)
		return false;
	std::string str = fileLoader->LoadAsString(GetFilename().c_str());
	if (str.empty())
		return false;
	try
	{
		json j				 = json::parse(str);
		profile.provider	 = j.value("provider", "");
		profile.subject		 = j.value("subject", "");
		profile.displayName	 = j.value("displayName", "");
		profile.email		 = j.value("email", "");
		profile.pictureUrl	 = j.value("pictureUrl", "");
		profile.verifiedUnixTime = j.value("verifiedUnixTime", (int64_t)0);
	}
	catch (const std::exception &e)
	{
		TELEPORT_WARN("IdentityStore: failed to parse {}: {}", GetFilename(), e.what());
		profile.Clear();
		return false;
	}
	if (!profile.IsValid())
	{
		profile.Clear();
		return false;
	}
	return true;
}

void IdentityStore::Clear()
{
	std::error_code ec;
	std::filesystem::remove(GetFilename(), ec);
}
