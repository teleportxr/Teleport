#include "TabContext.h"
#include "SessionClient.h"
#include <map>

using namespace teleport;
using namespace client;

static std::set<int32_t> tabIndices;
static std::map<int32_t,std::shared_ptr<TabContext>> tabContexts;

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

int32_t TabContext::AddTabContext()
{
	static int32_t next_tab = 1;
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


void TabContext::ConnectTo(std::string url)
{
	IpPort ipP = GetIpPort(url.c_str());
	if(server_uid==0)
	{
		server_uid=SessionClient::CreateSessionClient();
	}
	if(!server_uid)
		return;
	auto currentSessionClient = SessionClient::GetSessionClient(server_uid);
	if(!currentSessionClient)
	{
		server_uid = 0;
		return;
	}
	if(currentSessionClient->IsConnecting())
	{
		if(currentSessionClient->GetConnectionURL()==url)
			return;
		currentSessionClient->Disconnect(0);
	}
	if(currentSessionClient->IsConnected())
	{
		if (currentSessionClient->GetConnectionURL() == url)
			return;
		previous_server_uid = server_uid;
		server_uid = SessionClient::CreateSessionClient();
		currentSessionClient = SessionClient::GetSessionClient(server_uid);
	}
	currentSessionClient->RequestConnection(ipP.ip, ipP.port);
	currentSessionClient->connected_url = url;
}

void TabContext::CancelConnection()
{
	auto sc = SessionClient::GetSessionClient(server_uid);
	sc->Disconnect(0);
}