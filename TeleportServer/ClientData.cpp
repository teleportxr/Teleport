#include "TeleportServer/ClientData.h"
#include "TeleportServer/GeometryStore.h"
using namespace teleport;
using namespace server;

ClientData::ClientData(  std::shared_ptr<ClientMessaging> clientMessaging)
	:   clientMessaging(clientMessaging)
{
	videoEncodePipeline = std::make_shared<VideoEncodePipeline>();
	audioEncodePipeline = std::make_shared<AudioEncodePipeline>();
	memset(&clientSettings,0,sizeof(clientSettings));
}

void ClientData::StartStreaming(const ServerSettings& serverSettings
	,uint32_t connectionTimeout
	,avs::uid serverID
	,GetUnixTimestampFn getUnixTimestamp
	,bool use_ssl)
{
	clientMessaging->ConfirmSessionStarted();
	CasterEncoderSettings encoderSettings{};

	encoderSettings.frameWidth = clientSettings.videoTextureSize[0];
	encoderSettings.frameHeight = clientSettings.videoTextureSize[1];

	if (serverSettings.useAlphaLayerEncoding)
	{
		encoderSettings.depthWidth = 0;
		encoderSettings.depthHeight = 0;
	}
	else if (serverSettings.usePerspectiveRendering)
	{
		encoderSettings.depthWidth = static_cast<int32_t>(serverSettings.perspectiveWidth * 0.5f);
		encoderSettings.depthHeight = static_cast<int32_t>(serverSettings.perspectiveHeight * 0.5f);
	}
	else
	{
		encoderSettings.depthWidth = static_cast<int32_t>(serverSettings.captureCubeSize * 1.5f);
		encoderSettings.depthHeight = static_cast<int32_t>(serverSettings.captureCubeSize);
	}

	encoderSettings.wllWriteDepthTexture = false;
	encoderSettings.enableStackDepth = true;
	encoderSettings.enableDecomposeCube = true;
	encoderSettings.maxDepth = 10000;

	teleport::core::SetupCommand setupCommand;
	setupCommand.server_http_port = clientMessaging->getServerPort() + 1;
	setupCommand.server_streaming_port = clientMessaging->getStreamingPort();
	setupCommand.debug_stream = serverSettings.debugStream;
	setupCommand.do_checksums = serverSettings.enableChecksums ? 1 : 0;
	setupCommand.debug_network_packets = serverSettings.enableDebugNetworkPackets;
	setupCommand.requiredLatencyMs = serverSettings.requiredLatencyMs;
	setupCommand.idle_connection_timeout = connectionTimeout;
	setupCommand.server_id = serverID;
	setupCommand.axesStandard = avs::AxesStandard::UnityStyle;
	setupCommand.audio_input_enabled = serverSettings.isReceivingAudio;
	setupCommand.control_model = serverSettings.controlModel;
	setupCommand.startTimestamp_utc_unix_ms = getUnixTimestamp();
	setupCommand.using_ssl = use_ssl;
	setupCommand.backgroundMode = clientSettings.backgroundMode;
	setupCommand.backgroundColour = clientSettings.backgroundColour;
	setupCommand.draw_distance = clientSettings.drawDistance;

	// Often drawDistance will equal serverSettings.detectionSphereRadius + serverSettings.clientDrawDistanceOffset;
	// but this is for the server operator to decide.

	avs::VideoConfig& videoConfig = setupCommand.video_config;
	videoConfig.video_width = encoderSettings.frameWidth;
	videoConfig.video_height = encoderSettings.frameHeight;
	videoConfig.depth_height = encoderSettings.depthHeight;
	videoConfig.depth_width = encoderSettings.depthWidth;
	videoConfig.perspective_width = serverSettings.perspectiveWidth;
	videoConfig.perspective_height = serverSettings.perspectiveHeight;
	videoConfig.perspective_fov = serverSettings.perspectiveFOV;
	videoConfig.webcam_width = clientSettings.webcamSize[0];
	videoConfig.webcam_height = clientSettings.webcamSize[1];
	videoConfig.webcam_offset_x = clientSettings.webcamPos[0];
	videoConfig.webcam_offset_y = clientSettings.webcamPos[1];
	videoConfig.use_10_bit_decoding = serverSettings.use10BitEncoding;
	videoConfig.use_yuv_444_decoding = serverSettings.useYUV444Decoding;
	videoConfig.use_alpha_layer_decoding = serverSettings.useAlphaLayerEncoding;
	videoConfig.colour_cubemap_size = serverSettings.captureCubeSize;
	videoConfig.compose_cube = encoderSettings.enableDecomposeCube;
	videoConfig.videoCodec = serverSettings.videoCodec;
	videoConfig.use_cubemap = !serverSettings.usePerspectiveRendering;
	videoConfig.stream_webcam = serverSettings.enableWebcamStreaming;
	
	TELEPORT_ASSERT(sizeof(setupCommand.clientDynamicLighting)==sizeof(clientDynamicLighting));
	memcpy(&setupCommand.clientDynamicLighting,&clientDynamicLighting,sizeof(clientDynamicLighting));

	// Set any static lighting textures to be required-streamable.
	if(setupCommand.clientDynamicLighting.diffuseCubemapTexture)
	{
		clientMessaging->GetGeometryStreamingService().addGenericTexture(setupCommand.clientDynamicLighting.diffuseCubemapTexture);
	}
	if(setupCommand.clientDynamicLighting.specularCubemapTexture)
	{
		clientMessaging->GetGeometryStreamingService().addGenericTexture(setupCommand.clientDynamicLighting.specularCubemapTexture);
	}
	videoConfig.shadowmap_x = clientSettings.shadowmapPos[0];
	videoConfig.shadowmap_y = clientSettings.shadowmapPos[1];
	videoConfig.shadowmap_size = clientSettings.shadowmapSize;
	clientMessaging->sendSignalingCommand(setupCommand);

	auto global_illumination_texture_uids = getGlobalIlluminationTextures();
	teleport::core::SetupLightingCommand setupLightingCommand((uint8_t)global_illumination_texture_uids.size());
	clientMessaging->sendSignalingCommand(std::move(setupLightingCommand), global_illumination_texture_uids);

	
	teleport::core::SetupInputsCommand setupInputsCommand((uint8_t)inputDefinitions.size());
	
 	clientMessaging->sendSignalingCommand(setupInputsCommand, inputDefinitions);
	
	isStreaming = true;

	for (auto s : nodeSubTypes)
	{
		clientMessaging->setNodePosePath(s.first,s.second.state.regexPath);
		s.second.status = ReflectedStateStatus::SENT;
	}
}

