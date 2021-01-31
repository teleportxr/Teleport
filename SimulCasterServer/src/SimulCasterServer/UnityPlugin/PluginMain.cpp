#include <functional>
#include <iostream>
#include <queue>
#include <sstream>
#include <vector>
#include <unordered_map>

#include "enet/enet.h"
#include "libavstream/common.hpp"

#include "SimulCasterServer/CasterSettings.h"
#include "SimulCasterServer/CaptureDelegates.h"
#include "SimulCasterServer/ClientData.h"
#include "SimulCasterServer/DefaultDiscoveryService.h"
#include "SimulCasterServer/GeometryStore.h"
#include "SimulCasterServer/GeometryStreamingService.h"
#include "SimulCasterServer/AudioEncodePipeline.h"
#include "SimulCasterServer/VideoEncodePipeline.h"

#include "Export.h"
#include "InteropStructures.h"
#include "PluginGraphics.h"
#include "SimulCasterServer/ErrorHandling.h"
#include "crossplatform/CustomAudioStreamTarget.h"

//#include <OAIdl.h>	// for SAFE_ARRAY

#ifdef _MSC_VER
#include "../VisualStudioDebugOutput.h"
VisualStudioDebugOutput debug_buffer(false, nullptr, 128);
#endif

using namespace SCServer;
TELEPORT_EXPORT void Client_StartSession(avs::uid clientID, int32_t listenPort);
TELEPORT_EXPORT void Client_StopStreaming(avs::uid clientID);
TELEPORT_EXPORT void Client_StopSession(avs::uid clientID);

TELEPORT_EXPORT void ConvertTransform(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, avs::Transform &transform)
{
	avs::ConvertTransform(fromStandard,toStandard,transform);
}
TELEPORT_EXPORT void ConvertRotation(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, avs::vec4 &rotation)
{
	avs::ConvertRotation(fromStandard,toStandard,rotation);
}
TELEPORT_EXPORT void ConvertPosition(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, avs::vec3 &position)
{
	avs::ConvertPosition(fromStandard,toStandard,position);
}
TELEPORT_EXPORT void ConvertScale(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, avs::vec3 &scale)
{
	avs::ConvertScale(fromStandard,toStandard,scale);
}
TELEPORT_EXPORT int8_t ConvertAxis(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, int8_t axis)
{
	return avs::ConvertAxis(fromStandard,toStandard,axis);
}


typedef void(__stdcall* SetHeadPoseFn) (avs::uid uid, const avs::Pose*);
typedef void(__stdcall* SetControllerPoseFn) (avs::uid uid, int index, const avs::Pose*);
typedef void(__stdcall* DisconnectFn) (avs::uid uid);
typedef void(__stdcall* ProcessAudioInputFn) (avs::uid uid, const uint8_t* data, size_t dataSize);

static avs::Context avsContext;

static std::shared_ptr<DefaultDiscoveryService> discoveryService = std::make_unique<DefaultDiscoveryService>();
static GeometryStore geometryStore;

std::map<avs::uid, ClientData> clientServices;

SCServer::CasterSettings casterSettings; //Unity side settings are copied into this, so inner-classes can reference this rather than managed code instance.

static std::function<bool(avs::uid clientID,avs::uid nodeID)> onShowActor;
static std::function<bool(avs::uid clientID,avs::uid nodeID)> onHideActor;

static SetHeadPoseFn setHeadPose;
static SetControllerPoseFn setControllerPose;
static ProcessNewInputFn processNewInput;
static DisconnectFn onDisconnect;
static ProcessAudioInputFn processAudioInput;
static ReportHandshakeFn reportHandshake;
static uint32_t connectionTimeout = 60000;
static avs::uid serverID = 0;

static std::set<avs::uid> unlinkedClientIDs; //Client IDs that haven't been linked to a session component.
static std::vector<avs::uid> lostClients; //Clients who have been lost, and are awaiting deletion.

static std::mutex audioMutex;
static std::mutex videoMutex;

// Messages related stuff
avs::MessageHandlerFunc messageHandler = nullptr;
struct LogMessage
{
	avs::LogSeverity severity;
	std::string msg;
	void* userData;
};

static std::vector<LogMessage> messages;
static std::mutex messagesMutex;


class PluginGeometryStreamingService : public SCServer::GeometryStreamingService
{
public:
	PluginGeometryStreamingService()
		:SCServer::GeometryStreamingService(&casterSettings)
	{
		this->geometryStore = &::geometryStore;
	}

	virtual ~PluginGeometryStreamingService() = default;
	
private:
	virtual void showActor_Internal(avs::uid clientID, avs::uid nodeID)
	{
		if(onShowActor)
		{
			if(!onShowActor(clientID,nodeID))
			{
				TELEPORT_CERR<<"onShowActor failed for node "<<nodeID<<"("<<geometryStore->getNodeName(nodeID)<<")"<<std::endl;
			}
		}
	}

	virtual void hideActor_Internal(avs::uid clientID, avs::uid nodeID)
	{
		if(onHideActor)
		{
			if(!onHideActor(clientID,nodeID))
			{
				TELEPORT_CERR<<"onHideActor failed for node "<<nodeID<<"("<<geometryStore->getNodeName(nodeID)<<")"<<std::endl;
			}
		}
	}
};

class PluginVideoEncodePipeline : public SCServer::VideoEncodePipeline
{
public:
	PluginVideoEncodePipeline() 
		:
		SCServer::VideoEncodePipeline(),
		inputSurfaceResource(nullptr),
		encoderSurfaceResource(nullptr),
		configured(false)
	{
		
	}

	~PluginVideoEncodePipeline()
	{
		deconfigure();
	}

