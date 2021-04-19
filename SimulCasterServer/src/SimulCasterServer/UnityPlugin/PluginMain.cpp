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

typedef bool(__stdcall* ShowNodeFn)(avs::uid clientID, avs::uid nodeID);
typedef bool(__stdcall* HideNodeFn)(avs::uid clientID, avs::uid nodeID);
typedef void(__stdcall* ProcessAudioInputFn) (avs::uid uid, const uint8_t* data, size_t dataSize);

static avs::Context avsContext;

static std::shared_ptr<DefaultDiscoveryService> discoveryService = std::make_unique<DefaultDiscoveryService>();
static GeometryStore geometryStore;

std::map<avs::uid, ClientData> clientServices;

SCServer::CasterSettings casterSettings; //Unity side settings are copied into this, so inner-classes can reference this rather than managed code instance.

static ShowNodeFn onShowNode;
static HideNodeFn onHideNode;

static SetHeadPoseFn setHeadPose;
static SetOriginFromClientFn setOriginFromClient;
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

static avs::vec3 bodyOffsetFromHead;

// Messages related stuff
avs::MessageHandlerFunc messageHandler = nullptr;
struct LogMessage
{
	avs::LogSeverity severity = avs::LogSeverity::Never;
	std::string msg;
	void* userData = nullptr;
};

static std::vector<LogMessage> messages(100);
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
	virtual void showNode_Internal(avs::uid clientID, avs::uid nodeID)
	{
		if(onShowNode)
		{
			if(!onShowNode(clientID, nodeID))
			{
				TELEPORT_CERR << "onShowNode failed for node " << nodeID << "(" << geometryStore->getNodeName(nodeID) << ")" << std::endl;
			}
		}
	}

	virtual void hideNode_Internal(avs::uid clientID, avs::uid nodeID)
	{
		if(onHideNode)
		{
			if(!onHideNode(clientID, nodeID))
			{
				TELEPORT_CERR << "onHideNode failed for node " << nodeID << "(" << geometryStore->getNodeName(nodeID) << ")" << std::endl;
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
			return Result::Code::EncoderAlreadyConfigured;
		}

		if (!GraphicsManager::mGraphicsDevice)
		{
			TELEPORT_CERR << "Graphics device handle is null. Cannot attempt to initialize video encode pipeline." << std::endl;
			return Result::Code::InvalidGraphicsDevice;
		}

		if (!videoEncodeParams.inputSurfaceResource)
		{
			TELEPORT_CERR << "Surface resource handle is null. Cannot attempt to initialize video encode pipeline." << std::endl;
			return Result::Code::InvalidGraphicsResource;
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
			return Result::Code::EncoderNotConfigured;
		}

		if (!GraphicsManager::mGraphicsDevice)
		{
			TELEPORT_CERR << "Graphics device handle is null. Cannot attempt to reconfigure video encode pipeline." << std::endl;
			return Result::Code::InvalidGraphicsDevice;
		}

		if (videoEncodeParams.inputSurfaceResource)
		{
			TELEPORT_CERR << "Surface resource handle is null. Cannot attempt to reconfigure video encode pipeline." << std::endl;
			return Result::Code::InvalidGraphicsResource;
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
			return Result::Code::EncoderNotConfigured;
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
			return Result::Code::EncoderNotConfigured;
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
			return Result::Code::EncoderAlreadyConfigured;
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
			return Result::Code::EncoderNotConfigured;
		}

		return SCServer::AudioEncodePipeline::process(data, dataSize);
	}

private:
	bool configured;
};

struct InitialiseState
{
	ShowNodeFn showNode;
	HideNodeFn hideNode;
	SetHeadPoseFn headPoseSetter;
	SetOriginFromClientFn setOriginFromClientFn;
	SetControllerPoseFn controllerPoseSetter;
	ProcessNewInputFn newInputProcessing;
	DisconnectFn disconnect;
	avs::MessageHandlerFunc messageHandler;
	uint32_t DISCOVERY_PORT = 10607;
	uint32_t SERVICE_PORT = 10500;
	ReportHandshakeFn reportHandshake;
	ProcessAudioInputFn processAudioInput;
	avs::vec3 bodyOffsetFromHead;
};

