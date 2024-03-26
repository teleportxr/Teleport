#include "TabContext.h"
#include "SessionClient.h"
#include "TeleportCore/ErrorHandling.h"
#include <map>
#include <string>
#include <fmt/format.h>
#include "Platform/Core/StringToWString.h"
#ifdef _MSC_VER
// For ShellExecute
#include <shellapi.h>
#include <windows.h>
#include <Shlwapi.h>
#endif
#include "URLHandlers.h"
using namespace std::string_literals;

using namespace teleport;
using namespace client;

static std::set<int32_t> tabIndices;
static std::map<int32_t, std::shared_ptr<TabContext>> tabContexts;
static int32_t next_tab = 1;

const std::set<int32_t> &TabContext::GetTabIndices()
{
	return tabIndices;
}

std::shared_ptr<TabContext> TabContext::GetTabContext(int32_t index)
{
	auto i=tabContexts.find(index);
	if(i==tabContexts.end())
		return nullptr;
	return i->second;
}

int32_t TabContext::GetEmptyTabContext()
{
	for(auto i:tabIndices)
	{
		auto j=tabContexts.find(i);
		if(j!=tabContexts.end())
		{
			if(j->second->IsInUse())
			{
				return i;
			}
		}
	}
	return 0;
}

int32_t TabContext::AddTabContext()
{
	while (tabIndices.find(next_tab) != tabIndices.end())
	{
		next_tab++;
	}
	int32_t tab=next_tab;
	tabIndices.insert(tab);
	auto ctx = std::make_shared<TabContext>();
	tabContexts[tab]=ctx;
	next_tab++;
	return tab;
}

void TabContext::DeleteTabContext(int32_t index)
{
	auto i = tabIndices.find(index);
	if (i != tabIndices.end())
	{
		tabIndices.erase(i);
	}
	auto j = tabContexts.find(index);
	if (j != tabContexts.end())
	{
		tabContexts.erase(j);
	}
}
void TabContext::ConnectButtonHandler(int32_t tab_context_id, const std::string &url)
{
	auto tabContext = TabContext::GetTabContext(tab_context_id);
	if(tabContext)
		tabContext->ConnectTo(url);
}

void TabContext::CancelConnectButtonHandler(int32_t tab_context_id)
{
	auto tabContext = TabContext::GetTabContext(tab_context_id);
	if (tabContext)
		tabContext->CancelConnection();
}

bool TabContext::IsInUse() const
{
	return server_uid!=0;
}
static std::queue<std::string> externalURLs;
static bool shouldFollowExternalURL=false;

bool TabContext::ShouldFollowExternalURL()
{
	return shouldFollowExternalURL;
}

std::string TabContext::PopExternalURL()
{
	std::string url=externalURLs.front();
	externalURLs.pop();
	shouldFollowExternalURL=(externalURLs.size()>0);
	return url;
}

void TabContext::ConnectTo(std::string url)
{
	core::DomainPortPath domainPortPath = core::GetDomainPortPath(url);
	// Not Teleport protocol? launch some other app or ignore.
	if (domainPortPath.protocol != "teleport")
	{
		if(domainPortPath.protocol.length()>0)
		{
			externalURLs.push(url);
			shouldFollowExternalURL=true;
		}
		return;
	}
	// Question: Is this the current server domain? Or are we reconnecting to a server that already has a cache?
	auto currentSessionClient = SessionClient::GetSessionClient(server_uid);
	if(currentSessionClient)
	{
		// same domain? 
		if (currentSessionClient->domain == domainPortPath.domain)
		{
			next_server_uid=server_uid;
			server_uid=0;
		}
	}
	if (next_server_uid == 0)
	{
		next_server_uid = SessionClient::CreateSessionClient(this, domainPortPath.domain);
	}
	else
	{
		auto sc=SessionClient::GetSessionClient(next_server_uid);
		if(sc->domain!=domainPortPath.domain)
		{
			next_server_uid = SessionClient::CreateSessionClient(this, domainPortPath.domain);
		}
	}
	if (!next_server_uid)
		return;
	if (currentSessionClient && currentSessionClient->IsConnected())
	{
		if (currentSessionClient->GetConnectionURL() == url)
		{
			next_server_uid = 0;
			return;
		}
	}
	auto nextSessionClient = SessionClient::GetSessionClient(next_server_uid);
	if (!nextSessionClient)
	{
		next_server_uid = 0;
		return;
	}
	if (nextSessionClient->IsConnecting())
	{
		if (nextSessionClient->GetConnectionURL() == url)
			return;
		nextSessionClient->Disconnect(0);
	}
	nextSessionClient->RequestConnection(domainPortPath.path, domainPortPath.port);
	nextSessionClient->connected_url = url;
}

void TabContext::CancelConnection()
{
	if (server_uid != 0)
	{
		auto sc = SessionClient::GetSessionClient(server_uid);
		if (sc)
			sc->Disconnect(0);
	}
	if (next_server_uid != 0)
	{
		auto next = SessionClient::GetSessionClient(next_server_uid);
		if (next)
			next->Disconnect(0);
	}
}

void TabContext::ConnectionComplete(avs::uid uid)
{
	if (uid==next_server_uid)
	{
		auto currentSessionClient = SessionClient::GetSessionClient(server_uid);
		if (currentSessionClient)
			currentSessionClient->Disconnect(0);
		server_uid = next_server_uid;
		next_server_uid=0;
	}
}