	Result configure(const VideoEncodeParams& videoEncodeParams, avs::Queue* colorQueue)
	{
		if (configured)
		{
			TELEPORT_CERR << "Video encode pipeline already configured." << std::endl;
			return Result::EncoderAlreadyConfigured;
		}

		if (!GraphicsManager::mGraphicsDevice)
		{
			TELEPORT_CERR << "Graphics device handle is null. Cannot attempt to initialize video encode pipeline." << std::endl;
			return Result::InvalidGraphicsDevice;
		}

		if (!videoEncodeParams.inputSurfaceResource)
		{
			TELEPORT_CERR << "Surface resource handle is null. Cannot attempt to initialize video encode pipeline." << std::endl;
			return Result::InvalidGraphicsResource;
		}

		inputSurfaceResource = videoEncodeParams.inputSurfaceResource;
		// Need to make a copy because Unity uses a typeless format which is not compatible with CUDA
		encoderSurfaceResource = GraphicsManager::CreateTextureCopy(inputSurfaceResource);

		VideoEncodeParams params = videoEncodeParams;
		params.deviceHandle = GraphicsManager::mGraphicsDevice;
		params.inputSurfaceResource = encoderSurfaceResource;

		Result result = SCServer::VideoEncodePipeline::initialize(casterSettings, params, colorQueue);
		if (result)
		{
			configured = true;
		}
		return result;
	}

	Result reconfigure(const VideoEncodeParams& videoEncodeParams)
	{
		if (!configured)
		{
			TELEPORT_CERR << "Video encoder cannot be reconfigured if pipeline has not been configured." << std::endl;
			return Result::EncoderNotConfigured;
		}

		if (!GraphicsManager::mGraphicsDevice)
		{
			TELEPORT_CERR << "Graphics device handle is null. Cannot attempt to reconfigure video encode pipeline." << std::endl;
			return Result::InvalidGraphicsDevice;
		}

		if (videoEncodeParams.inputSurfaceResource)
		{
			TELEPORT_CERR << "Surface resource handle is null. Cannot attempt to reconfigure video encode pipeline." << std::endl;
			return Result::InvalidGraphicsResource;
		}

		VideoEncodeParams params = videoEncodeParams;
		params.deviceHandle = GraphicsManager::mGraphicsDevice;

		if (videoEncodeParams.inputSurfaceResource)
		{
			inputSurfaceResource = videoEncodeParams.inputSurfaceResource;
			// Need to make a copy because Unity uses a typeless format which is not compatible with CUDA
			encoderSurfaceResource = GraphicsManager::CreateTextureCopy(inputSurfaceResource);
			params.inputSurfaceResource = encoderSurfaceResource;
		}
		else
		{
			params.inputSurfaceResource = encoderSurfaceResource;
		}
		
		return SCServer::VideoEncodePipeline::reconfigure(casterSettings, params);
	}

	Result encode(const uint8_t* tagData, size_t tagDataSize, bool forceIDR = false)
	{
		if (!configured)
		{
			TELEPORT_CERR << "Video encoder cannot encode because it has not been configured." << std::endl;
			return Result::EncoderNotConfigured;
		}

		// Copy data from Unity texture to its CUDA compatible copy
		GraphicsManager::CopyResource(encoderSurfaceResource, inputSurfaceResource);

		return SCServer::VideoEncodePipeline::process(tagData, tagDataSize, forceIDR);
	}

	Result deconfigure() 
	{
		if (!configured)
		{
			TELEPORT_CERR << "Video encoder cannot be deconfigured because it has not been configured." << std::endl;
			return Result::EncoderNotConfigured;
		}

		Result result = release();
		if (!result)
		{
			return result;
		}
		
		GraphicsManager::ReleaseResource(encoderSurfaceResource);
		inputSurfaceResource = nullptr;

		configured = false;
		
		return result;
	}
	
private:
	void* inputSurfaceResource;
	void* encoderSurfaceResource;
	bool configured;
};

class PluginAudioEncodePipeline : public SCServer::AudioEncodePipeline
{
public:
	PluginAudioEncodePipeline()
		:
		SCServer::AudioEncodePipeline(),
		configured(false)
	{
		
	}

	~PluginAudioEncodePipeline()
	{
		
	}

	Result configure(const AudioParams& audioParams, avs::Queue* audioQueue)
	{
		if (configured)
		{
			TELEPORT_CERR << "Audio encode pipeline already configured." << std::endl;
			return Result::EncoderAlreadyConfigured;
		}

		Result result = SCServer::AudioEncodePipeline::initialize(casterSettings, audioParams, audioQueue);
		if (result)
		{
			configured = true;
		}
		return result;
	}

	Result sendAudio(const uint8_t* data, size_t dataSize)
	{
		if (!configured)
		{
			TELEPORT_CERR << "Audio encoder can not encode because it has not been configured." << std::endl;
			return Result::EncoderNotConfigured;
		}

		return SCServer::AudioEncodePipeline::process(data, dataSize);
	}

private:
	bool configured;
};

struct InitialiseState
{
	bool(*showActor)(avs::uid clientID, avs::uid nodeID);
	bool(*hideActor)(avs::uid clientID, avs::uid nodeID);
	void(*headPoseSetter)(avs::uid clientID, const avs::Pose*);
	void(*controllerPoseSetter)(avs::uid uid, int index, const avs::Pose*);
	ProcessNewInputFn newInputProcessing;
	DisconnectFn disconnect;
	avs::MessageHandlerFunc messageHandler;
	uint32_t DISCOVERY_PORT = 10607;
	uint32_t SERVICE_PORT = 10500;
	void(*reportHandshake)(avs::uid clientID, const avs::Handshake *h);
	ProcessAudioInputFn processAudioInput;
};

