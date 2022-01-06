#include "SimulCasterServer/ClientData.h"
using namespace teleport;
using namespace server;

ClientData::ClientData(std::shared_ptr<teleport::GeometryStreamingService> geometryStreamingService, std::shared_ptr<PluginVideoEncodePipeline> videoPipeline, std::shared_ptr<PluginAudioEncodePipeline> audioPipeline, const teleport::ClientMessaging& clientMessaging)
	: geometryStreamingService(geometryStreamingService), videoEncodePipeline(videoPipeline), audioEncodePipeline(audioPipeline), clientMessaging(clientMessaging)
{
	originClientHas.x = originClientHas.y = originClientHas.z = 0.f;
	memset(&clientSettings,0,sizeof(clientSettings));
}
void ClientData::StartStreaming(const teleport::CasterSettings& casterSettings
	,const teleport::CasterEncoderSettings& encoderSettings
	,uint32_t connectionTimeout
	,avs::uid serverID
	,GetUnixTimestampFn getUnixTimestamp
	,bool use_ssl)
{
	avs::SetupCommand setupCommand;
	setupCommand.server_streaming_port = clientMessaging.getServerPort() + 1;
	setupCommand.server_http_port = setupCommand.server_streaming_port + 1;
	setupCommand.debug_stream = casterSettings.debugStream;
	setupCommand.do_checksums = casterSettings.enableChecksums ? 1 : 0;
	setupCommand.debug_network_packets = casterSettings.enableDebugNetworkPackets;
	setupCommand.requiredLatencyMs = casterSettings.requiredLatencyMs;
	setupCommand.idle_connection_timeout = connectionTimeout;
	setupCommand.server_id = serverID;
	setupCommand.axesStandard = avs::AxesStandard::UnityStyle;
	setupCommand.audio_input_enabled = casterSettings.isReceivingAudio;
	setupCommand.control_model = casterSettings.controlModel;
	setupCommand.bodyOffsetFromHead = clientSettings.bodyOffsetFromHead;
	setupCommand.startTimestamp = getUnixTimestamp();
	setupCommand.using_ssl = use_ssl;

	avs::VideoConfig& videoConfig = setupCommand.video_config;
	videoConfig.video_width = encoderSettings.frameWidth;
	videoConfig.video_height = encoderSettings.frameHeight;
	videoConfig.depth_height = encoderSettings.depthHeight;
	videoConfig.depth_width = encoderSettings.depthWidth;
	videoConfig.perspective_width = casterSettings.perspectiveWidth;
	videoConfig.perspective_height = casterSettings.perspectiveHeight;
	videoConfig.perspective_fov = casterSettings.perspectiveFOV;
	videoConfig.webcam_width = clientSettings.webcamSize[0];
	videoConfig.webcam_height = clientSettings.webcamSize[1];
	videoConfig.webcam_offset_x = clientSettings.webcamPos[0];
	videoConfig.webcam_offset_y = clientSettings.webcamPos[1];
	videoConfig.use_10_bit_decoding = casterSettings.use10BitEncoding;
	videoConfig.use_yuv_444_decoding = casterSettings.useYUV444Decoding;
	videoConfig.use_alpha_layer_decoding = casterSettings.useAlphaLayerEncoding;
	videoConfig.colour_cubemap_size = casterSettings.captureCubeSize;
	videoConfig.compose_cube = encoderSettings.enableDecomposeCube;
	videoConfig.videoCodec = casterSettings.videoCodec;
	videoConfig.use_cubemap = !casterSettings.usePerspectiveRendering;
	videoConfig.stream_webcam = casterSettings.enableWebcamStreaming;
	videoConfig.draw_distance = casterSettings.detectionSphereRadius + casterSettings.clientDrawDistanceOffset;

	videoConfig.specular_cubemap_size = clientSettings.specularCubemapSize;

	// To the right of the depth cube, underneath the colour cube.
	videoConfig.specular_x = clientSettings.specularPos[0];
	videoConfig.specular_y = clientSettings.specularPos[1];

	videoConfig.specular_mips = clientSettings.specularMips;
	// To the right of the specular cube, after 3 mips = 1 + 1/2 + 1/4
	videoConfig.diffuse_cubemap_size = clientSettings.diffuseCubemapSize;
	// To the right of the depth map (if alpha layer encoding is disabled), under the specular map.
	videoConfig.diffuse_x = clientSettings.diffusePos[0];
	videoConfig.diffuse_y = clientSettings.diffusePos[1];

	videoConfig.light_cubemap_size = clientSettings.lightCubemapSize;
	// To the right of the diffuse map.
	videoConfig.light_x = clientSettings.lightPos[0];
	videoConfig.light_y = clientSettings.lightPos[1];
	videoConfig.shadowmap_x = clientSettings.shadowmapPos[0];
	videoConfig.shadowmap_y = clientSettings.shadowmapPos[1];
	videoConfig.shadowmap_size = clientSettings.shadowmapSize;
	clientMessaging.sendCommand(setupCommand);

	auto global_illumination_texture_uids = getGlobalIlluminationTextures();
	avs::SetupLightingCommand setupLightingCommand((uint8_t)global_illumination_texture_uids.size());
	clientMessaging.sendCommand(std::move(setupLightingCommand), global_illumination_texture_uids);

	isStreaming = true;

	for (auto s : nodeSubTypes)
	{
		clientMessaging.setNodeSubtype(s.first,s.second.state);
		s.second.status = ReflectedStateStatus::SENT;
	}
}

void ClientData::setNodeSubtype(avs::uid nodeID, avs::NodeSubtype subType)
{
	nodeSubTypes[nodeID].state = subType;
	nodeSubTypes[nodeID].status=ReflectedStateStatus::UNSENT;
	if (isStreaming)
	{
		clientMessaging.setNodeSubtype(nodeID, subType);
		nodeSubTypes[nodeID].status = ReflectedStateStatus::SENT;
	}
}

bool ClientData::setOrigin(uint64_t ctr,avs::vec3 pos,bool set_rel,avs::vec3 rel_to_head,avs::vec4 orientation)
{
	if(clientMessaging.hasPeer()&& clientMessaging.hasReceivedHandshake())
	{
		if(clientMessaging.setPosition(ctr,pos,set_rel,rel_to_head,orientation))
		{
		// ASSUME the message was received...
		// TODO: Only set this when client confirms.
			_hasOrigin=true;
			originClientHas=pos;
			return true;
		}
	}
	return false;
}

bool ClientData::isConnected() const
{
	return clientMessaging.hasPeer();
}

bool ClientData::hasOrigin() const
{
	if(clientMessaging.hasPeer())
	{
		return _hasOrigin;
	}
	_hasOrigin=false;
	return false;
}

avs::vec3 ClientData::getOrigin() const
{
	return originClientHas;
}

void ClientData::setGlobalIlluminationTextures(size_t num,const avs::uid *uids)
{
	if(num>255)
	{
		num=255;
		TELEPORT_CERR<<"Too many GI Textures."<<std::endl;
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
			geometryStreamingService->addGenericTexture(uids[i]);
		}
	}
	if (!isStreaming)
		return;
	if(changed)
	{
		avs::SetupLightingCommand setupLightingCommand;
		setupLightingCommand.num_gi_textures=(uint8_t)num;
		clientMessaging.sendCommand(std::move(setupLightingCommand), global_illumination_texture_uids);
	}
	
}