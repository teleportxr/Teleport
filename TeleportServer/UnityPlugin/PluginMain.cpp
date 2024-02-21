#include <functional>
#include <iostream>
#include <queue>
#include <sstream>
#include <vector>
#include <unordered_map>

#include "libavstream/common.hpp"

#include "TeleportCore/Profiling.h"

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
#include "TeleportAudio/CustomAudioStreamTarget.h"
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
#include <regex>

using namespace teleport;
using namespace server;


static avs::Context avsContext;

AudioSettings audioSettings;

std::set<avs::uid> unlinkedClientIDs; //Client IDs that haven't been linked to a session component.

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

namespace teleport
{
	namespace server
	{
	/// The collected values required to initialize a server session; see Server_Teleport_Initialize().
		struct InitializationSettings
		{
			char* clientIP;														///< IP address to match to connecting clients. May be blank.
			char* httpMountDirectory;											///< Local (server-side) directory for HTTP requests: usually the Teleport cache directory.
			char* certDirectory;												///< Local directory for HTTP certificates.
			char* privateKeyDirectory;											///< Local directory for private keys.
			char* signalingPorts;												///< Optional list of ports to listen for signaling connections and queries.

			ClientStoppedRenderingNodeFn clientStoppedRenderingNode;			///< Delegate to be called when client is no longer rendering a specified node.
			ClientStartedRenderingNodeFn clientStartedRenderingNode;
			SetHeadPoseFn headPoseSetter;
			SetControllerPoseFn controllerPoseSetter;
			ProcessNewInputStateFn newInputStateProcessing;
			ProcessNewInputEventsFn newInputEventsProcessing;
			DisconnectFn disconnect;
			avs::MessageHandlerFunc messageHandler;
			ReportHandshakeFn reportHandshake;
			ProcessAudioInputFn processAudioInput;
			GetUnixTimestampFn getUnixTimestampNs;
			int64_t start_unix_time_us;
		};
	}
}

static void passOnOutput(const char *msg)
{
	TELEPORT_PROFILE_AUTOZONE;
	if (msg)
		avsContext.log(avs::LogSeverity::Info, msg);
}

static void passOnError(const char *msg)
{
	TELEPORT_PROFILE_AUTOZONE;
	if (msg)
		avsContext.log(avs::LogSeverity::Error, msg);
}

void AccumulateMessagesFromThreads(avs::LogSeverity severity, const char *msg, void *userData)
{
	TELEPORT_PROFILE_AUTOZONE;
	std::lock_guard<std::mutex> lock(messagesMutex);
	if (severity == avs::LogSeverity::Error || severity == avs::LogSeverity::Critical)
	{
		LogMessage tst = {severity, msg, userData};
		// can break here.
	}
	if (messages.size() == 99)
	{
		LogMessage logMessage = {avs::LogSeverity::Error, "Too many messages since last call to PipeOutMessages()", nullptr};
		messages.push_back(std::move(logMessage));
		return;
	}
	else if (messages.size() > 99)
	{
		return;
	}
	LogMessage logMessage = {severity, msg, userData};
	messages.push_back(std::move(logMessage));
}

void PipeOutMessages()
{
	TELEPORT_PROFILE_ZONE(PipeOut);
	std::lock_guard<std::mutex> lock(messagesMutex);
	if (messageHandler)
	{
		for (LogMessage &message : messages)
		{
			messageHandler(message.severity, message.msg.c_str(), message.userData);
		}
		messages.clear();
	}
}

