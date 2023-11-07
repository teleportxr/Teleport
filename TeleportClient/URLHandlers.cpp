#include "TeleportClient/URLHandlers.h"
#ifdef _MSC_VER
#include <Shlwapi.h>
#include <ShlGuid.h>
#endif
#include <regex>
#include "Platform/Core/StringToWString.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/URLParser.h"
using namespace teleport;
using namespace client;
static std::string GetAssocString(IQueryAssociations *pqa, ASSOCSTR stringType)
{
	wchar_t *pszExe = nullptr;
	WCHAR wszName[MAX_PATH];
	DWORD cchName = MAX_PATH;
	HRESULT hr = pqa->GetString(0, stringType, NULL, wszName, &cchName);
	if (SUCCEEDED(hr))
	{
		return platform::core::WStringToUtf8(wszName);
	}
	return "";
}

std::string teleport::client::GetLauncherForProtocol(std::string protocol)
{
	std::string exe;
	std::string cmd;
#ifdef _MSC_VER

	IQueryAssociations *pqa;
	HRESULT hr = AssocCreate(CLSID_QueryAssociations, IID_IQueryAssociations, (void **)&pqa);
	if (!SUCCEEDED(hr))
		return "";
	hr = pqa->Init(0, platform::core::StringToWString(protocol).c_str(), NULL, NULL);
	if (SUCCEEDED(hr))
	{
		exe = GetAssocString(pqa, ASSOCSTR_EXECUTABLE);
		cmd = GetAssocString(pqa, ASSOCSTR_COMMAND);
		TELEPORT_COUT << exe << "\n";
		TELEPORT_COUT << cmd << "\n";
	}
	pqa->Release();
	
#endif
	return exe;
}

void teleport::client::LaunchProtocolHandler( std::string url)
{
	core::DomainPortPath domainPortPath = core::GetDomainPortPath(url);
	LaunchProtocolHandler(domainPortPath.protocol,url);
}

void teleport::client::LaunchProtocolHandler(std::string protocol, std::string url)
{
	std::string cmd = teleport::client::GetLauncherForProtocol(protocol);
	if(!cmd.length())
		return;
	using std::string, std::regex;
	try
	{
		regex re("%1", std::regex_constants::icase);
		std::smatch match;
		// construct a string holding the results
		cmd=std::regex_replace(cmd, re, url);
	}
	catch (std::regex_error err)
	{
		TELEPORT_CERR << "Regex error for url " << url << ": " << err.what() << "\n";
	}
	catch (...)
	{
		TELEPORT_CERR << "Regex error for url " << url << "\n";
	}

	SHELLEXECUTEINFOA exInfo = {sizeof(SHELLEXECUTEINFOA)};
	exInfo.lpVerb = nullptr;
	exInfo.lpFile = url.c_str();
	exInfo.nShow = SW_SHOW;

	try
	{
		//ellExecuteA(0, nullptr, url.c_str(), 0, 0, SW_SHOW);
		ShellExecuteExA(&exInfo);
	}
	catch(...)
	{
	}

}