///PLUGIN-INTERNAL START
void RemoveClient(avs::uid clientID)
{
	std::lock_guard<std::mutex> videoLock(videoMutex);
	std::lock_guard<std::mutex> audioLock(audioMutex);

	// Early-out if a client with this ID doesn't exist.
	auto& clientIt = clientServices.find(clientID);
	if (clientIt == clientServices.end())
		return;

	ClientData& client = clientIt->second;

	client.clientMessaging.stopSession();

	// Remove references to lost client.
	clientServices.erase(clientID);
}

void Disconnect(avs::uid clientID)
{
	onDisconnect(clientID);
	Client_StopStreaming(clientID);
}

void ProcessAudioInput(avs::uid clientID, const uint8_t* data, size_t dataSize)
{
	processAudioInput(clientID, data, dataSize);
}

///PLUGIN-INTERNAL END

///MEMORY-MANAGEMENT START
TELEPORT_EXPORT void DeleteUnmanagedArray(void** unmanagedArray)
{
	delete[] *unmanagedArray;
}
///MEMORY-MANAGEMENT END

///PLUGIN-SPECIFIC START
TELEPORT_EXPORT void UpdateCasterSettings(const SCServer::CasterSettings newSettings)
{
	casterSettings = newSettings;
}

TELEPORT_EXPORT void SetShowActorDelegate(bool(*showActor)(avs::uid,avs::uid))
{
	onShowActor = showActor;
}

TELEPORT_EXPORT void SetHideActorDelegate(bool(*hideActor)(avs::uid,avs::uid))
{
	onHideActor = hideActor;
}

TELEPORT_EXPORT void SetHeadPoseSetterDelegate(SetHeadPoseFn headPoseSetter)
{
	setHeadPose = headPoseSetter;
}

TELEPORT_EXPORT void SetControllerPoseSetterDelegate(SetControllerPoseFn f)
{
	setControllerPose = f;
}

TELEPORT_EXPORT void SetNewInputProcessingDelegate(ProcessNewInputFn newInputProcessing)
{
	processNewInput = newInputProcessing;
}

TELEPORT_EXPORT void SetDisconnectDelegate(DisconnectFn disconnect)
{
	onDisconnect = disconnect;
}

TELEPORT_EXPORT void SetProcessAudioInputDelegate(ProcessAudioInputFn f)
{
	processAudioInput = f;
}

static void passOnOutput(const char *msg)
{
	if(msg)
		avsContext.log(avs::LogSeverity::Warning,msg);
}

static void passOnError(const char *msg)
{
	if(msg)
		avsContext.log(avs::LogSeverity::Error,msg);
}

void AccumulateMessagesFromThreads(avs::LogSeverity severity, const char* msg, void* userData)
{
	std::lock_guard<std::mutex> lock(messagesMutex);
	if(severity==avs::LogSeverity::Error|| severity==avs::LogSeverity::Critical)
	{
		LogMessage tst={severity,msg,userData};
		// can break here.
	}
	if(messages.size()==99)
	{
		LogMessage logMessage={avs::LogSeverity::Error,"Too many messages since last call to PipeOutMessages()",nullptr};
		messages.push_back(std::move(logMessage));
		return;
	}
	else if(messages.size()>99)
	{
		return;
	}
	LogMessage logMessage={severity,msg,userData};
	messages.push_back(std::move(logMessage));
}

void PipeOutMessages()
{
	std::lock_guard<std::mutex> lock(messagesMutex);
	if(messageHandler)
	{
		for(auto m:messages)
		{
			messageHandler(m.severity,m.msg.c_str(),m.userData);
		}
		messages.clear();
	}
}

TELEPORT_EXPORT void SetMessageHandlerDelegate(avs::MessageHandlerFunc msgh)
{
	messageHandler=msgh;
	avsContext.setMessageHandler(AccumulateMessagesFromThreads, nullptr);
	debug_buffer.setOutputCallback(&passOnOutput);
	debug_buffer.setErrorCallback(&passOnError);
}

TELEPORT_EXPORT void SetConnectionTimeout(int32_t timeout)
{
	connectionTimeout = timeout;
}

TELEPORT_EXPORT bool Initialise(const InitialiseState *initialiseState)
{
	serverID = avs::GenerateUid();

	SetShowActorDelegate(initialiseState->showActor);
	SetHideActorDelegate(initialiseState->hideActor);
	SetHeadPoseSetterDelegate(initialiseState->headPoseSetter);
	SetControllerPoseSetterDelegate(initialiseState->controllerPoseSetter);
	SetNewInputProcessingDelegate(initialiseState->newInputProcessing);
	SetDisconnectDelegate(initialiseState->disconnect);
	SetMessageHandlerDelegate(initialiseState->messageHandler);
	SetProcessAudioInputDelegate(initialiseState->processAudioInput);

	reportHandshake=initialiseState->reportHandshake;

	if(enet_initialize() != 0)
	{
		TELEPORT_CERR<<"An error occurred while attempting to initalise ENet!\n";
		return false;
	}
	atexit(enet_deinitialize);

	ClientMessaging::startAsyncNetworkDataProcessing();

	return discoveryService->initialise(initialiseState->DISCOVERY_PORT,initialiseState->SERVICE_PORT);
}

TELEPORT_EXPORT void Shutdown()
{
	std::lock_guard<std::mutex> videoLock(videoMutex);
	std::lock_guard<std::mutex> audioLock(audioMutex);

	discoveryService->shutdown();

	ClientMessaging::stopAsyncNetworkDataProcessing(true);

	for(auto& clientService : clientServices)
	{
		ClientData& client = clientService.second;
		// Stre
		if (client.isStreaming)
		{
			// This will add to lost clients but will be cleared below
			Client_StopStreaming(clientService.first);
		}
		client.clientMessaging.stopSession();
	}

	lostClients.clear();
	clientServices.clear();

	onShowActor = nullptr;
	onHideActor = nullptr;

	setHeadPose = nullptr;
	setControllerPose = nullptr;
	processNewInput = nullptr;
}

