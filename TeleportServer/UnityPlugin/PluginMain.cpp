#include <functional>
#include <iostream>
#include <queue>
#include <sstream>
#include <vector>
#include <unordered_map>

#include "enet/enet.h"
#include "libavstream/common.hpp"

#include "TeleportServer/ServerSettings.h"
#include "TeleportServer/CaptureDelegates.h"
#include "TeleportServer/ClientData.h"
#include "TeleportServer/DefaultHTTPService.h"
#include "TeleportServer/GeometryStore.h"
#include "TeleportServer/GeometryStreamingService.h"
#include "TeleportServer/AudioEncodePipeline.h"
#include "TeleportServer/VideoEncodePipeline.h"
#include "TeleportServer/ClientManager.h"

#include "Export.h"
#include "InteropStructures.h"
#include "PluginGraphics.h"
#include "TeleportCore/ErrorHandling.h"
#include "CustomAudioStreamTarget.h"
#include "PluginClient.h"
#include "PluginMain.h"

//#include <OAIdl.h>	// for SAFE_ARRAY

#ifdef _MSC_VER
#include "../VisualStudioDebugOutput.h"
VisualStudioDebugOutput debug_buffer(true, "teleport_server.log", 128);
#else
#include "../UnixDebugOutput.h"
DebugOutput debug_buffer(true, "teleport_server.log", 128);
#endif

using namespace teleport;
using namespace server;

TELEPORT_EXPORT void AddUnlinkedClientID(avs::uid clientID);

TELEPORT_EXPORT void ConvertTransform(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, avs::Transform &transform)
{
	avs::ConvertTransform(fromStandard,toStandard,transform);
}
TELEPORT_EXPORT void ConvertRotation(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, vec4 &rotation)
{
	avs::ConvertRotation(fromStandard,toStandard,rotation);
}
TELEPORT_EXPORT void ConvertPosition(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, vec3 &position)
{
	avs::ConvertPosition(fromStandard,toStandard,position);
}
TELEPORT_EXPORT void ConvertScale(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, vec3 &scale)
{
	avs::ConvertScale(fromStandard,toStandard,scale);
}
TELEPORT_EXPORT int8_t ConvertAxis(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, int8_t axis)
{
	return avs::ConvertAxis(fromStandard,toStandard,axis);
}

static avs::Context avsContext;

AudioSettings audioSettings;

static std::set<avs::uid> unlinkedClientIDs; //Client IDs that haven't been linked to a session component.

namespace teleport
{
	namespace server
	{
		std::vector<avs::uid> lostClients; //Clients who have been lost, and are awaiting deletion.
	}
}


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


struct InitialiseState
{
	char* clientIP;
	char* httpMountDirectory;
	char* certDirectory;
	char* privateKeyDirectory;
	uint32_t DISCOVERY_PORT = 10607;
	uint32_t SERVICE_PORT = 10500;

	ClientStoppedRenderingNodeFn clientStoppedRenderingNode;
	ClientStartedRenderingNodeFn clientStartedRenderingNode;
	SetHeadPoseFn headPoseSetter;
	SetControllerPoseFn controllerPoseSetter;
	ProcessNewInputStateFn newInputStateProcessing;
	ProcessNewInputEventsFn newInputEventsProcessing;
	DisconnectFn disconnect;
	avs::MessageHandlerFunc messageHandler;
	ReportHandshakeFn reportHandshake;
	ProcessAudioInputFn processAudioInput;
	GetUnixTimestampFn getUnixTimestamp;
};


///MEMORY-MANAGEMENT START
TELEPORT_EXPORT void DeleteUnmanagedArray(void** unmanagedArray)
{
	delete[] (uint8_t*)*unmanagedArray;
}
///MEMORY-MANAGEMENT END

///PLUGIN-SPECIFIC START
TELEPORT_EXPORT void UpdateServerSettings(const ServerSettings newSettings)
{
	serverSettings = newSettings;
}

TELEPORT_EXPORT bool SetCachePath(const char* path)
{
	return GeometryStore::GetInstance().SetCachePath(path);
}

TELEPORT_EXPORT void SetClientStoppedRenderingNodeDelegate(ClientStoppedRenderingNodeFn clientStoppedRenderingNode)
{
	PluginGeometryStreamingService::callback_clientStoppedRenderingNode = clientStoppedRenderingNode;
}