#ifndef AxesConversions
			/// Convert the transform between two avs::AxesStandard's.
			TELEPORT_EXPORT void Server_ConvertTransform(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, avs::Transform &transform)
			{
				avs::ConvertTransform(fromStandard, toStandard, transform);
			}
			/// Convert a quaternion rotation between two avs::AxesStandard's.
			TELEPORT_EXPORT void Server_ConvertRotation(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, vec4 &rotation)
			{
				avs::ConvertRotation(fromStandard, toStandard, rotation);
			}
			/// Convert a position between two avs::AxesStandard's.
			TELEPORT_EXPORT void Server_ConvertPosition(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, vec3 &position)
			{
				avs::ConvertPosition(fromStandard, toStandard, position);
			}
			/// Convert a scale between two avs::AxesStandard's.
			TELEPORT_EXPORT void Server_ConvertScale(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, vec3 &scale)
			{
				avs::ConvertScale(fromStandard, toStandard, scale);
			}
			/// Convert the specified axis index between two avs::AxesStandard's.
			TELEPORT_EXPORT int8_t Server_ConvertAxis(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, int8_t axis)
			{
				return avs::ConvertAxis(fromStandard, toStandard, axis);
			}
#endif
#ifndef MemoryManagement
			/// Delete an array that was created in the dll.
			TELEPORT_EXPORT void Server_DeleteUnmanagedArray(void **unmanagedArray)
			{
				delete[] (uint8_t *)*unmanagedArray;
			}
			/// Returns the size of the named structure according to the dll. This should match the size of the corresponding interop structure in the engine.
			TELEPORT_EXPORT size_t Server_SizeOf(const char *str)
			{
				TELEPORT_PROFILE_AUTOZONE;
				std::string n = str;
				if (n == "ServerSettings")
				{
					return sizeof(ServerSettings);
				}
				if (n == "ClientSettings")
				{
					return sizeof(teleport::server::ClientSettings);
				}
				if (n == "ClientDynamicLighting")
				{
					return sizeof(avs::ClientDynamicLighting);
				}
				TELEPORT_CERR << "Unknown type for SizeOf: " << str << "\n";
				return 0;
			}
#endif

#ifndef SettingProperties
			/// Apply new server settings.
			TELEPORT_EXPORT void Server_UpdateServerSettings(const ServerSettings newSettings)
			{
				serverSettings = newSettings;
			}

			/// Set the local (server-side) path where cached streamable resources are stored.
			TELEPORT_EXPORT bool Server_SetCachePath(const char* path)
			{
				return GeometryStore::GetInstance().SetCachePath(path);
			}

			/// Tell the dll how long to wait for a timeout.
			TELEPORT_EXPORT void Server_SetConnectionTimeout(int32_t timeout)
			{
				connectionTimeout = timeout;
			}
