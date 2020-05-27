#include <functional>
#include <iostream>
#include <queue>
#include <sstream>
#include <vector>

#include "enet/enet.h"
#include "libavstream/common.hpp"

#include "SimulCasterServer/CasterSettings.h"
#include "SimulCasterServer/CaptureDelegates.h"
#include "SimulCasterServer/ClientData.h"
#include "SimulCasterServer/DefaultDiscoveryService.h"
#include "SimulCasterServer/GeometryStore.h"
#include "SimulCasterServer/GeometryStreamingService.h"
#include "SimulCasterServer/VideoEncodePipeline.h"

#include "Export.h"
#include "InteropStructures.h"
#include "PluginGraphics.h"
#include "SimulCasterServer/ErrorHandling.h"

#ifdef _MSC_VER
#include "../VisualStudioDebugOutput.h"
VisualStudioDebugOutput debug_buffer(true, nullptr, 128);
#endif

using namespace SCServer;
TELEPORT_EXPORT void StartSession(avs::uid clientID, int32_t listenPort);
TELEPORT_EXPORT void StopStreaming(avs::uid clientID);
TELEPORT_EXPORT void StopSession(avs::uid clientID);

typedef void(__stdcall* SetHeadPoseFn) (avs::uid uid, const avs::HeadPose*);
typedef void(__stdcall* SetControllerPoseFn) (avs::uid uid, int index, const avs::HeadPose*);
typedef void(__stdcall* ProcessNewInputFn) (avs::uid uid, const avs::InputState*);
typedef void(__stdcall* DisconnectFn) (avs::uid uid);

static avs::Context avsContext;

static std::shared_ptr<DefaultDiscoveryService> discoveryService = std::make_unique<DefaultDiscoveryService>();
static GeometryStore geometryStore;

std::map<avs::uid, ClientData> clientServices;

SCServer::CasterSettings casterSettings; //Unity side settings are copied into this, so inner-classes can reference this rather than managed code instance.

static std::function<void(avs::uid clientID,void** actorPtr)> onShowActor;
static std::function<void(avs::uid clientID,void** actorPtr)> onHideActor;

static SetHeadPoseFn setHeadPose;
static SetControllerPoseFn setControllerPose;
static ProcessNewInputFn processNewInput;
static DisconnectFn onDisconnect;

static int32_t connectionTimeout = 5;
static avs::uid serverID = 0;

static std::set<avs::uid> unlinkedClientIDs; //Client IDs that haven't been linked to a session component.
static std::vector<avs::uid> lostClients; //Clients who have been lost, and are awaiting deletion.


class PluginGeometryStreamingService : public SCServer::GeometryStreamingService
{
public:
	PluginGeometryStreamingService()
		:SCServer::GeometryStreamingService(&casterSettings)
	{
		this->geometryStore = &::geometryStore;
	}

	virtual ~PluginGeometryStreamingService() = default;

	void addActor(void* newActor, avs::uid actorID)
	{
		SCServer::GeometryStreamingService::addActor(newActor, actorID);
	}

	avs::uid removeActor(void* oldActor)
	{
		return SCServer::GeometryStreamingService::removeActor(oldActor);
	}

	avs::uid getActorID(void* actor)
	{
		return SCServer::GeometryStreamingService::getActorID(actor);
	}

	bool isStreamingActor(void* actor)
	{
		return SCServer::GeometryStreamingService::isStreamingActor(actor);
	}

private:
	virtual void showActor_Internal(avs::uid clientID, void* actorPtr)
	{
		if(onShowActor)
			onShowActor(clientID,&actorPtr);
	}