ClientData::ClientData(std::shared_ptr<PluginGeometryStreamingService> gs, std::shared_ptr<PluginVideoEncodePipeline> vep, std::shared_ptr<PluginAudioEncodePipeline> aep, std::function<void(void)> disconnect)
	: geometryStreamingService(gs)
	, videoEncodePipeline(vep)
	, audioEncodePipeline(aep)
	, clientMessaging(&casterSettings, discoveryService, geometryStreamingService, setHeadPose, setControllerPose, processNewInput, disconnect, connectionTimeout,reportHandshake)
	
{
	originClientHas.x= originClientHas.y= originClientHas.z=0.f;
}

TELEPORT_EXPORT void Client_StartSession(avs::uid clientID, int32_t listenPort)
{
	// Already started this session.
	if(clientServices.find(clientID) == clientServices.end())
	{
		ClientData newClientData(std::make_shared<PluginGeometryStreamingService>(), std::make_shared<PluginVideoEncodePipeline>(), std::make_shared<PluginAudioEncodePipeline>(), std::bind(&Disconnect, clientID));
	
		if(newClientData.clientMessaging.startSession(clientID, listenPort))
		{
			clientServices.emplace(clientID, std::move(newClientData));
		}
		else
		{
			TELEPORT_CERR << "Failed to start session for client: " << clientID << std::endl;
		}
	}
	else
	{
		TELEPORT_CERR << "Already got client: " << clientID << std::endl;
		// already got this client. This can happen if the client thinks it's disconnected but we didn't know that.
//		auto &c= clientServices.find(clientID);
//		if(c!=clientServices.end())
//			c->second.clientMessaging.unInitialise();
	}
	auto &c= clientServices.find(clientID);
	if(c==clientServices.end())
		return;
	ClientData& newClient = c->second;
	if(newClient.clientMessaging.isInitialised())
		return;

	// Sending
	newClient.casterContext.ColorQueue = std::make_unique<avs::Queue>();
	newClient.casterContext.GeometryQueue = std::make_unique<avs::Queue>();
	newClient.casterContext.AudioQueue = std::make_unique<avs::Queue>();

	newClient.casterContext.ColorQueue->configure(16,"ColorQueue");
	newClient.casterContext.GeometryQueue->configure(16, "GeometryQueue");
	newClient.casterContext.AudioQueue->configure(120, "AudioQueue");

	// Receiving
	if (casterSettings.isReceivingAudio)
	{
		newClient.casterContext.sourceAudioQueue = std::make_unique<avs::Queue>();
		newClient.casterContext.audioDecoder = std::make_unique<avs::AudioDecoder>();
		newClient.casterContext.audioTarget = std::make_unique<avs::AudioTarget>();
		newClient.casterContext.audioStreamTarget = std::make_unique<sca::CustomAudioStreamTarget>(std::bind(&ProcessAudioInput, clientID, std::placeholders::_1, std::placeholders::_2));

		newClient.casterContext.sourceAudioQueue->configure(120, "SourceAudioQueue");
		newClient.casterContext.audioDecoder->configure(100);
		newClient.casterContext.audioTarget->configure(newClient.casterContext.audioStreamTarget.get());
	}

	///TODO: Initialise real delegates for capture component.
	SCServer::CaptureDelegates delegates;
	delegates.startStreaming = [](SCServer::CasterContext* context){};
	delegates.requestKeyframe = [&newClient]()
	{
		newClient.videoKeyframeRequired = true;
	};
	delegates.getClientCameraInfo = []()->SCServer::CameraInfo&
	{
		static SCServer::CameraInfo c;
		return c;
	};

	newClient.clientMessaging.initialise(&newClient.casterContext, delegates);

	unlinkedClientIDs.insert(clientID);
	
}