#endif
#ifndef SettingDelegates
			/// Tell the dll what delegate to use when a client has stopped rendering a node.
			TELEPORT_EXPORT void Server_SetClientStoppedRenderingNodeDelegate(ClientStoppedRenderingNodeFn clientStoppedRenderingNode)
			{
				PluginGeometryStreamingService::callback_clientStoppedRenderingNode = clientStoppedRenderingNode;
			}
			/// Tell the dll what delegate to use when a client has started rendering a node.
			TELEPORT_EXPORT void Server_SetClientStartedRenderingNodeDelegate(ClientStartedRenderingNodeFn clientStartedRenderingNode)
			{
				PluginGeometryStreamingService::callback_clientStartedRenderingNode = clientStartedRenderingNode;
			}

			/// Tell the dll what delegate to use when a client has updated its head pose.
			TELEPORT_EXPORT void Server_SetHeadPoseSetterDelegate(SetHeadPoseFn headPoseSetter)
			{
				setHeadPose = headPoseSetter;
			}
			/// Tell the dll what delegate to use when a client has sent the server new input state data to process.
			TELEPORT_EXPORT void Server_SetNewInputStateProcessingDelegate(ProcessNewInputStateFn newInputProcessing)
			{
				processNewInputState = newInputProcessing;
			}
			/// Tell the dll what delegate to use when a client has sent the server new input events to process.
			TELEPORT_EXPORT void Server_SetNewInputEventsProcessingDelegate(ProcessNewInputEventsFn newInputProcessing)
			{
				processNewInputEvents = newInputProcessing;
			}

			/// Tell the dll what delegate to use when a client has disconnected.
			TELEPORT_EXPORT void Server_SetDisconnectDelegate(DisconnectFn disconnect)
			{
				onDisconnect = disconnect;
			}

			/// Tell the dll what delegate to use when a client has sent audio packets.
			TELEPORT_EXPORT void Server_SetProcessAudioInputDelegate(ProcessAudioInputFn f)
			{
				processAudioInput = f;
			}

			/// Tell the dll what delegate to use to obtain a current Unix timestamp.
			TELEPORT_EXPORT void Server_SetGetUnixTimestampDelegate(GetUnixTimestampFn function)
			{
				getUnixTimestampNs = function;
			}
			/// Tell the dll what delegate to use to send log messages.
			TELEPORT_EXPORT void Server_SetMessageHandlerDelegate(avs::MessageHandlerFunc msgh)
			{
				TELEPORT_PROFILE_AUTOZONE;
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
#endif

			/// Initialize the server for a server session.
			TELEPORT_EXPORT bool Server_Teleport_Initialize(const teleport::server::InitializationSettings *initializationSettings)
			{
				unlinkedClientIDs.clear();

				Server_SetClientStoppedRenderingNodeDelegate(initializationSettings->clientStoppedRenderingNode);
				Server_SetClientStartedRenderingNodeDelegate(initializationSettings->clientStartedRenderingNode);
				Server_SetHeadPoseSetterDelegate(initializationSettings->headPoseSetter);

				setControllerPose = initializationSettings->controllerPoseSetter;
				Server_SetNewInputStateProcessingDelegate(initializationSettings->newInputStateProcessing);
				Server_SetNewInputEventsProcessingDelegate(initializationSettings->newInputEventsProcessing);
				Server_SetDisconnectDelegate(initializationSettings->disconnect);
				Server_SetMessageHandlerDelegate(initializationSettings->messageHandler);
				Server_SetProcessAudioInputDelegate(initializationSettings->processAudioInput);
				Server_SetGetUnixTimestampDelegate(initializationSettings->getUnixTimestampNs);

				reportHandshake=initializationSettings->reportHandshake;

				if (!initializationSettings->signalingPorts)
				{
					TELEPORT_CERR << "Failed to identify ports as string was null.";
					return false;
				}
				std::string str(initializationSettings->signalingPorts);
				std::string::size_type pos_begin = { 0 }, pos_end = { 0 };
				std::set<uint16_t> ports;
				do
				{
					pos_end = str.find_first_of(",", pos_begin);
					std::string str2 = str.substr(pos_begin, pos_end - pos_begin);
					uint16_t p=std::stoi(str2);
					ports.insert(p);
					pos_begin = pos_end + 1;
				} while (str.find_first_of(",", pos_end) != std::string::npos);
				if(!ports.size())
				{
					TELEPORT_CERR << "Failed to identify ports from string " << initializationSettings->signalingPorts  << "!\n";
					return false;
				}
				bool result = ClientManager::instance().initialize(ports, initializationSettings->start_unix_time_us,std::string(initializationSettings->clientIP));

				if (!result)
				{
					TELEPORT_CERR<<"An error occurred while attempting to initalise clientManager!\n";
					return false;
				}

				ClientManager::instance().startAsyncNetworkDataProcessing();

				result = httpService->initialize(initializationSettings->httpMountDirectory
					, initializationSettings->certDirectory
					, initializationSettings->privateKeyDirectory
					,80);
				return result;
			}

			/// Shut down the server.
			TELEPORT_EXPORT void Server_Teleport_Shutdown()
			{
				std::lock_guard<std::mutex> videoLock(videoMutex);
				std::lock_guard<std::mutex> audioLock(audioMutex);

				ClientManager::instance().stopAsyncNetworkDataProcessing(true);

				for(auto& uid : ClientManager::instance().GetClientUids())
				{
					auto &client= ClientManager::instance().GetClient(uid);
					if (!client)
						continue;
					if(client->GetConnectionState()!=UNCONNECTED)
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

				ClientManager::instance().shutdown();
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

			/// Perform periodic (e.g. once-per-frame) updates while playing.
			TELEPORT_EXPORT void Server_Tick(float deltaTime)
			{
				TELEPORT_PROFILE_AUTOZONE;
				//Delete client data for clients who have been lost.
				for(avs::uid clientID : lostClients)
				{
					ClientManager::instance().removeClient(clientID);
				}
				lostClients.clear();

				ClientManager::instance().tick(deltaTime);

				PipeOutMessages();

				TELEPORT_FRAME_END;
			}

			/// Perform periodic (e.g. once-per-frame) updates for Editor mode (not playing).
			TELEPORT_EXPORT void Server_EditorTick()
			{
				TELEPORT_PROFILE_AUTOZONE;
				GeometryStore::GetInstance().compressNextTexture();
				PipeOutMessages();
			}

			/// Get an id for a client that has connected but not yet been hooked up engine-side.
			TELEPORT_EXPORT avs::uid Server_GetUnlinkedClientID()
			{
				TELEPORT_PROFILE_AUTOZONE;
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

			//PLUGIN-SPECIFC END

			//libavstream START

			/// Get a new unique id.
			TELEPORT_EXPORT avs::uid Server_GenerateUid()
			{
				TELEPORT_PROFILE_AUTOZONE;
				return avs::GenerateUid();
			}
			//libavstream END

			/// Get the unique id (avs::uid) corresponding to the given resource path. Generate a new one if no id is yet associated with this path.
			TELEPORT_EXPORT avs::uid Server_GetOrGenerateUid(const char *path)
			{
				TELEPORT_PROFILE_AUTOZONE;
				if(!path)
					return 0;
				std::string str=(path);
				return GeometryStore::GetInstance().GetOrGenerateUid(str);
			}

			/// Get the unique id (avs::uid) corresponding to the given resource path. Returns 0 if none is defined.
			TELEPORT_EXPORT avs::uid Server_PathToUid(const char* path)
			{
				TELEPORT_PROFILE_AUTOZONE;
				if (!path)
					return 0;
				std::string str = (path);
				return GeometryStore::GetInstance().PathToUid(str);
			}

			/// Get the resource path corresponding to the given unique id. Returns the length of the path string, or 0 if none is defined.
			TELEPORT_EXPORT size_t Server_UidToPath(avs::uid u, char* const path, size_t len)
			{
				TELEPORT_PROFILE_AUTOZONE;
				std::string str = GeometryStore::GetInstance().UidToPath(u);
				if (str.length() < len)
				{
					// copy path including null-terminator char.
					memcpy(path, str.c_str(), str.length() + 1);
				}
				return str.length() + 1;
			}

			/// If the resource is already loaded in memory, return true. If it is not, try to load it from the file cache and return true if succeeded, false otherwise.
			TELEPORT_EXPORT bool Server_EnsureResourceIsLoaded(avs::uid u)
			{
				TELEPORT_PROFILE_AUTOZONE;
				if (GeometryStore::GetInstance().EnsureResourceIsLoaded(u))
					return true;
				return 0;
			}
			/// If the resource is loaded in memory, return the uid for the given path. If not, try to load it, return the corresponding uid if successful or 0 if not.
			TELEPORT_EXPORT avs::uid Server_EnsurePathResourceIsLoaded(const char * path)
			{
				TELEPORT_PROFILE_AUTOZONE;
				if (!path)
					return 0;
				std::string str = (path);
				avs::uid u=GeometryStore::GetInstance().GetOrGenerateUid(str);
				if(GeometryStore::GetInstance().EnsureResourceIsLoaded(u))
					return u;
				return 0;
			}

			//GeometryStreamingService END

			/// Request the dll fill in the server session state.
			TELEPORT_EXPORT bool Server_Teleport_GetSessionState(teleport::server::SessionState &sessionState)
			{
				sessionState = ClientManager::instance().getSessionState();
				return true;
			}

			//VideoEncodePipeline START

			/// Get the video encoding capabilities of this server.
			TELEPORT_EXPORT bool Server_GetVideoEncodeCapabilities(avs::EncodeCapabilities& capabilities)
			{
				TELEPORT_PROFILE_AUTOZONE;
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

			/// Initialize for video encoding.
			TELEPORT_EXPORT void Server_InitializeVideoEncoder(avs::uid clientID, VideoEncodeParams& videoEncodeParams)
			{
				TELEPORT_PROFILE_AUTOZONE;
				std::lock_guard<std::mutex> lock(videoMutex);

				auto client = ClientManager::instance().GetClient(clientID);
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

			/// Reconfigure video encoding while running.
			TELEPORT_EXPORT void Server_ReconfigureVideoEncoder(avs::uid clientID, VideoEncodeParams& videoEncodeParams)
			{
				TELEPORT_PROFILE_AUTOZONE;
				std::lock_guard<std::mutex> lock(videoMutex);

				auto client = ClientManager::instance().GetClient(clientID);
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

				client->clientMessaging->sendReconfigureVideoCommand(cmd);
			}

			/// Encode the given (uncompressed) video frame from memory.
			TELEPORT_EXPORT void Server_EncodeVideoFrame(avs::uid clientID, const uint8_t* tagData, size_t tagDataSize)
			{
				TELEPORT_PROFILE_AUTOZONE;
				std::lock_guard<std::mutex> lock(videoMutex);

				auto client = ClientManager::instance().GetClient(clientID);
				if(!client)
				{
					TELEPORT_CERR << "Failed to encode video frame for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
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

			// GeometryStore START
			/// Save all the resources from memory to disk.
			TELEPORT_EXPORT void Server_SaveGeometryStore()
			{
				TELEPORT_PROFILE_AUTOZONE;
				GeometryStore::GetInstance().saveToDisk();
				GeometryStore::GetInstance().verify();
			}

			/// Check all resources in memory for errors.
			TELEPORT_EXPORT bool Server_CheckGeometryStoreForErrors()
			{
				TELEPORT_PROFILE_AUTOZONE;
				return GeometryStore::GetInstance().CheckForErrors();
			}

			/// Load all resources that can be found in the disk cache into memory.
			TELEPORT_EXPORT void Server_LoadGeometryStore(size_t *meshAmount, LoadedResource **meshes, size_t *textureAmount, LoadedResource **textures, size_t *materialAmount, LoadedResource **materials)
			{
				TELEPORT_PROFILE_AUTOZONE;
				GeometryStore::GetInstance().loadFromDisk(*meshAmount, *meshes, *textureAmount, *textures, *materialAmount, *materials);
			}

			/// Clear all resources from memory.
			TELEPORT_EXPORT void Server_ClearGeometryStore()
			{
				TELEPORT_PROFILE_AUTOZONE;
				GeometryStore::GetInstance().clear(true);
			}

			/// Tell the dll whether to delay texture compression (rather than compress instantly when textures are stored).
			TELEPORT_EXPORT void Server_SetDelayTextureCompression(bool willDelay)
			{
				TELEPORT_PROFILE_AUTOZONE;
				GeometryStore::GetInstance().willDelayTextureCompression = willDelay;
			}

			/// Tell the dll what compression values to use for textures. Applies only to future compression events, does not recompress existing data.
			TELEPORT_EXPORT void Server_SetCompressionLevels(uint8_t compressionStrength, uint8_t compressionQuality)
			{
				TELEPORT_PROFILE_AUTOZONE;
				GeometryStore::GetInstance().setCompressionLevels(compressionStrength, compressionQuality);
			}

			/// Store the given node in memory.
			TELEPORT_EXPORT void Server_StoreNode(avs::uid id, InteropNode node)
			{
				TELEPORT_PROFILE_AUTOZONE;
				avs::Node avsNode(node);
				GeometryStore::GetInstance().storeNode(id, avsNode);
			}

			/// Get the given node's data if stored.
			TELEPORT_EXPORT bool Server_GetNode(avs::uid id, InteropNode *node)
			{
				TELEPORT_PROFILE_AUTOZONE;
				auto *avsNode = GeometryStore::GetInstance().getNode(id);
				if (avsNode)
				{
					node->name = avsNode->name.c_str();
					node->dataID = avsNode->data_uid;
					node->dataType = avsNode->data_type;
					return true;
				}
				return false;
			}

			/// Store the given skeleton in memory.
			TELEPORT_EXPORT void Server_StoreSkeleton(avs::uid id, InteropSkeleton skeleton)
			{
				TELEPORT_PROFILE_AUTOZONE;
				avs::Skeleton avsSkeleton(skeleton);
				GeometryStore::GetInstance().storeSkeleton(id, avsSkeleton, avs::AxesStandard::UnityStyle);
			}

			/// Store the given animation in memory and on disk.
			TELEPORT_EXPORT void Server_StoreTransformAnimation(avs::uid animationID, const char *path, InteropTransformAnimation *animation)
			{
				TELEPORT_PROFILE_AUTOZONE;
				teleport::core::Animation a(*animation);
				GeometryStore::GetInstance().storeAnimation(animationID, path, a, avs::AxesStandard::UnityStyle);
			}

			/// Store the given mesh in memory and on disk.
			TELEPORT_EXPORT void Server_StoreMesh(avs::uid id, const char *guid, const char *path, std::time_t lastModified, const InteropMesh *mesh, avs::AxesStandard extractToStandard, bool compress, bool verify)
			{
				TELEPORT_PROFILE_AUTOZONE;
				avs::Mesh avsMesh(*mesh);
				GeometryStore::GetInstance().storeMesh(id, (guid), (path), lastModified, avsMesh, extractToStandard, compress, verify);
			}

			/// Store the given material in memory and on disk.
			TELEPORT_EXPORT void Server_StoreMaterial(avs::uid id, const char *guid, const char *path, std::time_t lastModified, InteropMaterial material)
			{
				TELEPORT_PROFILE_AUTOZONE;
				avs::Material avsMaterial(material);
				GeometryStore::GetInstance().storeMaterial(id, (guid), (path), lastModified, avsMaterial);
			}

			/// Store the given texture in memory and on disk.
			TELEPORT_EXPORT void Server_StoreTexture(avs::uid id, const char *guid, const char *relative_asset_path, std::time_t lastModified, InteropTexture texture, bool genMips, bool highQualityUASTC, bool forceOverwrite)
			{
				TELEPORT_PROFILE_AUTOZONE;
				avs::Texture avsTexture(texture);
				GeometryStore::GetInstance().storeTexture(id, (guid), (relative_asset_path), lastModified, avsTexture, genMips, highQualityUASTC, forceOverwrite);
			}

			/// Store the given font in memory and on disk.
			TELEPORT_EXPORT avs::uid Server_StoreFont(const char *ttf_path, const char *relative_asset_path, std::time_t lastModified, int size)
			{
				TELEPORT_PROFILE_AUTOZONE;
				return GeometryStore::GetInstance().storeFont((ttf_path), (relative_asset_path), lastModified, size);
			}

			/// Store the given text canvas in memory.
			TELEPORT_EXPORT avs::uid Server_StoreTextCanvas(const char *relative_asset_path, const InteropTextCanvas *interopTextCanvas)
			{
				TELEPORT_PROFILE_AUTOZONE;
				avs::uid u = GeometryStore::GetInstance().storeTextCanvas((relative_asset_path), interopTextCanvas);
				if (u)
				{
					for (avs::uid u : ClientManager::instance().GetClientUids())
					{
						auto client = ClientManager::instance().GetClient(u);
						if (!client)
							continue;
						if (client->clientMessaging->GetGeometryStreamingService().hasResource(u))
							client->clientMessaging->GetGeometryStreamingService().requestResource(u);
					}
				}
				return u;
			}

			// TODO: This is a really basic resend/update function. Must make better.

			/// Resend the specified node to all clients.
			TELEPORT_EXPORT void Server_ResendNode(avs::uid u)
			{
				TELEPORT_PROFILE_AUTOZONE;
				for (avs::uid u : ClientManager::instance().GetClientUids())
				{
					auto client = ClientManager::instance().GetClient(u);
					if (!client)
						continue;
					if (client->clientMessaging->GetGeometryStreamingService().hasResource(u))
						client->clientMessaging->GetGeometryStreamingService().requestResource(u);
				}
			}

			/// Get the font atlas which has the given path.
			TELEPORT_EXPORT bool Server_GetFontAtlas(const char *ttf_path, InteropFontAtlas *interopFontAtlas)
			{
				TELEPORT_PROFILE_AUTOZONE;
				return teleport::server::Font::GetInstance().GetInteropFontAtlas((ttf_path), interopFontAtlas);
			}

			/// Store a shadow map in memory.
			TELEPORT_EXPORT void Server_StoreShadowMap(avs::uid id, const char *guid, const char *path, std::time_t lastModified, InteropTexture shadowMap)
			{
				TELEPORT_PROFILE_AUTOZONE;
				avs::Texture avsTexture(shadowMap);
				GeometryStore::GetInstance().storeShadowMap(id, guid, path, lastModified, avsTexture);
			}

			/// Returns true if id is the id of a node stored in memory.
			TELEPORT_EXPORT bool Server_IsNodeStored(avs::uid id)
			{
				TELEPORT_PROFILE_AUTOZONE;
				const avs::Node *node = GeometryStore::GetInstance().getNode(id);
				return node != nullptr;
			}

			/// Returns true if id is the id of a skeleton stored in memory.
			TELEPORT_EXPORT bool Server_IsSkeletonStored(avs::uid id)
			{
				TELEPORT_PROFILE_AUTOZONE;
				// NOTE: Assumes we always are storing animations in the engineering axes standard.
				const avs::Skeleton *skeleton = GeometryStore::GetInstance().getSkeleton(id, avs::AxesStandard::EngineeringStyle);
				return skeleton != nullptr;
			}

			/// Returns true if id is the id of a mesh stored in memory.
			TELEPORT_EXPORT bool Server_IsMeshStored(avs::uid id)
			{
				TELEPORT_PROFILE_AUTOZONE;
				// NOTE: Assumes we always are storing meshes in the engineering axes standard.
				const avs::Mesh *mesh = GeometryStore::GetInstance().getMesh(id, avs::AxesStandard::EngineeringStyle);
				return mesh != nullptr;
			}

			/// Returns true if id is the id of a material stored in memory.
			TELEPORT_EXPORT bool Server_IsMaterialStored(avs::uid id)
			{
				TELEPORT_PROFILE_AUTOZONE;
				const avs::Material *material = GeometryStore::GetInstance().getMaterial(id);
				return material != nullptr;
			}

			/// Returns true if id is the id of a texture stored in memory.
			TELEPORT_EXPORT bool Server_IsTextureStored(avs::uid id)
			{
				TELEPORT_PROFILE_AUTOZONE;
				const avs::Texture *texture = GeometryStore::GetInstance().getTexture(id);
				return texture != nullptr;
			}

			/// Remove the given node from memory.
			TELEPORT_EXPORT void Server_RemoveNode(avs::uid nodeID)
			{
				TELEPORT_PROFILE_AUTOZONE;
				GeometryStore::GetInstance().removeNode(nodeID);
			}

			/// Returns how many textures are in the queue to be compressed and stored.
			TELEPORT_EXPORT uint64_t Server_GetNumberOfTexturesWaitingForCompression()
			{
				TELEPORT_PROFILE_AUTOZONE;
				return static_cast<int64_t>(GeometryStore::GetInstance().getNumberOfTexturesWaitingForCompression());
			}

			// TODO: Free memory of allocated string, or use passed in string to return message.
			/// Get a progress update message for the texture next in the compression queue.
			TELEPORT_EXPORT bool Server_GetMessageForNextCompressedTexture(char *str, size_t len)
			{
				TELEPORT_PROFILE_AUTOZONE;
				const avs::Texture *texture = GeometryStore::GetInstance().getNextTextureToCompress();
				if (!texture)
				{
					return false;
				}
				std::stringstream messageStream;
				// Write compression message to  string stream.
				messageStream << "Compressing texture "
							  << " (" << texture->name.data() << " [" << texture->width << " x " << texture->height << "])";

				memcpy(str, messageStream.str().data(), std::min(len, messageStream.str().length()));
				return true;
			}

			/// Compress the next texture in the queue.
			TELEPORT_EXPORT void Server_CompressNextTexture()
			{
				TELEPORT_PROFILE_AUTOZONE;
				GeometryStore::GetInstance().compressNextTexture();
			}

			// GeometryStore END

			// AudioEncodePipeline START
			/// Assign new audio settings.
			TELEPORT_EXPORT void Server_SetAudioSettings(const AudioSettings &newAudioSettings)
			{
				audioSettings = newAudioSettings;
			}

			/// Send a chunk of audio data to all clients.
			TELEPORT_EXPORT void Server_SendAudio(const uint8_t *data, size_t dataSize)
			{
				TELEPORT_PROFILE_AUTOZONE;
				// Only continue processing if the main thread hasn't hung.
				double elapsedTime = avs::Platform::getTimeElapsedInSeconds(ClientManager::instance().getLastTickTimestamp(), avs::Platform::getTimestamp());
				if (elapsedTime > 0.15f)
				{
					return;
				}

				std::lock_guard<std::mutex> lock(audioMutex);

				for (avs::uid clientID : ClientManager::instance().GetClientUids())
				{
					auto client = ClientManager::instance().GetClient(clientID);
					if (!client)
						continue;
					// If handshake hasn't been received, the network pipeline is not set up yet, and can't receive packets from the AudioQueue.
					if (!client->clientMessaging->hasReceivedHandshake())
						continue;
					Result result = Result(Result::Code::OK);
					if (!client->audioEncodePipeline->isConfigured())
					{
						result = client->audioEncodePipeline->configure(serverSettings, audioSettings, &client->clientMessaging->getClientNetworkContext()->NetworkPipeline.AudioQueue);
						if (!result)
						{
							TELEPORT_CERR << "Failed to configure audio encoder pipeline for Client " << clientID << "!\n";
							continue;
						}
					}

					result = client->audioEncodePipeline->sendAudio(data, dataSize);
					if (!result)
					{
						TELEPORT_CERR << "Failed to send audio to Client " << clientID << "! Error occurred when trying to send audio"
									  << "\n";
					}
				}
			}
			// AudioEncodePipeline END
struct EncodeVideoParamsWrapper
{
	avs::uid clientID;
	VideoEncodeParams videoEncodeParams;
};

static void UNITY_INTERFACE_API OnRenderEventWithData(int eventID, void* data)
{
	TELEPORT_PROFILE_AUTOZONE;
	if (eventID == 0)
	{
		auto wrapper = (EncodeVideoParamsWrapper*)data;
		Server_InitializeVideoEncoder(wrapper->clientID, wrapper->videoEncodeParams);
	}
	else if (eventID == 1)
	{
		auto wrapper = (EncodeVideoParamsWrapper*)data;
		Server_ReconfigureVideoEncoder(wrapper->clientID, wrapper->videoEncodeParams);
	}
	else if (eventID == 2)
	{
		const auto buffer = (uint8_t*)data;

		avs::uid clientID;
		memcpy(&clientID, buffer, sizeof(avs::uid));

		uint32_t tagDataSize;
		memcpy(&tagDataSize, buffer + sizeof(avs::uid), sizeof(tagDataSize));

		const uint8_t* tagData = buffer + sizeof(avs::uid) + sizeof(size_t);
		
		Server_EncodeVideoFrame(clientID, tagData, tagDataSize);
	}
	else
	{
		TELEPORT_CERR << "Unknown event id" << "\n";
	}
}

TELEPORT_EXPORT UnityRenderingEventAndData Server_GetRenderEventWithDataCallback()
{
	TELEPORT_PROFILE_AUTOZONE;
	return OnRenderEventWithData;
}
///VideoEncodePipeline END
