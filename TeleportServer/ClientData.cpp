#include "TeleportServer/ClientData.h"
#include "TeleportCore/Profiling.h" 
#include "TeleportCore/ErrorHandling.h" 
#include "TeleportServer/GeometryStore.h"
#include "TeleportServer/ClientManager.h"
#include "TeleportServer/InteropStructures.h"
using namespace teleport;
using namespace server;

void ClientData::SetConnectionState(ConnectionState c)
{
	if (connectionState == c)
		return;
	connectionState = c;
	if(connectionState!=CONNECTED)
	{
		// Have to assume that client has lost any information it might have had: 
		clientMessaging->setHasOrigin(false);
	}
}
ClientData::ClientData(avs::uid clid, std::shared_ptr<ClientMessaging> clientMessaging)
	:   clientMessaging(clientMessaging)
{
	videoEncodePipeline = std::make_shared<VideoEncodePipeline>();
	audioEncodePipeline = std::make_shared<AudioEncodePipeline>();
	clientID=clid;
}

void ClientData::StartStreaming(uint32_t connectionTimeout
	,uint64_t session_id
	,GetUnixTimestampFn getUnixTimestamp, int64_t startTimestamp_utc_unix_us
	,bool use_ssl)
{
	orthogonalNodeStates.clear();
	CasterEncoderSettings encoderSettings{};
	const ClientSettings *clientSettings = ClientManager::instance().GetClientSettings(clientID);
	if(clientSettings)
	{
		encoderSettings.frameWidth = clientSettings->videoTextureSize[0];
		encoderSettings.frameHeight = clientSettings->videoTextureSize[1];
	}
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
	setupCommand.debug_stream = serverSettings.debugStream;
	setupCommand.debug_network_packets = serverSettings.enableDebugNetworkPackets;
	setupCommand.requiredLatencyMs = serverSettings.requiredLatencyMs;
	setupCommand.idle_connection_timeout = connectionTimeout;

	setupCommand.session_id = session_id;
	// TODO: this must change:
	setupCommand.axesStandard = avs::AxesStandard::UnityStyle;
	setupCommand.audio_input_enabled = serverSettings.isReceivingAudio;
	setupCommand.startTimestamp_utc_unix_us = startTimestamp_utc_unix_us;
	setupCommand.using_ssl = use_ssl;
	setupCommand.backgroundMode = clientSettings->backgroundMode;
	setupCommand.backgroundColour = clientSettings->backgroundColour;
	setupCommand.draw_distance = clientSettings->drawDistance;
	setupCommand.backgroundTexture = clientSettings->backgroundTexture;

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
	videoConfig.webcam_width = clientSettings->webcamSize[0];
	videoConfig.webcam_height = clientSettings->webcamSize[1];
	videoConfig.webcam_offset_x = clientSettings->webcamPos[0];
	videoConfig.webcam_offset_y = clientSettings->webcamPos[1];
	videoConfig.use_10_bit_decoding = serverSettings.use10BitEncoding;
	videoConfig.use_yuv_444_decoding = serverSettings.useYUV444Decoding;
	videoConfig.use_alpha_layer_decoding = serverSettings.useAlphaLayerEncoding;
	videoConfig.colour_cubemap_size = serverSettings.captureCubeSize;
	videoConfig.compose_cube = encoderSettings.enableDecomposeCube;
	videoConfig.videoCodec = serverSettings.videoCodec;
	videoConfig.use_cubemap = !serverSettings.usePerspectiveRendering;
	videoConfig.stream_webcam = serverSettings.enableWebcamStreaming;

	// Set any static lighting textures to be required-streamable.
	clientMessaging->GetGeometryStreamingService().addGenericTexture(setupCommand.backgroundTexture);
	
	videoConfig.shadowmap_x = clientSettings->shadowmapPos[0];
	videoConfig.shadowmap_y = clientSettings->shadowmapPos[1];
	videoConfig.shadowmap_size = clientSettings->shadowmapSize;

	auto &global_illumination_texture_uids = getGlobalIlluminationTextures();
	teleport::core::SetLightingCommand setupLightingCommand(clientDynamicLighting);

	teleport::core::SetupInputsCommand setupInputsCommand((uint8_t)inputDefinitions.size());
	clientMessaging->sendSetupCommand(setupCommand,global_illumination_texture_uids, setupInputsCommand, inputDefinitions);
	clientMessaging->sendSetLightingCommand(setupLightingCommand);

	connectionState = CONNECTED;

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
	if (connectionState ==CONNECTED)
	{
		clientMessaging->setNodePosePath(nodeID,  regexPosePath);
		nodeSubTypes[nodeID].status = ReflectedStateStatus::SENT;
	}
}