	virtual void hideActor_Internal(avs::uid clientID, void* actorPtr)
	{
		if(onHideActor)
			onHideActor(clientID,&actorPtr);
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
		configured(false) {}

	~PluginVideoEncodePipeline()
	{
		//GraphicsManager::ReleaseResource(encoderSurfaceResource);
	}

	Result configure(VideoEncodeParams& videoEncodeParams, avs::Queue* colorQueue)
	{
		if (configured)
		{
			std::cout << "Video encode pipeline already configured." << std::endl;
			return Result::EncoderAlreadyConfigured;
		}

		if (!GraphicsManager::mGraphicsDevice)
		{
			std::cout << "Graphics device handle is null. Cannot attempt to initialize video encode pipeline." << std::endl;
			return Result::InvalidGraphicsDevice;
		}

		if (!videoEncodeParams.inputSurfaceResource)
		{
			std::cout << "Surface resource handle is null. Cannot attempt to initialize video encode pipeline." << std::endl;
			return Result::InvalidGraphicsResource;
		}

		inputSurfaceResource = videoEncodeParams.inputSurfaceResource;
		// Need to make a copy because Unity uses a typeless format which is not compatible with CUDA
		encoderSurfaceResource = GraphicsManager::CreateTextureCopy(inputSurfaceResource);

		videoEncodeParams.deviceHandle = GraphicsManager::mGraphicsDevice;
		videoEncodeParams.inputSurfaceResource = encoderSurfaceResource;

		Result result = SCServer::VideoEncodePipeline::initialize(casterSettings, videoEncodeParams, colorQueue);
		if (result)
		{
			configured = true;
		}
		return result;
	}

	Result reconfigure(VideoEncodeParams& videoEncodeParams)
	{
		if (!configured)
		{
			std::cout << "Video encoder cannot be reconfigured if pipeline has not been configured." << std::endl;
			return Result::EncoderNotConfigured;
		}

		if (!GraphicsManager::mGraphicsDevice)
		{
			std::cout << "Graphics device handle is null. Cannot attempt to initialize video encode pipeline." << std::endl;
			return Result::InvalidGraphicsDevice;
		}

		if (videoEncodeParams.inputSurfaceResource)
		{
			std::cout << "Surface resource handle is null. Cannot attempt to initialize video encode pipeline." << std::endl;
			return Result::InvalidGraphicsResource;
		}

		inputSurfaceResource = videoEncodeParams.inputSurfaceResource;
		// Need to make a copy because Unity uses a typeless format which is not compatible with CUDA
		encoderSurfaceResource = GraphicsManager::CreateTextureCopy(inputSurfaceResource);

		videoEncodeParams.deviceHandle = GraphicsManager::mGraphicsDevice;
		videoEncodeParams.inputSurfaceResource = encoderSurfaceResource;

		return SCServer::VideoEncodePipeline::reconfigure(casterSettings, videoEncodeParams);
	}

	Result encode(avs::Transform& cameraTransform, bool forceIDR = false)
	{
		if (!configured)
		{
			std::cout << "Video encoder can not encode because it has not been configured." << std::endl;
			return Result::EncoderNotConfigured;
		}

		// Copy data from Unity texture to its CUDA compatible copy
		GraphicsManager::CopyResource(encoderSurfaceResource, inputSurfaceResource);
		return SCServer::VideoEncodePipeline::process(cameraTransform, forceIDR);
	}

private:
	void* inputSurfaceResource;
	void* encoderSurfaceResource;
	bool configured;
};

///PLUGIN-INTERNAL START
void Disconnect(avs::uid clientID)
{
	onDisconnect(clientID);
	StopStreaming(clientID);
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

TELEPORT_EXPORT void SetShowActorDelegate(void(*showActor)(avs::uid,void*))
{
	onShowActor = showActor;
}

TELEPORT_EXPORT void SetHideActorDelegate(void(*hideActor)(avs::uid,void*))
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

TELEPORT_EXPORT void SetMessageHandlerDelegate(avs::MessageHandlerFunc messageHandler)
{
	avsContext.setMessageHandler(messageHandler, nullptr);
}

TELEPORT_EXPORT void SetConnectionTimeout(int32_t timeout)
{
	connectionTimeout = timeout;
}


struct InitializeState
{
	void(*showActor)(avs::uid clientID,void*);
	void(*hideActor)(avs::uid clientID,void*);
	void(*headPoseSetter)(avs::uid clientID, const avs::HeadPose*);
	void(*controllerPoseSetter)(avs::uid uid,int index,const avs::HeadPose*);
	void(*newInputProcessing)(avs::uid clientID, const avs::InputState*);
	DisconnectFn disconnect;
	avs::MessageHandlerFunc messageHandler;
	uint32_t DISCOVERY_PORT = 10607;
	uint32_t SERVICE_PORT = 10500;

};

TELEPORT_EXPORT bool Initialise(const InitializeState *initializeState)
{
	serverID = avs::GenerateUid();

	SetShowActorDelegate(initializeState->showActor);
	SetHideActorDelegate(initializeState->hideActor);
	SetHeadPoseSetterDelegate(initializeState->headPoseSetter);
	SetControllerPoseSetterDelegate(initializeState->controllerPoseSetter);
	SetNewInputProcessingDelegate(initializeState->newInputProcessing);
	SetDisconnectDelegate(initializeState->disconnect);
	SetMessageHandlerDelegate(initializeState->messageHandler);

	if(enet_initialize() != 0)
	{
		TELEPORT_CERR<<"An error occurred while attempting to initalise ENet!\n";
		return false;
	}
	atexit(enet_deinitialize);

	return discoveryService->initialise(initializeState->DISCOVERY_PORT,initializeState->SERVICE_PORT);
}

TELEPORT_EXPORT void Shutdown()
{
	discoveryService->shutdown();

	for(auto& clientService : clientServices)
	{
		clientService.second.clientMessaging.stopSession();
	}
	clientServices.clear();

	onShowActor = nullptr;
	onHideActor = nullptr;

	setHeadPose = nullptr;
	setControllerPose=nullptr;
	processNewInput = nullptr;
}

ClientData::ClientData(std::shared_ptr<PluginGeometryStreamingService> gs, std::shared_ptr<PluginVideoEncodePipeline> vep, std::function<void(void)> disconnect)
	: geometryStreamingService(gs)
	, videoEncodePipeline(vep)
	, clientMessaging(&casterSettings, discoveryService, geometryStreamingService, setHeadPose, setControllerPose, processNewInput, disconnect, connectionTimeout)
	
{
	originClientHas.x= originClientHas.y= originClientHas.z=0.f;
}

TELEPORT_EXPORT void StartSession(avs::uid clientID, int32_t listenPort)
{
	// Already started this session.
	if(clientServices.find(clientID) == clientServices.end())
	{
		ClientData newClientData(std::make_shared<PluginGeometryStreamingService>(), std::make_shared<PluginVideoEncodePipeline>(), std::bind(&Disconnect, clientID));
	
		if(newClientData.clientMessaging.startSession(clientID, listenPort))
		{
			clientServices.emplace(clientID, std::move(newClientData));
		}
		else
		{
			std::cerr << "Failed to start session for client: " << clientID << std::endl;
		}
	}
	else
		return;	// already got this client.
	auto &c= clientServices.find(clientID);
	if(c==clientServices.end())
		return;
	ClientData& newClient = c->second;
	newClient.casterContext.ColorQueue = std::make_unique<avs::Queue>();
	newClient.casterContext.GeometryQueue = std::make_unique<avs::Queue>();

	newClient.casterContext.ColorQueue->configure(16,"colorQueue");
	newClient.casterContext.GeometryQueue->configure(16, "GeometryQueue");

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

TELEPORT_EXPORT void StopSession(avs::uid clientID)
{
	//Early-out if a client with this ID doesn't exist.
	auto& clientIt = clientServices.find(clientID);
	if(clientIt == clientServices.end())
		return;
		
	ClientData& lostClient = clientIt->second;
	//Shut-down connections to the client.
	if(lostClient.isStreaming)
		StopStreaming(clientID);
	lostClient.clientMessaging.stopSession();

	//Remove references to lost client.
	clientServices.erase(clientID);
	// TODO: How does this work? 
	for(int i=0;i<lostClients.size();i++)
	{
		if(lostClients[i] ==clientID)
			lostClients.erase(lostClients.begin()+i);
	}
}

TELEPORT_EXPORT void StartStreaming(avs::uid clientID)
{
	auto c = clientServices.find(clientID);
	ClientData& client = c->second;
	client.geometryStreamingService->startStreaming(&client.casterContext);

	///TODO: Need to retrieve encoder settings from unity.
	SCServer::CasterEncoderSettings encoderSettings
	{
		1536,
		1536,
		1920,
		960,
		false,
		true,
		true,
		10000
	};

	avs::SetupCommand setupCommand;
	setupCommand.port = c->second.clientMessaging.getServerPort() + 1;
	setupCommand.video_width = encoderSettings.frameWidth;
	setupCommand.video_height = encoderSettings.frameHeight;
	setupCommand.depth_height = encoderSettings.depthHeight;
	setupCommand.depth_width = encoderSettings.depthWidth;
	setupCommand.use_10_bit_decoding = casterSettings.use10BitEncoding;
	setupCommand.use_yuv_444_decoding = casterSettings.useYUV444Decoding;
	setupCommand.colour_cubemap_size = encoderSettings.frameWidth / 3;
	setupCommand.compose_cube = encoderSettings.enableDecomposeCube;
	setupCommand.debug_stream = casterSettings.debugStream;
	setupCommand.do_checksums = casterSettings.enableChecksums ? 1 : 0;
	setupCommand.debug_network_packets = casterSettings.enableDebugNetworkPackets;
	setupCommand.requiredLatencyMs = casterSettings.requiredLatencyMs;
	setupCommand.server_id = serverID;
	setupCommand.videoCodec = casterSettings.videoCodec;

	///TODO: Initialise actors in range.

	client.clientMessaging.sendCommand(std::move(setupCommand));

	client.isStreaming = true;
}

TELEPORT_EXPORT void StopStreaming(avs::uid clientID)
{
	auto c = clientServices.find(clientID);
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
			StopSession(clientID);
		}
		lostClients.clear();
	}

	for(auto& idClientPair : clientServices)
	{
		ClientData &clientData=idClientPair.second;
		clientData.clientMessaging.handleEvents();

		if(clientData.clientMessaging.hasPeer())
		{
			if(idClientPair.second.casterContext.NetworkPipeline)
			{
				idClientPair.second.casterContext.NetworkPipeline->process();
			}

			clientData.clientMessaging.tick(deltaTime);

			if(clientData.isStreaming == false)
			{
				StartStreaming(idClientPair.first);
			}
		}
	}

	discoveryService->tick();
}

TELEPORT_EXPORT bool Client_SetOrigin(avs::uid clientID, const avs::vec3 *pos)
{
	auto &c=clientServices.find(clientID);
	if(c==clientServices.end())
		return false;
	ClientData &clientData=c->second;
	return clientData.setOrigin(*pos);
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
	return c->second.casterContext.NetworkPipeline->getBandWidthKPS();
}
///libavstream END

///GeometryStreamingService START
TELEPORT_EXPORT void AddActor(avs::uid clientID, void* newActor, avs::uid actorID)
{
	auto c= clientServices.find(clientID);
	if(c==clientServices.end())
		return;
	c->second.geometryStreamingService->addActor(newActor, actorID);
}

TELEPORT_EXPORT avs::uid RemoveActor(avs::uid clientID, void* oldActor)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
		return 0;
	return c->second.geometryStreamingService->removeActor(oldActor);
}

