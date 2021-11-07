#include "DefaultHTTPService.h"

#include "SimulCasterServer/ErrorHandling.h"
#include "SimulCasterServer/ClientData.h"
#include "SimulCasterServer/CasterSettings.h"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

//extern std::map<avs::uid, ClientData> clientServices;
//extern teleport::CasterSettings casterSettings;

namespace teleport
{
	bool DefaultHTTPService::initialize(std::string directoryPath)
	{
		if (directoryPath == "")
		{
			TELEPORT_CERR << "Path of directory to mount cannot be empty!" << std::endl;
			return false;
		}

		mServer.reset(new httplib::Server());

		// Mount / to provided directory
		auto ret = mServer->set_mount_point("/", directoryPath);
		if (!ret) {
			TELEPORT_CERR << "Provided mount directory does not exist!" << std::endl;
			return false;
		}

		return true;
	}

	void DefaultHTTPService::shutdown()
	{
		mServer.reset();
	}

	void DefaultHTTPService::tick()
	{

	}
}