void ClientData::setNodePosePath(avs::uid nodeID, const std::string &regexPosePath)
{
	nodeSubTypes[nodeID].state.regexPath = regexPosePath;
	nodeSubTypes[nodeID].status=ReflectedStateStatus::UNSENT;
	if (isStreaming)
	{
		clientMessaging->setNodePosePath(nodeID,  regexPosePath);
		nodeSubTypes[nodeID].status = ReflectedStateStatus::SENT;
	}
}
void ClientData::setInputDefinitions(const std::vector<teleport::core::InputDefinition>& inputDefs)
{
	inputDefinitions = inputDefs;
}

bool ClientData::setOrigin(uint64_t ctr,avs::uid uid)
{
	if(clientMessaging->hasPeer()&& clientMessaging->hasReceivedHandshake())
	{
		if(clientMessaging->setOrigin(ctr,uid))
		{
		// ASSUME the message was received...
		// TODO: Only set this when client confirms.
			_hasOrigin=true;
			originClientHas=uid;
			return true;
		}
	}
	TELEPORT_INTERNAL_CERR("Can't set origin - no handshake yet.\n");
	return false;
}

bool ClientData::isConnected() const
{
	if(!clientMessaging->hasPeer())
		return false;
	return true;
}

bool ClientData::hasOrigin() const
{
	if(clientMessaging->hasPeer())
	{
		return _hasOrigin;
	}
	_hasOrigin=false;
	return false;
}

avs::uid ClientData::getOrigin() const
{
	return originClientHas;
}

void ClientData::setGlobalIlluminationTextures(size_t num,const avs::uid *uids)
{
	if(num>255)
	{
		num=255;
		TELEPORT_CERR<<"Too many GI Textures.\n";
	}
	if(global_illumination_texture_uids.size()!=num)
		global_illumination_texture_uids.resize(num);
	bool changed=false;
	for(size_t i=0;i<num;i++)
	{
		if(global_illumination_texture_uids[i]!=uids[i])
		{
			changed=true;
			global_illumination_texture_uids[i] = uids[i];
			clientMessaging->GetGeometryStreamingService().addGenericTexture(uids[i]);
		}
	}
	if (!isStreaming)
		return;
	if(changed)
	{
		teleport::core::SetupLightingCommand setupLightingCommand;
		setupLightingCommand.num_gi_textures=(uint8_t)num;
		clientMessaging->sendCommand(std::move(setupLightingCommand), global_illumination_texture_uids);
	}
	
}