TELEPORT_EXPORT avs::uid GetActorID(avs::uid clientID, void* actor)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
		return 0;
	return c->second.geometryStreamingService->getActorID(actor);
}

TELEPORT_EXPORT bool IsStreamingActor(avs::uid clientID, void* actor)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
		return false;
	return c->second.geometryStreamingService->isStreamingActor(actor);
}

TELEPORT_EXPORT void ShowActor(avs::uid clientID, avs::uid actorID)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
		return;
	c->second.geometryStreamingService->showActor(clientID,actorID);
}

TELEPORT_EXPORT void HideActor(avs::uid clientID, avs::uid actorID)
{
	auto c = clientServices.find(clientID);
	if (c == clientServices.end())
		return;
	c->second.geometryStreamingService->hideActor(clientID,actorID);
}

TELEPORT_EXPORT void SetActorVisible(avs::uid clientID, avs::uid actorID, bool isVisible)
{
	if(isVisible)
		ShowActor(clientID, actorID);
	else
		HideActor(clientID, actorID);
}

bool HasResource(avs::uid clientID, avs::uid resourceID)
{
	auto c = clientServices.find(clientID);
	return c->second.geometryStreamingService->hasResource(resourceID);
}
///GeometryStreamingService END


///VideoEncodePipeline START
TELEPORT_EXPORT void InitializeVideoEncoder(avs::uid clientID, SCServer::VideoEncodeParams& videoEncodeParams)
{
	auto c = clientServices.find(clientID);
	auto& clientData = c->second;
	Result result = clientData.videoEncodePipeline->configure(videoEncodeParams, clientData.casterContext.ColorQueue.get());
	if(!result)
	{
		std::cout << "Error occurred when trying to configure the video encode pipeline" << std::endl;
	}
}