TELEPORT_EXPORT void Client_StopSession(avs::uid clientID)
{
	// Early-out if a client with this ID doesn't exist.
	auto& clientIt = clientServices.find(clientID);
	if(clientIt == clientServices.end())
		return;
		
	ClientData& client = clientIt->second;
	// Shut-down connections to the client.
	if (client.isStreaming)
	{
		// Will add to lost clients and call shutdown command
		Client_StopStreaming(clientID);
	}

	RemoveClient(clientID);

	auto iter = lostClients.begin();
	while (iter != lostClients.end())
	{
		if (*iter == clientID)
		{
			// Continue checking rest of container just in case client ID was added more than once
			iter = lostClients.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

TELEPORT_EXPORT void Client_StartStreaming(avs::uid clientID)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
	{
		return;
	}
	ClientData& client = c->second;
	client.geometryStreamingService->startStreaming(&client.casterContext);

	///TODO: Need to retrieve encoder settings from unity.
	SCServer::CasterEncoderSettings encoderSettings;
	if (casterSettings.usePerspectiveRendering)
	{
		encoderSettings.frameWidth = casterSettings.sceneCaptureWidth;
		encoderSettings.frameHeight = casterSettings.sceneCaptureHeight;
	}
	else
	{
		encoderSettings.frameWidth = static_cast<int32_t>(casterSettings.captureCubeSize * 3);
		encoderSettings.frameHeight = static_cast<int32_t>(casterSettings.captureCubeSize * 3);
	}
	encoderSettings.depthWidth = 0; // not used
	encoderSettings.depthHeight = 0; // not used
	encoderSettings.wllWriteDepthTexture = false;
	encoderSettings.enableStackDepth = true;
	encoderSettings.enableDecomposeCube = true;
	encoderSettings.maxDepth = 10000;

	avs::SetupCommand setupCommand;
	setupCommand.port = c->second.clientMessaging.getServerPort() + 1;
	setupCommand.debug_stream = casterSettings.debugStream;
	setupCommand.do_checksums = casterSettings.enableChecksums ? 1 : 0;
	setupCommand.debug_network_packets = casterSettings.enableDebugNetworkPackets;
	setupCommand.requiredLatencyMs = casterSettings.requiredLatencyMs;
	setupCommand.idle_connection_timeout = connectionTimeout;
	setupCommand.server_id = serverID;
	setupCommand.axesStandard = avs::AxesStandard::UnityStyle;
	setupCommand.audio_input_enabled = casterSettings.isReceivingAudio;
	setupCommand.lock_player_height = casterSettings.lockPlayerHeight;

	avs::VideoConfig& videoConfig		= setupCommand.video_config;
	videoConfig.video_width				= encoderSettings.frameWidth;
	videoConfig.video_height			= encoderSettings.frameHeight;
	videoConfig.depth_height			= encoderSettings.depthHeight;
	videoConfig.depth_width				= encoderSettings.depthWidth;
	videoConfig.perspectiveFOV			= casterSettings.perspectiveFOV;
	videoConfig.use_10_bit_decoding		= casterSettings.use10BitEncoding;
	videoConfig.use_yuv_444_decoding	= casterSettings.useYUV444Decoding;
	videoConfig.colour_cubemap_size		= encoderSettings.frameWidth / 3;
	videoConfig.compose_cube			= encoderSettings.enableDecomposeCube;
	videoConfig.videoCodec				= casterSettings.videoCodec;
	videoConfig.use_cubemap				= !casterSettings.usePerspectiveRendering;
	videoConfig.stream_webcam			= casterSettings.enableWebcamStreaming;

	videoConfig.specular_cubemap_size	=casterSettings.specularCubemapSize;
	int depth_cubemap_size				=videoConfig.colour_cubemap_size/2;
	// To the right of the depth cube, underneath the colour cube.
	videoConfig.specular_x			=depth_cubemap_size*3;
	videoConfig.specular_y			=videoConfig.colour_cubemap_size*2;
	
	videoConfig.rough_cubemap_size	=casterSettings.roughCubemapSize;
	// To the right of the specular cube, after 3 mips = 1 + 1/2 + 1/4
	videoConfig.rough_x				=videoConfig.specular_x+(casterSettings.specularCubemapSize*3*7)/4;
	videoConfig.rough_y				=videoConfig.specular_y;
	
	videoConfig.diffuse_cubemap_size=casterSettings.diffuseCubemapSize;
	// To the right of the depth map, under the specular map.
	videoConfig.diffuse_x			=depth_cubemap_size*3;
	videoConfig.diffuse_y			=videoConfig.specular_y+casterSettings.specularCubemapSize*2;
	
	videoConfig.light_cubemap_size	=casterSettings.lightCubemapSize;
	// To the right of the diffuse map.
	videoConfig.light_x				=videoConfig.diffuse_x+(casterSettings.diffuseCubemapSize*3*7)/4;
	videoConfig.light_y				=videoConfig.diffuse_y;
	///TODO: Initialise actors in range.
	
	videoConfig.shadowmap_x		=videoConfig.diffuse_x;
	videoConfig.shadowmap_y		=videoConfig.diffuse_y+2*videoConfig.diffuse_cubemap_size;
	videoConfig.shadowmap_size	=64;
	client.clientMessaging.sendCommand(std::move(setupCommand));

	client.isStreaming = true;
}

TELEPORT_EXPORT void Client_StopStreaming(avs::uid clientID)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end()) return;

	ClientData& lostClient = c->second;

	lostClient.clientMessaging.sendCommand(avs::ShutdownCommand());
	lostClient.isStreaming = false;

	//Delay deletion of clients.
	lostClients.push_back(clientID);
}

TELEPORT_EXPORT void Tick(float deltaTime)
{
	//Delete client data for clients who have been lost.
	if(lostClients.size() != 0)
	{
		for(avs::uid clientID : lostClients)
		{
			RemoveClient(clientID);
		}
		lostClients.clear();
	}

	for(auto& idClientPair : clientServices)
	{
		ClientData &clientData=idClientPair.second;
		clientData.clientMessaging.handleEvents(deltaTime);

		if(clientData.clientMessaging.hasPeer())
		{
			clientData.clientMessaging.tick(deltaTime);

			if(clientData.isStreaming == false)
			{
				Client_StartStreaming(idClientPair.first);
			}
		}
	}

	discoveryService->tick();
	PipeOutMessages();
}

TELEPORT_EXPORT void Tock()
{
	PipeOutMessages();
}

TELEPORT_EXPORT bool Client_SetOrigin(avs::uid clientID, const avs::vec3 *pos, bool set_rel,const avs::vec3 *orig_to_head)
{
	auto &c=clientServices.find(clientID);
	if(c==clientServices.end())
		return false;
	ClientData &clientData=c->second;
	return clientData.setOrigin(*pos,set_rel,*orig_to_head);
}

TELEPORT_EXPORT bool Client_IsConnected(avs::uid clientID)
{
	auto &c=clientServices.find(clientID);
	if(c==clientServices.end())
		return false;
	ClientData &clientData=c->second;
	return clientData.isConnected();
}

TELEPORT_EXPORT bool Client_HasOrigin(avs::uid clientID, avs::vec3* pos)
{
	auto &c=clientServices.find(clientID);
	if(c==clientServices.end())
		return false;
	ClientData &clientData=c->second;
	bool result=(clientData.hasOrigin());
	if(result&&pos)
		*pos=clientData.getOrigin();
	return result;
}

