#include "Config.h"
#include "Platform/Core/SimpleIni.h"
#include "Platform/Core/FileLoader.h"
#include "TeleportCore/ErrorHandling.h"

using namespace teleport;
using namespace client;

void Config::LoadConfigFromIniFile()
{
	CSimpleIniA ini;
	std::string str;
	auto *fileLoader=platform::core::FileLoader::GetFileLoader();
	if(!fileLoader)
		return;
	void *ptr=nullptr;
	unsigned bytelen=0;
	fileLoader->AcquireFileContents(ptr,bytelen,"client.ini",true);
	if(ptr)
		str=(char*)ptr;
	fileLoader->ReleaseFileContents(ptr);
	SI_Error rc = ini.LoadData(str);
	if(rc == SI_OK)
	{
		std::string server_ip = ini.GetValue("", "SERVER_IP", TELEPORT_SERVER_IP);
		std::string ip_list;
		ip_list = ini.GetValue("", "SERVER_IP", "");

		size_t pos = 0;
		std::string token;
		do
		{
			pos = ip_list.find(",");
			std::string ip = ip_list.substr(0, pos);
			server_ips.push_back(ip);
			ip_list.erase(0, pos + 1);
		} while (pos != std::string::npos);

		enable_vr = ini.GetLongValue("", "ENABLE_VR", true);
		dev_mode = ini.GetLongValue("", "DEV_MODE", false);
		log_filename = ini.GetValue("", "LOG_FILE", "TeleportClient.log");

		render_local_offline = ini.GetLongValue("", "RENDER_LOCAL_OFFLINE", false);

	}
	else
	{
		TELEPORT_CERR<<"Create client.ini in pc_client directory to specify settings."<<std::endl;
	}
}