TELEPORT_EXPORT void ReconfigureVideoEncoder(avs::uid clientID, SCServer::VideoEncodeParams& videoEncodeParams)
{
	auto c = clientServices.find(clientID);
	auto& clientData = c->second;
	Result result = clientData.videoEncodePipeline->reconfigure(videoEncodeParams);
	if (!result)
	{
		std::cout << "Error occurred when trying to reconfigure the video encode pipeline" << std::endl;
	}
}

TELEPORT_EXPORT void EncodeVideoFrame(avs::uid clientID, avs::Transform& cameraTransform)
{
	auto c = clientServices.find(clientID);
	auto& clientData = c->second;
	if(!clientData.clientMessaging.hasPeer())
	{
		TELEPORT_CERR<< "EncodeVideoFrame called but peer is not connected." << std::endl;
		return;
	}
	avs::ConvertTransform(avs::AxesStandard::UnityStyle, clientData.casterContext.axesStandard, cameraTransform);
	Result result = clientData.videoEncodePipeline->encode(cameraTransform, clientData.videoKeyframeRequired);
	if (result)
	{
		clientData.videoKeyframeRequired = false;
	}
	else
	{
		std::cout << "Error occurred when trying to encode video" << std::endl;
	}
}

struct EncodeVideoParamsWrapper
{
	avs::uid clientID;
	SCServer::VideoEncodeParams videoEncodeParams;
};

