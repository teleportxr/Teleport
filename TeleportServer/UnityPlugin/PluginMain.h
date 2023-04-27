#pragma once
#include "libavstream/common.hpp"
#include <vector>
namespace teleport
{
	namespace server
	{
		extern std::vector<avs::uid> lostClients; //Clients who have been lost, and are awaiting deletion.
		extern avs::uid serverID;
	}
}