///PLUGIN-INTERNAL START
void RemoveClient(avs::uid clientID)
{
	std::lock_guard<std::mutex> videoLock(videoMutex);
	std::lock_guard<std::mutex> audioLock(audioMutex);

	// Early-out if a client with this ID doesn't exist.
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to remove client from server! No client exists with ID " << clientID << "!\n";
		return;
	}
	clientPair->second.clientMessaging.stopSession();

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

TELEPORT_EXPORT void SetShowNodeDelegate(ShowNodeFn showNode)
{
	onShowNode = showNode;
}

TELEPORT_EXPORT void SetHideNodeDelegate(HideNodeFn hideNode)
{
	onHideNode = hideNode;
}

TELEPORT_EXPORT void SetHeadPoseSetterDelegate(SetHeadPoseFn headPoseSetter)
{
	setHeadPose = headPoseSetter;
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
		for(LogMessage& message : messages)
		{
			messageHandler(message.severity, message.msg.c_str(), message.userData);
		}
		messages.clear();
	}
}

TELEPORT_EXPORT void SetMessageHandlerDelegate(avs::MessageHandlerFunc msgh)
{
	if(msgh)
	{
		debug_buffer.setToOutputWindow(false);
		messageHandler=msgh;
		avsContext.setMessageHandler(AccumulateMessagesFromThreads, nullptr); 
		debug_buffer.setOutputCallback(&passOnOutput);
		debug_buffer.setErrorCallback(&passOnError);
	}
	else
	{
		debug_buffer.setToOutputWindow(true);
		messageHandler=nullptr;
		avsContext.setMessageHandler(nullptr, nullptr); 
		debug_buffer.setOutputCallback(nullptr);
		debug_buffer.setErrorCallback(nullptr);
	}
}

TELEPORT_EXPORT void SetConnectionTimeout(int32_t timeout)
{
	connectionTimeout = timeout;
}

TELEPORT_EXPORT bool Initialise(const InitialiseState *initialiseState)
{
	unlinkedClientIDs.clear();

	serverID = avs::GenerateUid();

	SetShowNodeDelegate(initialiseState->showNode);
	SetHideNodeDelegate(initialiseState->hideNode);
	SetHeadPoseSetterDelegate(initialiseState->headPoseSetter);

	setOriginFromClient = initialiseState->setOriginFromClientFn;
	setControllerPose = initialiseState->controllerPoseSetter;
	SetNewInputProcessingDelegate(initialiseState->newInputProcessing);
	SetDisconnectDelegate(initialiseState->disconnect);
	SetMessageHandlerDelegate(initialiseState->messageHandler);
	SetProcessAudioInputDelegate(initialiseState->processAudioInput);

	reportHandshake=initialiseState->reportHandshake;

	bodyOffsetFromHead = initialiseState->bodyOffsetFromHead;

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

	for(auto& clientPair : clientServices)
	{
		ClientData& clientData = clientPair.second;
		if(clientData.isStreaming)
		{
			// This will add to lost clients but will be cleared below
			Client_StopStreaming(clientPair.first);
		}
		clientData.clientMessaging.stopSession();
	}

	lostClients.clear();
	unlinkedClientIDs.clear();
	clientServices.clear();

	onShowNode = nullptr;
	onHideNode = nullptr;

	setHeadPose = nullptr;
	setOriginFromClient=nullptr;
	setControllerPose = nullptr;
	processNewInput = nullptr;
}