TELEPORT_EXPORT void SetClientStartedRenderingNodeDelegate(ClientStartedRenderingNodeFn clientStartedRenderingNode)
{
	PluginGeometryStreamingService::callback_clientStartedRenderingNode = clientStartedRenderingNode;
}

TELEPORT_EXPORT void SetHeadPoseSetterDelegate(SetHeadPoseFn headPoseSetter)
{
	setHeadPose = headPoseSetter;
}

TELEPORT_EXPORT void SetNewInputStateProcessingDelegate(ProcessNewInputStateFn newInputProcessing)
{
	processNewInputState = newInputProcessing;
}

TELEPORT_EXPORT void SetNewInputEventsProcessingDelegate(ProcessNewInputEventsFn newInputProcessing)
{
	processNewInputEvents = newInputProcessing;
}

TELEPORT_EXPORT void SetDisconnectDelegate(DisconnectFn disconnect)
{
	onDisconnect = disconnect;
}

TELEPORT_EXPORT void SetProcessAudioInputDelegate(ProcessAudioInputFn f)
{
	processAudioInput = f;
}

TELEPORT_EXPORT void SetGetUnixTimestampDelegate(GetUnixTimestampFn function)
{
	getUnixTimestamp = function;
}

static void passOnOutput(const char *msg)
{
	if(msg)
		avsContext.log(avs::LogSeverity::Info,msg);
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
	std::lock_guard<std::mutex> lock(messagesMutex);
	if(msgh)
	{
		debug_buffer.setToOutputWindow(true);
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

TELEPORT_EXPORT bool Teleport_Initialize(const InitialiseState *initialiseState)
{
	unlinkedClientIDs.clear();

	SetClientStoppedRenderingNodeDelegate(initialiseState->clientStoppedRenderingNode);
	SetClientStartedRenderingNodeDelegate(initialiseState->clientStartedRenderingNode);
	SetHeadPoseSetterDelegate(initialiseState->headPoseSetter);

	setControllerPose = initialiseState->controllerPoseSetter;
	SetNewInputStateProcessingDelegate(initialiseState->newInputStateProcessing);
	SetNewInputEventsProcessingDelegate(initialiseState->newInputEventsProcessing);
	SetDisconnectDelegate(initialiseState->disconnect);
	SetMessageHandlerDelegate(initialiseState->messageHandler);
	SetProcessAudioInputDelegate(initialiseState->processAudioInput);
	SetGetUnixTimestampDelegate(initialiseState->getUnixTimestamp);

	reportHandshake=initialiseState->reportHandshake;

	if(enet_initialize() != 0)
	{
		TELEPORT_CERR<<"An error occurred while attempting to initalise ENet!\n";
		return false;
	}
	atexit(enet_deinitialize);

	bool result = clientManager.initialize(initialiseState->DISCOVERY_PORT, initialiseState->SERVICE_PORT, std::string(initialiseState->clientIP));

	if (!result)
	{
		TELEPORT_CERR<<"An error occurred while attempting to initalise clientManager!\n";
		return false;
	}

	clientManager.startAsyncNetworkDataProcessing();

	result = httpService->initialize(initialiseState->httpMountDirectory, initialiseState->certDirectory, initialiseState->privateKeyDirectory, initialiseState->SERVICE_PORT + 1);
	return result;
}

TELEPORT_EXPORT void Teleport_Shutdown()
{
	std::lock_guard<std::mutex> videoLock(videoMutex);
	std::lock_guard<std::mutex> audioLock(audioMutex);

	clientManager.stopAsyncNetworkDataProcessing(true);

	for(auto& uid : clientManager.GetClientUids())
	{
		auto &client= clientManager.GetClient(uid);
		if (!client)
			continue;
		if(client->isStreaming)
		{
			// This will add to lost clients and lost clients will be cleared below.
			// That's okay because the session is being stopped in Client_StopStreaming 
			// and the clientServices map is being cleared below too.
			Client_StopStreaming(uid);
		}
		else
		{
			client->clientMessaging->stopSession();
		}
	}

	clientManager.shutdown();
	httpService->shutdown();

	lostClients.clear();
	unlinkedClientIDs.clear();

	PluginGeometryStreamingService::callback_clientStoppedRenderingNode = nullptr;
	PluginGeometryStreamingService::callback_clientStartedRenderingNode = nullptr;

	setHeadPose = nullptr;
	setControllerPose = nullptr;
	processNewInputState = nullptr;
	processNewInputEvents = nullptr;
}

TELEPORT_EXPORT void Tick(float deltaTime)
{
	//Delete client data for clients who have been lost.
	for(avs::uid clientID : lostClients)
	{
		clientManager.removeClient(clientID);
	}
	lostClients.clear();

	clientManager.tick(deltaTime);

	PipeOutMessages();
}

TELEPORT_EXPORT void EditorTick()
{
	PipeOutMessages();
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

TELEPORT_EXPORT void AddUnlinkedClientID(avs::uid clientID)
{
	unlinkedClientIDs.insert(clientID);
}
///PLUGIN-SPECIFC END

///libavstream START
TELEPORT_EXPORT avs::uid GenerateUid()
{
	return avs::GenerateUid();
}
///libavstream END

TELEPORT_EXPORT avs::uid GetOrGenerateUid(const char *path)
{
	if(!path)
		return 0;
	std::string str=(path);
	return GeometryStore::GetInstance().GetOrGenerateUid(str);
}

TELEPORT_EXPORT avs::uid PathToUid(const char* path)
{
	if (!path)
		return 0;
	std::string str = (path);
	return GeometryStore::GetInstance().PathToUid(str);
}

TELEPORT_EXPORT size_t UidToPath(avs::uid u, char* const path, size_t len)
{
	std::string str = GeometryStore::GetInstance().UidToPath(u);
	if (str.length() < len)
	{
		// copy path including null-terminator char.
		memcpy(path, str.c_str(), str.length() + 1);
	}
	return str.length() + 1;
}

///GeometryStreamingService END


///VideoEncodePipeline START
TELEPORT_EXPORT bool GetVideoEncodeCapabilities(avs::EncodeCapabilities& capabilities)
{
	VideoEncodeParams params;
	params.deviceHandle = GraphicsManager::mGraphicsDevice;

	switch (GraphicsManager::mRendererType)
	{
	case kUnityGfxRendererD3D11:
	{
		params.deviceType = GraphicsDeviceType::Direct3D11;
		break;
	}
	case kUnityGfxRendererD3D12:
	{
		params.deviceType = GraphicsDeviceType::Direct3D12;
		break;
	}
	case kUnityGfxRendererVulkan:
	{
		params.deviceType = GraphicsDeviceType::Vulkan;
		break;
	}
	default:
		return false;
	};

	if (VideoEncodePipeline::getEncodeCapabilities(serverSettings, params, capabilities))
	{
		return true;
	}

	return false;
}

TELEPORT_EXPORT void InitializeVideoEncoder(avs::uid clientID, VideoEncodeParams& videoEncodeParams)
{
	std::lock_guard<std::mutex> lock(videoMutex);

	auto client = clientManager.GetClient(clientID);
	if (!client)
	{
		TELEPORT_CERR << "Failed to initialise video encoder for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	avs::Queue* cq = &client->clientMessaging->getClientNetworkContext()->NetworkPipeline.ColorQueue;
	avs::Queue* tq = &client->clientMessaging->getClientNetworkContext()->NetworkPipeline.TagDataQueue;
	Result result = client->videoEncodePipeline->configure(serverSettings,videoEncodeParams, cq, tq);
	if (!result)
	{
		TELEPORT_CERR << "Failed to initialise video encoder for Client " << clientID << "! Error occurred when trying to configure the video encoder pipeline!\n";
		client->clientMessaging->video_encoder_initialized = false;
	}
	else
		client->clientMessaging->video_encoder_initialized = true;
}

TELEPORT_EXPORT void ReconfigureVideoEncoder(avs::uid clientID, VideoEncodeParams& videoEncodeParams)
{
	std::lock_guard<std::mutex> lock(videoMutex);

	auto client = clientManager.GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "Failed to reconfigure video encoder for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	Result result = client->videoEncodePipeline->reconfigure(serverSettings, videoEncodeParams);
	if (!result)
	{
		TELEPORT_CERR << "Failed to reconfigure video encoder for Client " << clientID << "! Error occurred when trying to reconfigure the video encoder pipeline!\n";
		return;
	}

	///TODO: Need to retrieve encoder settings from unity.
	CasterEncoderSettings encoderSettings
	{
		videoEncodeParams.encodeWidth,
		videoEncodeParams.encodeHeight,
		0, // not used
		0, // not used
		false,
		true,
		true,
		10000,
		0
		,0
		,0
		,0
	};
	core::ReconfigureVideoCommand cmd;
	avs::VideoConfig& videoConfig = cmd.video_config;
	videoConfig.video_width = encoderSettings.frameWidth;
	videoConfig.video_height = encoderSettings.frameHeight;
	videoConfig.depth_height = encoderSettings.depthHeight;
	videoConfig.depth_width = encoderSettings.depthWidth;
	videoConfig.perspective_width = serverSettings.perspectiveWidth;
	videoConfig.perspective_height = serverSettings.perspectiveHeight;
	videoConfig.perspective_fov = serverSettings.perspectiveFOV;
	videoConfig.use_10_bit_decoding = serverSettings.use10BitEncoding;
	videoConfig.use_yuv_444_decoding = serverSettings.useYUV444Decoding;
	videoConfig.use_alpha_layer_decoding = serverSettings.useAlphaLayerEncoding;
	videoConfig.colour_cubemap_size = serverSettings.captureCubeSize;
	videoConfig.compose_cube = encoderSettings.enableDecomposeCube;
	videoConfig.videoCodec = serverSettings.videoCodec;
	videoConfig.use_cubemap = !serverSettings.usePerspectiveRendering;

	client->clientMessaging->sendCommand2(cmd);
}

TELEPORT_EXPORT void EncodeVideoFrame(avs::uid clientID, const uint8_t* tagData, size_t tagDataSize)
{
	std::lock_guard<std::mutex> lock(videoMutex);

	auto client = clientManager.GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "Failed to encode video frame for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	if(!client->clientMessaging->hasPeer())
	{
		TELEPORT_COUT << "Failed to encode video frame for Client " << clientID << "! Client has no peer!\n";
		return;
	}
	if (!client->clientMessaging->hasReceivedHandshake())
	{
		return;
	}
	if (!client->clientMessaging->video_encoder_initialized)
		return;
	if (!client->clientMessaging->getClientNetworkContext()->NetworkPipeline.isInitialized())
		return;
	Result result = client->videoEncodePipeline->encode(tagData, tagDataSize, client->videoKeyframeRequired);
	if(result)
	{
		client->videoKeyframeRequired = false;
	}
	else
	{
		TELEPORT_CERR << "Failed to encode video frame for Client " << clientID << "! Error occurred when trying to encode video!\n";

		// repeat the attempt for debugging purposes.
		result = client->videoEncodePipeline->encode(tagData, tagDataSize, client->videoKeyframeRequired);
		if(result)
		{
			client->videoKeyframeRequired = false;
		}
	}
}

struct EncodeVideoParamsWrapper
{
	avs::uid clientID;
	VideoEncodeParams videoEncodeParams;
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
		memcpy(&tagDataSize, buffer + sizeof(avs::uid), sizeof(tagDataSize));

		const uint8_t* tagData = buffer + sizeof(avs::uid) + sizeof(size_t);
		
		EncodeVideoFrame(clientID, tagData, tagDataSize);
	}
	else
	{
		TELEPORT_CERR << "Unknown event id" << "\n";
	}
}

TELEPORT_EXPORT UnityRenderingEventAndData GetRenderEventWithDataCallback()
{
	return OnRenderEventWithData;
}
///VideoEncodePipeline END

///AudioEncodePipeline START
TELEPORT_EXPORT void SetAudioSettings(const AudioSettings& newAudioSettings)
{
	audioSettings = newAudioSettings;
}

TELEPORT_EXPORT void SendAudio(const uint8_t* data, size_t dataSize)
{
	// Only continue processing if the main thread hasn't hung.
	double elapsedTime = avs::Platform::getTimeElapsedInSeconds(clientManager.getLastTickTimestamp(), avs::Platform::getTimestamp());
	if (elapsedTime > 0.15f)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(audioMutex);

	for (avs::uid clientID : clientManager.GetClientUids())
	{
		auto client = clientManager.GetClient(clientID);
		if (!client)
			continue;
		if (!client->clientMessaging->hasPeer())
		{
			continue;
		}
		// If handshake hasn't been received, the network pipeline is not set up yet, and can't receive packets from the AudioQueue.
		if (!client->clientMessaging->hasReceivedHandshake())
			continue;
		Result result = Result(Result::Code::OK);
		if (!client->audioEncodePipeline->isConfigured())
		{
			result = client->audioEncodePipeline->configure(serverSettings
				, audioSettings,&client->clientMessaging->getClientNetworkContext()->NetworkPipeline.AudioQueue);
			if (!result)
			{
				TELEPORT_CERR << "Failed to configure audio encoder pipeline for Client " << clientID << "!\n";
				continue;
			}
		}

		result = client->audioEncodePipeline->sendAudio(data, dataSize);
		if (!result)
		{
			TELEPORT_CERR << "Failed to send audio to Client " << clientID << "! Error occurred when trying to send audio" << "\n";
		}
	}
}
///AudioEncodePipeline END

///GeometryStore START
TELEPORT_EXPORT void SaveGeometryStore()
{
	GeometryStore::GetInstance().saveToDisk();
	GeometryStore::GetInstance().verify();
}

TELEPORT_EXPORT bool CheckGeometryStoreForErrors()
{
	return GeometryStore::GetInstance().CheckForErrors();
}

TELEPORT_EXPORT void LoadGeometryStore(size_t* meshAmount, LoadedResource** meshes, size_t* textureAmount, LoadedResource** textures, size_t* materialAmount, LoadedResource** materials)
{
	GeometryStore::GetInstance().loadFromDisk(*meshAmount, *meshes, *textureAmount, *textures, *materialAmount, *materials);
}

TELEPORT_EXPORT void ClearGeometryStore()
{
	GeometryStore::GetInstance().clear(true);
}

TELEPORT_EXPORT void SetDelayTextureCompression(bool willDelay)
{
	GeometryStore::GetInstance().willDelayTextureCompression = willDelay;
}

TELEPORT_EXPORT void SetCompressionLevels(uint8_t compressionStrength, uint8_t compressionQuality)
{
	GeometryStore::GetInstance().setCompressionLevels(compressionStrength, compressionQuality);
}

TELEPORT_EXPORT void StoreNode(avs::uid id, InteropNode node)
{
	avs::Node avsNode(node);
	GeometryStore::GetInstance().storeNode(id, avsNode);
}

TELEPORT_EXPORT void StoreSkin(avs::uid id, InteropSkin skin)
{
	avs::Skin avsSkin(skin);
	GeometryStore::GetInstance().storeSkin(id, avsSkin, avs::AxesStandard::UnityStyle);
}

TELEPORT_EXPORT void StoreTransformAnimation(avs::uid animationID, InteropTransformAnimation* animation)
{
	teleport::core::Animation a(*animation);
	GeometryStore::GetInstance().storeAnimation(animationID, a, avs::AxesStandard::UnityStyle);
}

TELEPORT_EXPORT void StoreMesh(avs::uid id, const char *  guid, const char *  path, std::time_t lastModified, const InteropMesh* mesh, avs::AxesStandard extractToStandard, bool compress,bool verify)
{
	avs::Mesh avsMesh(*mesh);
	GeometryStore::GetInstance().storeMesh(id, (guid), (path), lastModified, avsMesh, extractToStandard,compress,verify);
}

TELEPORT_EXPORT void StoreMaterial(avs::uid id, const char *  guid, const char *  path, std::time_t lastModified, InteropMaterial material)
{
	avs::Material avsMaterial(material);
	GeometryStore::GetInstance().storeMaterial(id, (guid), (path), lastModified, avsMaterial);
}

TELEPORT_EXPORT void StoreTexture(avs::uid id, const char * guid, const char *  relative_asset_path, std::time_t lastModified, InteropTexture texture, char* basisFileLocation,  bool genMips, bool highQualityUASTC, bool forceOverwrite)
{
	avs::Texture avsTexture(texture);
	GeometryStore::GetInstance().storeTexture(id, (guid), (relative_asset_path), lastModified, avsTexture, basisFileLocation,  genMips,  highQualityUASTC, forceOverwrite);
}

TELEPORT_EXPORT avs::uid StoreFont( const char *  ttf_path,const char *  relative_asset_path,std::time_t lastModified, int size)
{
	return GeometryStore::GetInstance().storeFont((ttf_path), (relative_asset_path),lastModified,size);
}

TELEPORT_EXPORT avs::uid StoreTextCanvas( const char *  relative_asset_path, const InteropTextCanvas *interopTextCanvas)
{
	avs::uid u=GeometryStore::GetInstance().storeTextCanvas((relative_asset_path),interopTextCanvas);
	if(u)
	{
		for(avs::uid u: clientManager.GetClientUids())
		{
			auto client = clientManager.GetClient(u);
			if(client->clientMessaging->GetGeometryStreamingService().hasResource(u))
				client->clientMessaging->GetGeometryStreamingService().requestResource(u);
		}
	}
	return u;
}

// TODO: This is a really basic resend/update function. Must make better.
TELEPORT_EXPORT void ResendNode(avs::uid u)
{
	for (avs::uid u : clientManager.GetClientUids())
	{
		auto client = clientManager.GetClient(u);
		if(client->clientMessaging->GetGeometryStreamingService().hasResource(u))
			client->clientMessaging->GetGeometryStreamingService().requestResource(u);
	}
}

TELEPORT_EXPORT bool GetFontAtlas( const char *  ttf_path,  InteropFontAtlas *interopFontAtlas)
{
	return teleport::server::Font::GetInstance().GetInteropFontAtlas((ttf_path),interopFontAtlas);
}

TELEPORT_EXPORT void StoreShadowMap(avs::uid id, const char *  guid, const char *  path, std::time_t lastModified, InteropTexture shadowMap)
{
	avs::Texture avsTexture(shadowMap);
	GeometryStore::GetInstance().storeShadowMap(id, guid, path, lastModified, avsTexture);
}

TELEPORT_EXPORT bool IsNodeStored(avs::uid id)
{
	const avs::Node* node = GeometryStore::GetInstance().getNode(id);
	return node != nullptr;
}

TELEPORT_EXPORT bool IsSkinStored(avs::uid id)
{
	//NOTE: Assumes we always are storing animations in the engineering axes standard.
	const avs::Skin* skin = GeometryStore::GetInstance().getSkin(id, avs::AxesStandard::EngineeringStyle);
	return skin != nullptr;
}

TELEPORT_EXPORT bool IsMeshStored(avs::uid id)
{
	//NOTE: Assumes we always are storing meshes in the engineering axes standard.
	const avs::Mesh* mesh = GeometryStore::GetInstance().getMesh(id, avs::AxesStandard::EngineeringStyle);
	return mesh != nullptr;
}

TELEPORT_EXPORT bool IsMaterialStored(avs::uid id)
{
	const avs::Material* material = GeometryStore::GetInstance().getMaterial(id);
	return material != nullptr;
}

TELEPORT_EXPORT bool IsTextureStored(avs::uid id)
{
	const avs::Texture* texture = GeometryStore::GetInstance().getTexture(id);
	return texture != nullptr;
}

TELEPORT_EXPORT void RemoveNode(avs::uid nodeID)
{
	GeometryStore::GetInstance().removeNode(nodeID);
}

TELEPORT_EXPORT avs::Node* getNode(avs::uid nodeID)
{
	return GeometryStore::GetInstance().getNode(nodeID);
}

TELEPORT_EXPORT uint64_t GetNumberOfTexturesWaitingForCompression()
{
	return static_cast<int64_t>(GeometryStore::GetInstance().getNumberOfTexturesWaitingForCompression());
}

///TODO: Free memory of allocated string, or use passed in string to return message.
TELEPORT_EXPORT bool GetMessageForNextCompressedTexture(uint64_t textureIndex, uint64_t totalTextures,char *str,size_t len)
{
	const avs::Texture* texture = GeometryStore::GetInstance().getNextTextureToCompress();

	std::stringstream messageStream;
	//Write compression message to  string stream.
	messageStream << "Compressing texture " << textureIndex << "/" << totalTextures << " (" << texture->name.data() << " [" << texture->width << " x " << texture->height << "])";

	memcpy(str,messageStream.str().data(),std::min(len,messageStream.str().length()));
	return true;
}

TELEPORT_EXPORT void CompressNextTexture()
{
	GeometryStore::GetInstance().compressNextTexture();
}
///GeometryStore END

TELEPORT_EXPORT size_t SizeOf(const char* str)
{
	std::string n=str;
	if(n=="ServerSettings")
	{
		return sizeof(ServerSettings);
	}
	if(n=="ClientSettings")
	{
		return sizeof(teleport::server::ClientSettings);
	}
	if(n=="ClientDynamicLighting")
	{
		return sizeof(avs::ClientDynamicLighting);
	}
	TELEPORT_CERR<<"Unknown type for SizeOf: "<<str<<"\n";
	return 0;
}