TELEPORT_EXPORT void Reset()
{
	for(auto& clientIDInfoPair : clientServices)
	{
		clientIDInfoPair.second.geometryStreamingService->reset();
	}
}

TELEPORT_EXPORT avs::uid GetUnlinkedClientID()
{
	if(unlinkedClientIDs.size() != 0)
	{
		avs::uid clientID = *unlinkedClientIDs.begin();
		unlinkedClientIDs.erase(unlinkedClientIDs.begin());

		return clientID;
	}
	else
	{
		return 0;
	}
}
///PLUGIN-SPECIFC END

///libavstream START
TELEPORT_EXPORT avs::uid GenerateID()
{
	return avs::GenerateUid();
}

TELEPORT_EXPORT float GetBandwidthInKbps(avs::uid clientID)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
		return 0;
	return c->second.clientMessaging.getBandWidthKPS();
}
///libavstream END

///GeometryStreamingService START
TELEPORT_EXPORT void Client_AddNode(avs::uid clientID,  avs::uid actorID, avs::Transform currentTransform)
{
	auto c= clientServices.find(clientID);
	if(c==clientServices.end())
		return;
	c->second.geometryStreamingService->addNode( actorID);
	//Update node transform as it may have changed since the actor was last streamed.
	geometryStore.updateNode(actorID, currentTransform);
}

TELEPORT_EXPORT avs::uid Client_RemoveNodeByID(avs::uid clientID, avs::uid actorID)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
		return 0;
	return c->second.geometryStreamingService->removeNodeByID(actorID);
}

TELEPORT_EXPORT bool Client_IsStreamingNodeID(avs::uid clientID, avs::uid actorID)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
		return false;
	return c->second.geometryStreamingService->isStreamingNodeID(actorID);
}

TELEPORT_EXPORT void Client_ShowActor(avs::uid clientID, avs::uid actorID)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
		return;
	c->second.geometryStreamingService->showNode(clientID,actorID);
}

TELEPORT_EXPORT void Client_HideActor(avs::uid clientID, avs::uid actorID)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
		return;
	c->second.geometryStreamingService->hideNode(clientID,actorID);
}

TELEPORT_EXPORT void Client_SetActorVisible(avs::uid clientID, avs::uid actorID, bool isVisible)
{
	if(isVisible)
		Client_ShowActor(clientID, actorID);
	else
		Client_HideActor(clientID, actorID);
}

TELEPORT_EXPORT bool Client_IsClientRenderingActorID(avs::uid clientID, avs::uid actorID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end()) return false;

	return clientPair->second.geometryStreamingService->isClientRenderingNode(actorID);
}

bool Client_HasResource(avs::uid clientID, avs::uid resourceID)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
	{
		return false;
	}
	return c->second.geometryStreamingService->hasResource(resourceID);
}
///GeometryStreamingService END


///VideoEncodePipeline START
TELEPORT_EXPORT void InitializeVideoEncoder(avs::uid clientID, SCServer::VideoEncodeParams& videoEncodeParams)
{
	std::lock_guard<std::mutex> lock(videoMutex);

	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
	{
		return;
	}

	auto& clientData = c->second;
	avs::Queue *q	=clientData.casterContext.ColorQueue.get();
	Result result	=clientData.videoEncodePipeline->configure(videoEncodeParams, q);
	if(!result)
	{
		TELEPORT_CERR << "Error occurred when trying to configure the video encode pipeline" << std::endl;
	}
}

TELEPORT_EXPORT void ReconfigureVideoEncoder(avs::uid clientID, SCServer::VideoEncodeParams& videoEncodeParams)
{
	std::lock_guard<std::mutex> lock(videoMutex);

	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
	{
		return;
	}

	auto& clientData = c->second;
	Result result = clientData.videoEncodePipeline->reconfigure(videoEncodeParams);
	if (!result)
	{
		TELEPORT_CERR << "Error occurred when trying to reconfigure the video encode pipeline" << std::endl;
		return;
	}

	///TODO: Need to retrieve encoder settings from unity.
	SCServer::CasterEncoderSettings encoderSettings
	{
		videoEncodeParams.encodeWidth,
		videoEncodeParams.encodeHeight,
		0, // not used
		0, // not used
		false,
		true,
		true,
		10000
	};
	avs::ReconfigureVideoCommand cmd;
	avs::VideoConfig& videoConfig = cmd.video_config;
	videoConfig.video_width = encoderSettings.frameWidth;
	videoConfig.video_height = encoderSettings.frameHeight;
	videoConfig.depth_height = encoderSettings.depthHeight;
	videoConfig.depth_width = encoderSettings.depthWidth;
	videoConfig.perspectiveFOV = casterSettings.perspectiveFOV;
	videoConfig.use_10_bit_decoding = casterSettings.use10BitEncoding;
	videoConfig.use_yuv_444_decoding = casterSettings.useYUV444Decoding;
	videoConfig.colour_cubemap_size = encoderSettings.frameWidth / 3;
	videoConfig.compose_cube = encoderSettings.enableDecomposeCube;
	videoConfig.videoCodec = casterSettings.videoCodec;
	videoConfig.use_cubemap = !casterSettings.usePerspectiveRendering;

	c->second.clientMessaging.sendCommand(cmd);
}