TELEPORT_EXPORT void Client_StartSession(avs::uid clientID, int32_t listenPort)
{
	//Check if we already have a session for a client with the passed ID.
	auto c=clientServices.find(clientID);
	if(c == clientServices.end())
	{
		std::shared_ptr<PluginGeometryStreamingService> geometryStreamingService = std::make_shared<PluginGeometryStreamingService>();
		std::shared_ptr<PluginVideoEncodePipeline> videoEncodePipeline = std::make_shared<PluginVideoEncodePipeline>();
		std::shared_ptr<PluginAudioEncodePipeline> audioEncodePipeline = std::make_shared<PluginAudioEncodePipeline>();
		SCServer::ClientMessaging clientMessaging(&casterSettings, discoveryService, geometryStreamingService, setHeadPose, setOriginFromClient, setControllerPose, processNewInput, onDisconnect, connectionTimeout, reportHandshake);
		ClientData newClientData(geometryStreamingService, videoEncodePipeline, audioEncodePipeline, clientMessaging);

		if(newClientData.clientMessaging.startSession(clientID, listenPort))
		{
			clientServices.emplace(clientID, std::move(newClientData));
		}
		else
		{
			TELEPORT_CERR << "Failed to start session for Client_" << clientID << "!\n";
			return;
		}
	}
	else
	{
	/*	TELEPORT_CERR << "Called StartSession on Client_" << clientID << ", but we have already started the session. Restarting!\n";
		if(!c->second.clientMessaging.restartSession(clientID, listenPort))
		{
			TELEPORT_CERR << "Failed to restartSession for Client_" << clientID << "!\n";
			return;
		}*/
		//unlinkedClientIDs.insert(clientID);
		return;
	}

	auto clientPair = clientServices.find(clientID);
	if(clientServices.find(clientID) == clientServices.end())
	{
		TELEPORT_CERR << "Failed to start session for Client_" << clientID << "! Failed to find client in client list despite being added/being found earlier!\n";
		return;
	}

	ClientData& newClient = clientPair->second;
	if(newClient.clientMessaging.isInitialised())
	{
		return;
	}

	// Sending
	newClient.casterContext.ColorQueue = std::make_unique<avs::Queue>();
	newClient.casterContext.GeometryQueue = std::make_unique<avs::Queue>();
	newClient.casterContext.AudioQueue = std::make_unique<avs::Queue>();

	newClient.casterContext.ColorQueue->configure(200000, 16,"ColorQueue");
	newClient.casterContext.GeometryQueue->configure(200000, 16, "GeometryQueue");
	newClient.casterContext.AudioQueue->configure(8192, 120, "AudioQueue");

	// Receiving
	if (casterSettings.isReceivingAudio)
	{
		newClient.casterContext.sourceAudioQueue = std::make_unique<avs::Queue>();
		newClient.casterContext.audioDecoder = std::make_unique<avs::AudioDecoder>();
		newClient.casterContext.audioTarget = std::make_unique<avs::AudioTarget>();
		newClient.casterContext.audioStreamTarget = std::make_unique<sca::CustomAudioStreamTarget>(std::bind(&ProcessAudioInput, clientID, std::placeholders::_1, std::placeholders::_2));

		newClient.casterContext.sourceAudioQueue->configure( 8192, 120, "SourceAudioQueue");
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
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to stop session to Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	// Shut-down connections to the client.
	if(clientPair->second.isStreaming)
	{
		// Will add to lost clients and call shutdown command
		Client_StopStreaming(clientID);
	}

	RemoveClient(clientID);

	auto iter = lostClients.begin();
	while(iter != lostClients.end())
	{
		if(*iter == clientID)
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
	auto clientPair = clientServices.find(clientID);
	if (clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to start streaming to Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	ClientData& clientData = clientPair->second;
	clientData.geometryStreamingService->startStreaming(&clientData.casterContext);

	SCServer::CasterEncoderSettings encoderSettings{};
	if (casterSettings.usePerspectiveRendering)
	{
		encoderSettings.frameWidth = casterSettings.perspectiveWidth;
		encoderSettings.frameHeight = casterSettings.perspectiveHeight * 1.5f;
		encoderSettings.depthWidth = casterSettings.perspectiveWidth * 0.5f;
		encoderSettings.depthHeight = casterSettings.perspectiveHeight * 0.5f;
	}
	else
	{
		encoderSettings.frameWidth = static_cast<int32_t>(casterSettings.captureCubeSize * 3);
		encoderSettings.frameHeight = static_cast<int32_t>(casterSettings.captureCubeSize * 3);
		encoderSettings.depthWidth = casterSettings.captureCubeSize * 1.5f;
		encoderSettings.depthHeight = casterSettings.captureCubeSize;
	}
	encoderSettings.wllWriteDepthTexture = false;
	encoderSettings.enableStackDepth = true;
	encoderSettings.enableDecomposeCube = true;
	encoderSettings.maxDepth = 10000;

	avs::SetupCommand setupCommand;
	setupCommand.server_streaming_port = clientData.clientMessaging.getServerPort() + 1;
	setupCommand.debug_stream = casterSettings.debugStream;
	setupCommand.do_checksums = casterSettings.enableChecksums ? 1 : 0;
	setupCommand.debug_network_packets = casterSettings.enableDebugNetworkPackets;
	setupCommand.requiredLatencyMs = casterSettings.requiredLatencyMs;
	setupCommand.idle_connection_timeout = connectionTimeout;
	setupCommand.server_id = serverID;
	setupCommand.axesStandard = avs::AxesStandard::UnityStyle;
	setupCommand.audio_input_enabled = casterSettings.isReceivingAudio;
	setupCommand.lock_player_height = casterSettings.lockPlayerHeight;
	setupCommand.control_model=casterSettings.controlModel;
	setupCommand.bodyOffsetFromHead = bodyOffsetFromHead;

	avs::VideoConfig& videoConfig		= setupCommand.video_config;
	videoConfig.video_width				= encoderSettings.frameWidth;
	videoConfig.video_height			= encoderSettings.frameHeight;
	videoConfig.depth_height			= encoderSettings.depthHeight;
	videoConfig.depth_width				= encoderSettings.depthWidth;
	videoConfig.perspective_width       = casterSettings.perspectiveWidth;
	videoConfig.perspective_height      = casterSettings.perspectiveHeight;
	videoConfig.perspective_fov			= casterSettings.perspectiveFOV;
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
	
	videoConfig.specular_mips		=casterSettings.specularMips;
	// To the right of the specular cube, after 3 mips = 1 + 1/2 + 1/4
	
	videoConfig.diffuse_cubemap_size=casterSettings.diffuseCubemapSize;
	// To the right of the depth map, under the specular map.
	videoConfig.diffuse_x			=depth_cubemap_size*3;
	videoConfig.diffuse_y			=videoConfig.specular_y+casterSettings.specularCubemapSize*2;
	
	videoConfig.light_cubemap_size	=casterSettings.lightCubemapSize;
	// To the right of the diffuse map.
	videoConfig.light_x				=videoConfig.diffuse_x+(casterSettings.diffuseCubemapSize*3*7)/4;
	videoConfig.light_y				=videoConfig.diffuse_y;
	
	videoConfig.shadowmap_x		=videoConfig.diffuse_x;
	videoConfig.shadowmap_y		=videoConfig.diffuse_y+2*videoConfig.diffuse_cubemap_size;
	videoConfig.shadowmap_size	=64;
	clientData.clientMessaging.sendCommand(std::move(setupCommand));

	clientData.isStreaming = true;
}

TELEPORT_EXPORT void Client_StopStreaming(avs::uid clientID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to stop streaming to Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	ClientData& lostClient = clientPair->second;
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

	for(auto& clientPair : clientServices)
	{
		ClientData& clientData = clientPair.second;
		clientData.clientMessaging.handleEvents(deltaTime);

		if(clientData.clientMessaging.hasPeer())
		{
			clientData.clientMessaging.tick(deltaTime);

			if(clientData.isStreaming == false)
			{
				Client_StartStreaming(clientPair.first);
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

TELEPORT_EXPORT bool Client_SetOrigin(avs::uid clientID,uint64_t validCounter, const avs::vec3* pos, bool set_rel, const avs::vec3* orig_to_head, const avs::vec4* orient)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to set client origin of Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}
	ClientData& clientData = clientPair->second;
	return clientData.setOrigin(validCounter,*pos, set_rel, *orig_to_head,*orient);
}

TELEPORT_EXPORT bool Client_IsConnected(avs::uid clientID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		//TELEPORT_CERR << "Failed to check Client_" << clientID << " is connected! No client exists with ID " << clientID << "!\n";
		return false;
	}

	ClientData& clientData = clientPair->second;
	return clientData.isConnected();
}

TELEPORT_EXPORT bool Client_HasOrigin(avs::uid clientID, avs::vec3* pos)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to check Client_" << clientID << " has origin! No client exists with ID " << clientID << "!\n";
		return false;
	}

	ClientData& clientData = clientPair->second;

	bool result = (clientData.hasOrigin());
	if(result && pos)
	{
		*pos = clientData.getOrigin();
	}

	return result;
}

TELEPORT_EXPORT void Reset()
{
	for(auto& clientPair : clientServices)
	{
		clientPair.second.geometryStreamingService->reset();
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
///libavstream END

///GeometryStreamingService START
TELEPORT_EXPORT void Client_AddNode(avs::uid clientID, avs::uid nodeID, avs::Transform currentTransform)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to start streaming Node_" << nodeID << " to Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	clientPair->second.geometryStreamingService->addNode(nodeID);
	//Update node transform, as it may have changed since the node was last streamed.
	geometryStore.updateNode(nodeID, currentTransform);
}

TELEPORT_EXPORT void Client_RemoveNodeByID(avs::uid clientID, avs::uid nodeID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to stop streaming Node_" << nodeID << " to Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	clientPair->second.geometryStreamingService->removeNode(nodeID);
}

TELEPORT_EXPORT bool Client_IsStreamingNodeID(avs::uid clientID, avs::uid nodeID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to check if Node_" << nodeID << "exists! No client exists with ID " << clientID << "!\n";
		return false;
	}

	return clientPair->second.geometryStreamingService->isStreamingNode(nodeID);
}

TELEPORT_EXPORT void Client_ShowNode(avs::uid clientID, avs::uid nodeID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to show Node_" << nodeID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	clientPair->second.geometryStreamingService->showNode(clientID, nodeID);
}

TELEPORT_EXPORT void Client_HideNode(avs::uid clientID, avs::uid nodeID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to hide Node_" << nodeID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	clientPair->second.geometryStreamingService->hideNode(clientID, nodeID);
}

TELEPORT_EXPORT void Client_SetNodeVisible(avs::uid clientID, avs::uid nodeID, bool isVisible)
{
	if(isVisible)
	{
		Client_ShowNode(clientID, nodeID);
	}
	else
	{
		Client_HideNode(clientID, nodeID);
	}
}

TELEPORT_EXPORT bool Client_IsClientRenderingNodeID(avs::uid clientID, avs::uid nodeID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to check if Client_" << clientID << " is rendering Node_" << nodeID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}

	return clientPair->second.geometryStreamingService->isClientRenderingNode(nodeID);
}

bool Client_HasResource(avs::uid clientID, avs::uid resourceID)
{
	auto clientPair = clientServices.find(clientID);
	if (clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to check if Client_" << clientID << " has Resource_" << resourceID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}
	return clientPair->second.geometryStreamingService->hasResource(resourceID);
}
///GeometryStreamingService END


///VideoEncodePipeline START
TELEPORT_EXPORT void InitializeVideoEncoder(avs::uid clientID, SCServer::VideoEncodeParams& videoEncodeParams)
{
	std::lock_guard<std::mutex> lock(videoMutex);

	auto clientPair = clientServices.find(clientID);
	if (clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to initialise video encoder for Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	ClientData& clientData = clientPair->second;
	avs::Queue *q	=clientData.casterContext.ColorQueue.get();
	Result result	=clientData.videoEncodePipeline->configure(videoEncodeParams, q);
	if(!result)
	{
		TELEPORT_CERR << "Failed to initialise video encoder for Client_" << clientID << "! Error occurred when trying to configure the video encoder pipeline!\n";
	}
}

TELEPORT_EXPORT void ReconfigureVideoEncoder(avs::uid clientID, SCServer::VideoEncodeParams& videoEncodeParams)
{
	std::lock_guard<std::mutex> lock(videoMutex);

	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to reconfigure video encoder for Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	ClientData& clientData = clientPair->second;
	Result result = clientData.videoEncodePipeline->reconfigure(videoEncodeParams);
	if (!result)
	{
		TELEPORT_CERR << "Failed to reconfigure video encoder for Client_" << clientID << "! Error occurred when trying to reconfigure the video encoder pipeline!\n";
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
	videoConfig.perspective_width = casterSettings.perspectiveWidth;
	videoConfig.perspective_height = casterSettings.perspectiveHeight;
	videoConfig.perspective_fov = casterSettings.perspectiveFOV;
	videoConfig.use_10_bit_decoding = casterSettings.use10BitEncoding;
	videoConfig.use_yuv_444_decoding = casterSettings.useYUV444Decoding;
	videoConfig.colour_cubemap_size = encoderSettings.frameWidth / 3;
	videoConfig.compose_cube = encoderSettings.enableDecomposeCube;
	videoConfig.videoCodec = casterSettings.videoCodec;
	videoConfig.use_cubemap = !casterSettings.usePerspectiveRendering;

	clientData.clientMessaging.sendCommand(cmd);
}

TELEPORT_EXPORT void EncodeVideoFrame(avs::uid clientID, const uint8_t* tagData, size_t tagDataSize)
{
	std::lock_guard<std::mutex> lock(videoMutex);

	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to encode video frame for Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	ClientData& clientData = clientPair->second;
	if(!clientData.clientMessaging.hasPeer())
	{
		TELEPORT_COUT << "Failed to encode video frame for Client_" << clientID << "! Client has no peer!\n";
		return;
	}

	Result result = clientData.videoEncodePipeline->encode(tagData, tagDataSize, clientData.videoKeyframeRequired);
	if(result)
	{
		clientData.videoKeyframeRequired = false;
	}
	else
	{
		TELEPORT_CERR << "Failed to encode video frame for Client_" << clientID << "! Error occurred when trying to encode video!\n";

		// repeat the attempt for debugging purposes.
		result = clientData.videoEncodePipeline->encode(tagData, tagDataSize, clientData.videoKeyframeRequired);
		if(result)
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

	auto clientPair = clientServices.find(clientID);
	if (clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to initialise audio encoder for Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	ClientData& clientData = clientPair->second;
	Result result = clientData.audioEncodePipeline->configure(audioParams, clientData.casterContext.AudioQueue.get());
	if (!result)
	{
		TELEPORT_CERR << "Failed to initialise audio encoder for Client_" << clientID << "! Error occurred when trying to configure the audio encoder pipeline!\n";
	}
}

TELEPORT_EXPORT void SendAudio(avs::uid clientID, const uint8_t* data, size_t dataSize)
{
	std::lock_guard<std::mutex> lock(audioMutex);

	auto clientPair = clientServices.find(clientID);
	if (clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to send audio to Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	ClientData& clientData = clientPair->second;
	if (!clientData.clientMessaging.hasPeer())
	{
		return;
	}

	// Only continue processing if the main thread hasn't hung.
	double elapsedTime = avs::PlatformWindows::getTimeElapsedInSeconds(ClientMessaging::getLastTickTimestamp(), avs::PlatformWindows::getTimestamp());
	if (elapsedTime > 0.15f)
	{
		return;
	}

	Result result = clientData.audioEncodePipeline->sendAudio(data, dataSize);
	if (!result)
	{
		TELEPORT_CERR << "Failed to send audio to Client_" << clientID << "! Error occurred when trying to send audio" << std::endl;
	} 
}
///AudioEncodePipeline END

///ClientMessaging START
TELEPORT_EXPORT void Client_NodeEnteredBounds(avs::uid clientID, avs::uid nodeID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to mark node as entering bounds for Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	clientPair->second.clientMessaging.nodeEnteredBounds(nodeID);
}

TELEPORT_EXPORT void Client_NodeLeftBounds(avs::uid clientID, avs::uid nodeID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to mark node as leaving bounds for Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	clientPair->second.clientMessaging.nodeLeftBounds(nodeID);
}

TELEPORT_EXPORT void Client_UpdateNodeMovement(avs::uid clientID, avs::MovementUpdate* updates, int updateAmount)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to update node movement for Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	std::vector<avs::MovementUpdate> updateList(updateAmount);
	for(int i = 0; i < updateAmount; i++)
	{
		updateList[i] = updates[i];

		avs::ConvertPosition(avs::AxesStandard::UnityStyle, clientPair->second.casterContext.axesStandard, updateList[i].position);
		avs::ConvertRotation(avs::AxesStandard::UnityStyle, clientPair->second.casterContext.axesStandard, updateList[i].rotation);
		avs::ConvertPosition(avs::AxesStandard::UnityStyle, clientPair->second.casterContext.axesStandard, updateList[i].velocity);
		avs::ConvertPosition(avs::AxesStandard::UnityStyle, clientPair->second.casterContext.axesStandard, updateList[i].angularVelocityAxis);
	}

	clientPair->second.clientMessaging.updateNodeMovement(updateList);
}

TELEPORT_EXPORT bool Client_HasHost(avs::uid clientID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to check if Client_" << clientID << " has host! No client exists with ID " << clientID << "!\n";
		return false;
	}
	return clientPair->second.clientMessaging.hasHost();
}

TELEPORT_EXPORT bool Client_HasPeer(avs::uid clientID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to check if Client_" << clientID << " has peer! No client exists with ID " << clientID << "!\n";
		return false;
	}
	return clientPair->second.clientMessaging.hasPeer();
}

TELEPORT_EXPORT bool Client_SendCommand(avs::uid clientID, const avs::Command& avsCommand)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to send command to Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}
	return clientPair->second.clientMessaging.sendCommand(avsCommand);
}

TELEPORT_EXPORT bool Client_SendCommandWithList(avs::uid clientID, const avs::Command& avsCommand, std::vector<avs::uid>& appendedList)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to send command to Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}
	return clientPair->second.clientMessaging.sendCommand(avsCommand, appendedList);
}

TELEPORT_EXPORT const DWORD WINAPI Client_GetClientIP(avs::uid clientID, __in DWORD bufferLength, __out char* lpBuffer)
{
	static std::string str;

	auto clientPair = clientServices.find(clientID);
	if(clientPair != clientServices.end())
	{
		str = clientPair->second.clientMessaging.getClientIP();
	}
	else
	{
		TELEPORT_CERR << "Failed to retrieve IP of Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		str = "";
	}

	size_t final_len = std::min(static_cast<size_t>(bufferLength), str.length());
	if(final_len > 0)
	{
		memcpy(reinterpret_cast<void*>(lpBuffer), str.c_str(), final_len);
		lpBuffer[final_len] = 0;
	}
	return final_len;
}

TELEPORT_EXPORT uint16_t Client_GetClientPort(avs::uid clientID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to retrieve client port of Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return 0;
	}
	return clientPair->second.clientMessaging.getClientPort();
}

