#include "ClientPipeline.h"

using namespace teleport;
using namespace client;

ClientPipeline::ClientPipeline()
{
}

ClientPipeline::~ClientPipeline()
{
}


void ClientPipeline::Shutdown()
{
	pipeline.deconfigure();
}