void ClientData::reparentNode(avs::uid nodeID)
{
	auto node=GeometryStore::GetInstance().getNode(nodeID);
	if (!node)
		return;
	teleport::core::Pose pose;
	pose.orientation	= node->localTransform.rotation;
	pose.position		= node->localTransform.position;
	const auto &lastSetupCommand=clientMessaging->GetLastSetupCommand();
	avs::ConvertRotation(lastSetupCommand.axesStandard, clientMessaging->getClientNetworkContext()->axesStandard, pose.orientation);
	avs::ConvertPosition(lastSetupCommand.axesStandard, clientMessaging->getClientNetworkContext()->axesStandard, pose.position);
	teleport::core::UpdateNodeStructureCommand command(nodeID, node->parentID, pose);
	command.confirmationNumber = nextConfirmationNumber++;
	auto& s = orthogonalNodeStates[nodeID];
	if (!s)
		s = std::make_shared<OrthogonalNodeStateMap>();
	auto& st = s->unconfirmedStates[command.commandPayloadType];
	st.confirmationNumber = command.confirmationNumber;
	st.serverTimeSentUs = clientMessaging->GetServerTimeUs();
	clientMessaging->reparentNode(command);
}

void ClientData::tick(float deltaTime)
{
	TELEPORT_PROFILE_AUTOZONE;
	std::set<uint64_t> conf = clientMessaging->GetAndResetConfirmationsReceived();
	for (auto& nodeState : orthogonalNodeStates)
	{
		if (!nodeState.second)
			continue;
		for (auto& unconfirmedState : nodeState.second->unconfirmedStates)
		{
			if (conf.find(unconfirmedState.second.confirmationNumber) != conf.end())
			{
				unconfirmedState.second.confirmationNumber = 0;
			}
		}
	}
	resendUnconfirmedOrthogonalStates();
	clientMessaging->tick(deltaTime);
}

void ClientData::resendUnconfirmedOrthogonalStates()
{
	int64_t serverTimeUs = clientMessaging->GetServerTimeUs();
	// wait one second before resending
	static int64_t resendOrthogonalStateTimeout = 1000000 ;
	for (auto& nodeState : orthogonalNodeStates)
	{
		if (!nodeState.second)
			continue;
		for (auto& unconfirmedState : nodeState.second->unconfirmedStates)
		{
			if (unconfirmedState.second.confirmationNumber == 0)
				continue;
			if (serverTimeUs - unconfirmedState.second.serverTimeSentUs > resendOrthogonalStateTimeout)
			{
				TELEPORT_COUT << "Resending unconfirmed state for node " << nodeState.first << std::endl;
				unconfirmedState.second.serverTimeSentUs = serverTimeUs;
				switch (unconfirmedState.first)
				{
				case core::CommandPayloadType::UpdateNodeStructure:
					reparentNode(nodeState.first);
				default:
					break;
				}
			}
		}
	}
}
 
void ClientData::setInputDefinitions(const std::vector<teleport::core::InputDefinition>& inputDefs)
{
	inputDefinitions = inputDefs;
}

void ClientData::SetClientDynamicLighting(const teleport::core::ClientDynamicLighting &c)
{
	if(c.diffuse_cubemap_texture_uid!=clientDynamicLighting.diffuse_cubemap_texture_uid)
	{
		clientMessaging->GetGeometryStreamingService().removeGenericTexture(clientDynamicLighting.diffuse_cubemap_texture_uid);
		clientMessaging->GetGeometryStreamingService().addGenericTexture(c.diffuse_cubemap_texture_uid);
	}
	if(c.diffuse_cubemap_texture_uid!=clientDynamicLighting.specular_cubemap_texture_uid)
	{
		clientMessaging->GetGeometryStreamingService().removeGenericTexture(clientDynamicLighting.specular_cubemap_texture_uid);
		clientMessaging->GetGeometryStreamingService().addGenericTexture(c.specular_cubemap_texture_uid);
	}
	clientDynamicLighting=c;
	teleport::core::SetLightingCommand setupLightingCommand;
	setupLightingCommand.clientDynamicLighting=clientDynamicLighting;
	clientMessaging->sendSetLightingCommand(setupLightingCommand);
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
}