TELEPORT_EXPORT uint16_t Client_GetServerPort(avs::uid clientID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to retrieve server port of Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return 0;
	}
	return clientPair->second.clientMessaging.getServerPort();
}

TELEPORT_EXPORT bool Client_GetClientNetworkStats(avs::uid clientID, avs::NetworkSinkCounters& counters)
{
	auto clientPair = clientServices.find(clientID);
	if (clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to retrieve network stats of Client_" << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}
	
	ClientData& clientData = clientPair->second;
	if (!clientData.clientMessaging.hasPeer())
	{
		TELEPORT_COUT << "Failed to retrieve network stats of Client_" << clientID << "! Client has no peer!\n";
		return false;
	}

	if (!clientData.casterContext.NetworkPipeline)
	{
		TELEPORT_COUT << "Failed to retrieve network stats of Client_" << clientID << "! NetworkPipeline is null!\n";
		return false;
	}
	
	// Thread safe
	clientData.casterContext.NetworkPipeline->getCounters(counters);

	return true;
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

TELEPORT_EXPORT void StoreTexture(avs::uid id, BSTR guid, std::time_t lastModified, InteropTexture texture, char* basisFileLocation)
{
	geometryStore.storeTexture(id, guid, lastModified, avs::Texture(texture), basisFileLocation);
}

TELEPORT_EXPORT void StoreShadowMap(avs::uid id, BSTR guid, std::time_t lastModified, InteropTexture shadowMap)
{
	geometryStore.storeShadowMap(id, guid, lastModified, avs::Texture(shadowMap));
}

TELEPORT_EXPORT bool IsNodeStored(avs::uid id)
{
	const avs::DataNode* node = geometryStore.getNode(id);
	return node != nullptr;
}

TELEPORT_EXPORT bool IsSkinStored(avs::uid id)
{
	//NOTE: Assumes we always are storing animations in the engineering axes standard.
	const avs::Skin* skin = geometryStore.getSkin(id, avs::AxesStandard::EngineeringStyle);
	return skin != nullptr;
}

TELEPORT_EXPORT bool IsMeshStored(avs::uid id)
{
	//NOTE: Assumes we always are storing meshes in the engineering axes standard.
	const avs::Mesh* mesh = geometryStore.getMesh(id, avs::AxesStandard::EngineeringStyle);
	return mesh != nullptr;
}

TELEPORT_EXPORT bool IsMaterialStored(avs::uid id)
{
	const avs::Material* material = geometryStore.getMaterial(id);
	return material != nullptr;
}

TELEPORT_EXPORT bool IsTextureStored(avs::uid id)
{
	const avs::Texture* texture = geometryStore.getTexture(id);
	return texture != nullptr;
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

TELEPORT_EXPORT size_t SizeOf(const char *str)
{
	if(strcmp(str,"CasterSettings")==0)
	{
		return sizeof(CasterSettings);
	}
	TELEPORT_CERR<<"Unknown type for SizeOf: "<<str<<std::endl;
	return 0;
}