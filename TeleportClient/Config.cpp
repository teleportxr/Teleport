#include "Config.h"
#include "Platform/Core/SimpleIni.h"
#include "Platform/Core/FileLoader.h"
#include "TeleportCore/ErrorHandling.h"
#include "Platform/External/magic_enum/include/magic_enum.hpp"
#include <fmt/core.h>
#include <sstream>
#include <filesystem>

using namespace teleport;
using namespace client;
using std::string;
using namespace std::string_literals;

Config &Config::GetInstance()
{
	static teleport::client::Config config;
	return config;
}

void Config::LoadConfigFromIniFile()
{
	CSimpleIniA ini;
	auto *fileLoader=platform::core::FileLoader::GetFileLoader();
	if(!fileLoader)
		return;
	string str=fileLoader->LoadAsString("assets/client.ini");
	SI_Error rc = ini.LoadData(str);
	if(rc == SI_OK)
	{
		pause_for_debugger = ini.GetLongValue("", "PAUSE", pause_for_debugger);
		enable_vr = ini.GetLongValue("", "ENABLE_VR", enable_vr);
		dev_mode = ini.GetLongValue("", "DEV_MODE", dev_mode);
		log_filename = ini.GetValue("", "LOG_FILE", "TeleportClient.log");
	}
	else
	{
		TELEPORT_CERR<<"Create client.ini in pc_client directory to specify settings."<<std::endl;
	}
	#ifdef _MSC_VER
	if(pause_for_debugger)
		Sleep(pause_for_debugger*1000);
	#endif
}

const std::vector<Bookmark> &Config::GetBookmarks() const
{
	return bookmarks;
}

const std::vector<std::string> &Config::GetRecent() const
{
	return recent_server_urls;
}

void Config::AddBookmark(const Bookmark &b)
{
	bookmarks.push_back(b);
	SaveBookmarks();
}

void Config::LoadBookmarks()
{
	string bookmarks_str;
	std::string recent_str;
	auto *fileLoader=platform::core::FileLoader::GetFileLoader();
	if(fileLoader)
	{
		void *ptr=nullptr;
		unsigned bytelen=0;
		std::string filename=GetStoragePath()+"config/bookmarks.txt"s;
		fileLoader->AcquireFileContents(ptr,bytelen,filename.c_str(),true);
		if(ptr)
			bookmarks_str=(char*)ptr;
		fileLoader->ReleaseFileContents(ptr);

		recent_str=fileLoader->LoadAsString((GetStoragePath()+"config/recent_servers.txt"s).c_str());
	}
	if(bookmarks_str.length())
	{
		std::istringstream f(bookmarks_str);
		string line;    
		while (std::getline(f, line))
		{
			size_t split=line.find_first_of(' ');
			Bookmark b={line.substr(0,split),line.substr(split+1,line.length()-split-1)};
			AddBookmark(b);
		}
	}
	else
	{
		bookmarks.push_back({"192.168.3.40","192.168.3.40"});
		bookmarks.push_back({"test.teleportvr.io","test.teleportvr.io"});
		SaveBookmarks();
	}
	if(recent_str.length())
	{
		std::istringstream f(recent_str);
		string line;    
		while (std::getline(f, line))
		{
			while(line.length()>0&&line[line.length()-1]=='\r')
			{
				line=line.substr(0,line.length()-1);
			}
			while(line.length()>0&&line[line.length()-1]==' ')
			{
				line=line.substr(0,line.length()-1);
			}
			while(line.length()>0&&line[line.length()-1]=='\n')
			{
				line=line.substr(0,line.length()-1);
			}
			while(line.length()>0&&line[0]==' ')
			{
				line=line.substr(1,line.length()-1);
			}
			bool already=false;
			for(int i=0;i<recent_server_urls.size();i++)
			{
				if(recent_server_urls[i]==line)
				{
					already=true;
					break;
				}
			}
			if(already)
				continue;
			recent_server_urls.push_back(line);
		}
	}
}

void Config::SaveBookmarks()
{
//std::ofstream
	auto *fileLoader=platform::core::FileLoader::GetFileLoader();
	{
		string str;
		for(const auto &b:bookmarks)
		{
			str+=fmt::format("{0} {1}\n",b.url,b.title);
		}
		std::string filename=GetStoragePath()+"config/bookmarks.txt"s;
		fileLoader->Save(str.data(),(uint32_t)str.length(),filename.c_str(),true);
	}
}

void Config::LoadOptions()
{
	CSimpleIniA ini;
	auto *fileLoader=platform::core::FileLoader::GetFileLoader();
	if(!fileLoader)
		return;
	std::string filename=GetStoragePath()+"config/options.txt"s;
	string str=fileLoader->LoadAsString(filename.c_str());
	if(!str.length())
		return;	
	SI_Error rc = ini.LoadData(str);
	if(rc == SI_OK)
	{
		std::string s=ini.GetValue("", "LobbyView","");
		auto l=magic_enum::enum_cast<LobbyView>(s);
		if(l.has_value())
			options.lobbyView = l.value(); 
		std::string c=ini.GetValue("", "StartupConnectOption","");
		auto C=magic_enum::enum_cast<StartupConnectOption>(c);
		if(C.has_value())
			options.startupConnectOption = C.value(); 
	}
}

void Config::SaveOptions()
{
	auto *fileLoader=platform::core::FileLoader::GetFileLoader();
	{
		string str;
		str+=fmt::format("LobbyView={0}",magic_enum::enum_name(options.lobbyView));
		str+=fmt::format("\nStartupConnectOption={0}",magic_enum::enum_name(options.startupConnectOption));
		std::string filename=GetStoragePath()+"config/options.txt"s;
		fileLoader->Save(str.data(),(unsigned int)str.length(),filename.c_str(),true);
		LoadOptions();
	}
}

void Config::StoreRecentURL(const char *r)
{
	string s=r;
	if(s.length()==0)
		return;
	// If it's already in the recent list, move it to the front:
	for(int i=0;i<recent_server_urls.size();i++)
	{
		if(recent_server_urls[i]==s)
		{
			recent_server_urls.erase(recent_server_urls.begin()+i);
			i--;
		}
	}
	recent_server_urls.insert(recent_server_urls.begin(),s);
	
	auto *fileLoader=platform::core::FileLoader::GetFileLoader();
	//save recent:
	{
		string str;
		for(const auto &i:recent_server_urls)
		{
			str+=fmt::format("{0}\n",i);
		}
		std::string filename=GetStoragePath()+"config/recent_servers.txt";
		fileLoader->Save(str.data(),(unsigned int)str.length(),filename.c_str(),true);
	}
}

void Config::SetStorageFolder(const char *f)
{
	std::filesystem::path p(f);
	storageFolder = p.generic_string();
}

const std::string &Config::GetStoragePath() const
{
	if(!storageFolder.length())
		return storageFolder;
	static std::string str;
	str=storageFolder+"/"s;
	return str;
}

const std::string &Config::GetStorageFolder() const
{
	return storageFolder;
}