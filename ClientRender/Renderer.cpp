#include "ClientRender/Renderer.h"
#include <libavstream/libavstream.hpp>
#include "TeleportClient/ServerTimestamp.h"
#include <regex>
#include "Tests.h"

avs::Timestamp clientrender::platformStartTimestamp ;
static bool timestamp_initialized=false;
using namespace clientrender;

Renderer::Renderer(clientrender::NodeManager *localNodeManager,clientrender::NodeManager *remoteNodeManager,SessionClient *sc,bool dev)
	:sessionClient(sc)
	,localGeometryCache(localNodeManager)
	, geometryCache(remoteNodeManager)
	, dev_mode(dev)
{
	sessionClient->SetSessionCommandInterface(this);
	if (!timestamp_initialized)
#ifdef _MSC_VER
		platformStartTimestamp = avs::PlatformWindows::getTimestamp();
#else
		platformStartTimestamp = avs::PlatformPOSIX::getTimestamp();
#endif
	timestamp_initialized=true;
	sessionClient->SetResourceCreator(&resourceCreator);
	sessionClient->SetGeometryCache(&geometryCache);
	resourceCreator.SetGeometryCache(&geometryCache);
	localResourceCreator.SetGeometryCache(&localGeometryCache);

	clientrender::Tests::RunAllTests();
}

void Renderer::Update(double timestamp_ms)
{
	double timeElapsed_s = (timestamp_ms - previousTimestamp) / 1000.0f;//ms to seconds

	teleport::client::ServerTimestamp::tick(timeElapsed_s);

	geometryCache.Update(static_cast<float>(timeElapsed_s));
	resourceCreator.Update(static_cast<float>(timeElapsed_s));

	localGeometryCache.Update(static_cast<float>(timeElapsed_s));
	localResourceCreator.Update(static_cast<float>(timeElapsed_s));

	previousTimestamp = timestamp_ms;
}

bool Renderer::Match(const std::string& full_string, const std::string& substring)
{
	try
	{
		std::regex regex(substring, std::regex_constants::icase | std::regex::extended);
		std::smatch match;
		if (std::regex_search(full_string, match, regex))
		{
			std::cout << "matches for '" << full_string << "'\n";
			std::cout << "Prefix: '" << match.prefix() << "'\n";
			for (size_t i = 0; i < match.size(); ++i)
				std::cout << i << ": " << match[i] << '\n';
			std::cout << "Suffix: '" << match.suffix() << "\'\n\n";
			return true;
		}
	}
	catch (std::exception&)
	{
		return false;
	}
	catch (...)
	{
		return false;
	}
	return false;
}