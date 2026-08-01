#include "ClientApp.h"
#include "TeleportClient/ClientTime.h"
#include "TeleportCore/Logging.h"
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
	// Config::LoadBookmarks() appends to the bookmark list and saves, so initializing twice would
	// duplicate the user's bookmarks on disk.
	if(initialized)
	{
		TELEPORT_WARN("ClientApp::Initialize() called more than once, ignoring.");
		return;
	}
	initialized=true;
	auto &config=Config::GetInstance();
	config.LoadConfigFromIniFile();
	config.LoadBookmarks();
	config.LoadOptions();
	ClientTime::GetInstance();
}
