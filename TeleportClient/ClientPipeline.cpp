#include "ClientPipeline.h"

using namespace teleport;
using namespace client;

ClientPipeline::ClientPipeline():
		decoder(avs::DecoderBackend::Custom)
{
}

ClientPipeline::~ClientPipeline()
{
}


void ClientPipeline::Shutdown()
{
	pipeline.deconfigure();
}
