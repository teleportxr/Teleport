#include "ClientPipeline.h"
#include <libavstream/network/webrtc_networksource.h>
#include "TeleportCore/ErrorHandling.h"

using namespace teleport;
using namespace client;

ClientPipeline::ClientPipeline()
{
	source.reset(new avs::WebRtcNetworkSource());
}

ClientPipeline::~ClientPipeline()
{
	Shutdown();
	unreliableToServerQueue.deconfigure();
	nodePosesQueue.deconfigure();
	inputStateQueue.deconfigure();
	source->deconfigure();
	videoQueue.deconfigure();
	decoder.deconfigure();
	surface.deconfigure();

	tagDataQueue.deconfigure();
	tagDataDecoder.deconfigure();

	geometryQueue.deconfigure();
	avsGeometryDecoder.deconfigure();
	avsGeometryTarget.deconfigure();

	audioQueue.deconfigure();
	avsAudioDecoder.deconfigure();
	avsAudioTarget.deconfigure();

	reliableOutQueue.deconfigure();
	commandDecoder.deconfigure();
}

bool ClientPipeline::Init(const teleport::core::SetupCommand& setupCommand, const char* server_ip)
{
	const uint32_t geoStreamID = 80;
	//TODO: these id's do NOT need to match the ones in the server. Only the labels need match.
	std::vector<avs::NetworkSourceStream> streams =
			{
				{20,"video"					,""				,"VideoQueue"	,true	,false}
				,{40,"video_tags"			,""				,"VideoTagQueue",true	,false}
				,{60,"audio_server_to_client",""			,"AudioQueue"	,true	,true}		// 2-way
				,{geoStreamID,"geometry"	,""				,"GeometryQueue",true	,false}
				,{100,"reliable"			,""				,"Reliable out"	,false	,false}		// 2-way
				,{120,"unreliable"			,"Unreliable in",""				,false	,true}					// 2-way
			};

	avs::NetworkSourceParams sourceParams;
	sourceParams.connectionTimeout = setupCommand.idle_connection_timeout;
	sourceParams.remoteIP = server_ip;
	sourceParams.maxHTTPConnections = 10;
	sourceParams.httpStreamID = geoStreamID;
	sourceParams.useSSL = setupCommand.using_ssl;

	// Configure for video stream, tag data stream, audio stream and geometry stream.
	if (!source->configure(std::move(streams), 3,sourceParams))
	{
		TELEPORT_BREAK_ONCE("Failed to configure network source node\n");
		return false;
	}

	source->setDebugStream(setupCommand.debug_stream);
	source->setDoChecksums(setupCommand.do_checksums);
	source->setDebugNetworkPackets(setupCommand.debug_network_packets);

	//test

	decoderParams.deferDisplay = false;
	decoderParams.decodeFrequency = avs::DecodeFrequency::NALUnit;
	decoderParams.codec = setupCommand.video_config.videoCodec;
	decoderParams.use10BitDecoding = setupCommand.video_config.use_10_bit_decoding;
	decoderParams.useYUV444ChromaFormat = setupCommand.video_config.use_yuv_444_decoding;
	decoderParams.useAlphaLayerDecoding = setupCommand.video_config.use_alpha_layer_decoding;

	pipeline.reset();
	// Top of the pipeline, we have the network source.
	pipeline.add(source.get());
	return true;
}

void ClientPipeline::Shutdown()
{
	pipeline.deconfigure();
}
