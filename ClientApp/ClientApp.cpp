#include "ClientApp.h"

using namespace teleport;
using namespace client;

ClientApp::ClientApp()
{
}

ClientApp::~ClientApp()
{
}

void ClientApp::Initialize()
{
	config.LoadConfigFromIniFile();
	config.LoadBookmarks();
}