TELEPORT_EXPORT void EncodeVideoFrame(avs::uid clientID, const uint8_t* tagData, size_t tagDataSize)
{
	std::lock_guard<std::mutex> lock(videoMutex);

	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
	{
		return;
	}
	auto& clientData = c->second;
	if(!clientData.clientMessaging.hasPeer())
	{
		TELEPORT_CERR<< "EncodeVideoFrame called but peer is not connected." << std::endl;
		return;
	}
	Result result = clientData.videoEncodePipeline->encode(tagData, tagDataSize, clientData.videoKeyframeRequired);
	if (result)
	{
		clientData.videoKeyframeRequired = false;
	}
	else
	{
		TELEPORT_CERR << "Error occurred when trying to encode video" << std::endl;
		// repeat the attempt for debugging purposes.
		result = clientData.videoEncodePipeline->encode(tagData, tagDataSize, clientData.videoKeyframeRequired);
		if (result)
		{
			clientData.videoKeyframeRequired = false;
		}
	}
}

struct EncodeVideoParamsWrapper
{
	avs::uid clientID;
	SCServer::VideoEncodeParams videoEncodeParams;
};

static void UNITY_INTERFACE_API OnRenderEventWithData(int eventID, void* data)
{
	if (eventID == 0)
	{
		auto wrapper = (EncodeVideoParamsWrapper*)data;
		InitializeVideoEncoder(wrapper->clientID, wrapper->videoEncodeParams);
	}
	else if (eventID == 1)
	{
		auto wrapper = (EncodeVideoParamsWrapper*)data;
		ReconfigureVideoEncoder(wrapper->clientID, wrapper->videoEncodeParams);
	}
	else if (eventID == 2)
	{
		const auto buffer = (uint8_t*)data;

		avs::uid clientID;
		memcpy(&clientID, buffer, sizeof(avs::uid));

		uint32_t tagDataSize;
		memcpy(&tagDataSize, buffer + sizeof(avs::uid), sizeof(size_t));

		const uint8_t* tagData = buffer + sizeof(avs::uid) + sizeof(size_t);
		
		EncodeVideoFrame(clientID, tagData, tagDataSize);
	}
	else
	{
		TELEPORT_CERR << "Unknown event id" << std::endl;
	}
}

TELEPORT_EXPORT UnityRenderingEventAndData GetRenderEventWithDataCallback()
{
	return OnRenderEventWithData;
}
///VideoEncodePipeline END

///AudioEncodePipeline START
TELEPORT_EXPORT void InitializeAudioEncoder(avs::uid clientID, const SCServer::AudioParams& audioParams)
{
	std::lock_guard<std::mutex> lock(audioMutex);

	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
	{
		return;
	}

	auto& clientData = c->second;
	Result result = clientData.audioEncodePipeline->configure(audioParams, clientData.casterContext.AudioQueue.get());
	if (!result)
	{
		TELEPORT_CERR << "Error occurred when trying to configure the audio encode pipeline" << std::endl;
	}
}

TELEPORT_EXPORT void SendAudio(avs::uid clientID, const uint8_t* data, size_t dataSize)
{
	std::lock_guard<std::mutex> lock(audioMutex);

	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
	{
		return;
	}

	auto& clientData = c->second;
	if (!clientData.clientMessaging.hasPeer())
	{
		TELEPORT_CERR << "SendAudio called but peer is not connected." << std::endl;
		return;
	}

	Result result = clientData.audioEncodePipeline->sendAudio(data, dataSize);
	if (!result)
	{
		TELEPORT_CERR << "Error occurred when trying to send audio" << std::endl;
		// repeat the attempt for debugging purposes.
		result = clientData.audioEncodePipeline->sendAudio(data, dataSize);
	} 
}
///AudioEncodePipeline END

///ClientMessaging START
TELEPORT_EXPORT void Client_ActorEnteredBounds(avs::uid clientID, avs::uid actorID)
{
	auto client = clientServices.find(clientID);
	if(client == clientServices.end()) return;

	client->second.clientMessaging.actorEnteredBounds(actorID);
}

TELEPORT_EXPORT void Client_ActorLeftBounds(avs::uid clientID, avs::uid actorID)
{
	auto client = clientServices.find(clientID);
	if(client == clientServices.end()) return;

	client->second.clientMessaging.actorLeftBounds(actorID);
}

TELEPORT_EXPORT void Client_UpdateActorMovement(avs::uid clientID, avs::MovementUpdate* updates, int updateAmount)
{
	auto client = clientServices.find(clientID);
	if (client == clientServices.end()) return;
	std::vector<avs::MovementUpdate> updateList(updateAmount);
	for(int i = 0; i < updateAmount; i++)
	{
		updateList[i] = updates[i];

		avs::ConvertPosition(avs::AxesStandard::UnityStyle, client->second.casterContext.axesStandard, updateList[i].position);
		avs::ConvertRotation(avs::AxesStandard::UnityStyle, client->second.casterContext.axesStandard, updateList[i].rotation);
		avs::ConvertPosition(avs::AxesStandard::UnityStyle, client->second.casterContext.axesStandard, updateList[i].velocity);
		avs::ConvertPosition(avs::AxesStandard::UnityStyle, client->second.casterContext.axesStandard, updateList[i].angularVelocityAxis);
	}

	client->second.clientMessaging.updateActorMovement(updateList);
}

TELEPORT_EXPORT bool Client_HasHost(avs::uid clientID)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end()) return false;
	return c->second.clientMessaging.hasHost();
}

TELEPORT_EXPORT bool Client_HasPeer(avs::uid clientID)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end()) return false;
	return c->second.clientMessaging.hasPeer();
}

TELEPORT_EXPORT bool Client_SendCommand(avs::uid clientID, const avs::Command& avsCommand)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end()) return false;
	return c->second.clientMessaging.sendCommand(avsCommand);
}

TELEPORT_EXPORT bool Client_SendCommandWithList(avs::uid clientID, const avs::Command& avsCommand, std::vector<avs::uid>& appendedList)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end()) return false;
	return c->second.clientMessaging.sendCommand(avsCommand, appendedList);
}