struct TransformWrapper
{
	avs::uid clientID;
	avs::Transform transform;
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
		auto wrapper = (TransformWrapper*)data;
		EncodeVideoFrame(wrapper->clientID, wrapper->transform);
	}
	else
	{
		std::cout << "Unknown event id" << std::endl;
	}
}

TELEPORT_EXPORT UnityRenderingEventAndData GetRenderEventWithDataCallback()
{
	return OnRenderEventWithData;
}
///VideoEncodePipeline END

///ClientMessaging START
TELEPORT_EXPORT void ActorEnteredBounds(avs::uid clientID, avs::uid actorID)
{
	auto c = clientServices.find(clientID);
	c->second.clientMessaging.actorEnteredBounds(actorID);
}

TELEPORT_EXPORT void ActorLeftBounds(avs::uid clientID, avs::uid actorID)
{
	auto c = clientServices.find(clientID);
	c->second.clientMessaging.actorLeftBounds(actorID);
}

TELEPORT_EXPORT bool HasHost(avs::uid clientID)
{
	auto c = clientServices.find(clientID);
	return c->second.clientMessaging.hasHost();
}

TELEPORT_EXPORT bool HasPeer(avs::uid clientID)
{
	auto c = clientServices.find(clientID);
	return c->second.clientMessaging.hasPeer();
}

TELEPORT_EXPORT bool SendCommand(avs::uid clientID, const avs::Command& avsCommand)
{
	auto c = clientServices.find(clientID);
	return c->second.clientMessaging.sendCommand(avsCommand);
}

TELEPORT_EXPORT bool SendCommandWithList(avs::uid clientID, const avs::Command& avsCommand, std::vector<avs::uid>& appendedList)
{
	auto c = clientServices.find(clientID);
	return c->second.clientMessaging.sendCommand(avsCommand, appendedList);
}

TELEPORT_EXPORT char* GetClientIP(avs::uid clientID)
{
	auto c = clientServices.find(clientID);
	return c->second.clientMessaging.getClientIP().data();
}

TELEPORT_EXPORT uint16_t GetClientPort(avs::uid clientID)
{
	auto c = clientServices.find(clientID);
	return c->second.clientMessaging.getClientPort();
}

TELEPORT_EXPORT uint16_t GetServerPort(avs::uid clientID)
{
	auto c = clientServices.find(clientID);
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

TELEPORT_EXPORT const std::vector<std::pair<void*, avs::uid>>& GetHands()
{
	return geometryStore.getHands();
}

TELEPORT_EXPORT void SetHands(std::pair<void*, avs::uid> firstHand, std::pair<void*, avs::uid> secondHand)
{
	geometryStore.setHands(firstHand, secondHand);
}

TELEPORT_EXPORT void StoreNode(avs::uid id, InteropNode node)
{
	geometryStore.storeNode(id, avs::DataNode(node));
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