TELEPORT_EXPORT const DWORD WINAPI Client_GetClientIP(avs::uid clientID,__in DWORD bufferLength,__out char* lpBuffer)
{
	auto c = clientServices.find(clientID);
	static std::string str;
	if (c != clientServices.end())
		str=c->second.clientMessaging.getClientIP();
	else
		str="";
	size_t final_len=std::min((size_t)bufferLength,str.length());
	if(final_len>0)
	{
		memcpy((void*)lpBuffer,str.c_str(),final_len);
		lpBuffer[final_len]=0;
	}
	return final_len;
}

TELEPORT_EXPORT uint16_t Client_GetClientPort(avs::uid clientID)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end()) return 0;
	return c->second.clientMessaging.getClientPort();
}

TELEPORT_EXPORT uint16_t Client_GetServerPort(avs::uid clientID)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end()) return 0;
	return c->second.clientMessaging.getServerPort();
}
///ClientMessaging END

///GeometryStore START
TELEPORT_EXPORT void SaveGeometryStore()
{
	geometryStore.saveToDisk();
}

TELEPORT_EXPORT void LoadGeometryStore(size_t* meshAmount, LoadedResource** meshes, size_t* textureAmount, LoadedResource** textures, size_t* materialAmount, LoadedResource** materials)
{
	geometryStore.loadFromDisk(*meshAmount, *meshes, *textureAmount, *textures, *materialAmount, *materials);
}

//Inform GeometryStore of resources that still exist, and of their new IDs.
TELEPORT_EXPORT void ReaffirmResources(int32_t meshAmount, ReaffirmedResource* reaffirmedMeshes, int32_t textureAmount, ReaffirmedResource* reaffirmedTextures, int32_t materialAmount, ReaffirmedResource* reaffirmedMaterials)
{
	geometryStore.reaffirmResources(meshAmount, reaffirmedMeshes, textureAmount, reaffirmedTextures, materialAmount, reaffirmedMaterials);
}

TELEPORT_EXPORT void ClearGeometryStore()
{
	geometryStore.clear(true);
}

TELEPORT_EXPORT void SetDelayTextureCompression(bool willDelay)
{
	geometryStore.willDelayTextureCompression = willDelay;
}

TELEPORT_EXPORT void SetCompressionLevels(uint8_t compressionStrength, uint8_t compressionQuality)
{
	geometryStore.setCompressionLevels(compressionStrength, compressionQuality);
}

TELEPORT_EXPORT void StoreNode(avs::uid id, InteropNode node)
{
	geometryStore.storeNode(id, avs::DataNode(node));
}

TELEPORT_EXPORT void StoreSkin(avs::uid id, InteropSkin skin)
{
	geometryStore.storeSkin(id, avs::Skin(skin), avs::AxesStandard::UnityStyle);
}

TELEPORT_EXPORT void StorePropertyAnimation(avs::uid animationID, InteropPropertyAnimation* animation)
{
	geometryStore.storeAnimation(animationID, avs::Animation(*animation), avs::AxesStandard::UnityStyle);
}

TELEPORT_EXPORT void StoreTransformAnimation(avs::uid animationID, InteropTransformAnimation* animation)
{
	geometryStore.storeAnimation(animationID, avs::Animation(*animation), avs::AxesStandard::UnityStyle);
}

TELEPORT_EXPORT void StoreMesh(avs::uid id, BSTR guid, std::time_t lastModified, InteropMesh* mesh, avs::AxesStandard extractToStandard)
{
	geometryStore.storeMesh(id, guid, lastModified, avs::Mesh(*mesh), extractToStandard);
}

TELEPORT_EXPORT void StoreMaterial(avs::uid id, BSTR guid, std::time_t lastModified, InteropMaterial material)
{
	geometryStore.storeMaterial(id, guid, lastModified, avs::Material(material));
}

TELEPORT_EXPORT bool IsMaterialStored(avs::uid id)
{
	const avs::Material *avsmat=geometryStore.getMaterial(id);
	if(avsmat)
		return true;
	return false;
}


TELEPORT_EXPORT void StoreTexture(avs::uid id, BSTR guid, std::time_t lastModified, InteropTexture texture, char* basisFileLocation)
{
	geometryStore.storeTexture(id, guid, lastModified, avs::Texture(texture), basisFileLocation);
}

TELEPORT_EXPORT void StoreShadowMap(avs::uid id, BSTR guid, std::time_t lastModified, InteropTexture shadowMap)
{
	geometryStore.storeShadowMap(id, guid, lastModified, avs::Texture(shadowMap));
}

TELEPORT_EXPORT void RemoveNode(avs::uid nodeID)
{
	geometryStore.removeNode(nodeID);
}

TELEPORT_EXPORT avs::DataNode* getNode(avs::uid nodeID)
{
	return geometryStore.getNode(nodeID);
}

TELEPORT_EXPORT uint64_t GetAmountOfTexturesWaitingForCompression()
{
	return static_cast<int64_t>(geometryStore.getAmountOfTexturesWaitingForCompression());
}

///TODO: Free memory of allocated string, or use passed in string to return message.
TELEPORT_EXPORT BSTR GetMessageForNextCompressedTexture(uint64_t textureIndex, uint64_t totalTextures)
{
	const avs::Texture* texture = geometryStore.getNextCompressedTexture();

	std::wstringstream messageStream;
	//Write compression message to wide string stream.
	messageStream << "Compressing texture " << textureIndex << "/" << totalTextures << " (" << texture->name.data() << " [" << texture->width << " x " << texture->height << "])";

	//Convert to binary string.
	return SysAllocString(messageStream.str().data());
}

TELEPORT_EXPORT void CompressNextTexture()
{
	geometryStore.compressNextTexture();
}
///GeometryStore END