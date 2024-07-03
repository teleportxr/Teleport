#include "ClientManager.h"

#include "ClientData.h"
#include "ClientMessaging.h"

#include <algorithm>
#include <iostream>
#include <random>

#include "libavstream/common_input.h"
#include "TeleportCore/CommonNetworking.h"


#include "SignalingService.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Threads.h"
#include "TeleportCore/Time.h"
#include "UnityPlugin/PluginClient.h"
#include "TeleportCore/Logging.h"
#include "TeleportCore/Profiling.h"
#ifdef _MSC_VER
#include "VisualStudioDebugOutput.h"
std::shared_ptr<VisualStudioDebugOutput> debug_buffer;
#else
#include "UnixDebugOutput.h"
std::shared_ptr<DebugOutput> debug_buffer(true, "teleport_server.log", 128);
#endif
#pragma optimize("",off)
namespace teleport
{
	namespace server
	{
		avs::Context avsContext;
		ServerSettings serverSettings;
		std::unique_ptr<DefaultHTTPService> httpService = std::make_unique<DefaultHTTPService>();
		SetHeadPoseFn setHeadPose=nullptr;
		SetControllerPoseFn setControllerPose = nullptr;
		ProcessNewInputStateFn processNewInputState = nullptr;
		ProcessNewInputEventsFn processNewInputEvents = nullptr;
		DisconnectFn onDisconnect = nullptr;
		ProcessAudioInputFn processAudioInput = nullptr;
		GetUnixTimestampFn getUnixTimestampNs = nullptr;
		ReportHandshakeFn reportHandshake = nullptr;
		uint32_t connectionTimeout = 60000;
		ServerSettings TELEPORT_SERVER_API &GetServerSettings()
		{
			return serverSettings;
		}
	}
}

using namespace teleport;
using namespace server;
bool teleport::server::ApplyInitializationSettings(const InitializationSettings *initializationSettings)
{
	GeometryStreamingService::callback_clientStoppedRenderingNode = initializationSettings->clientStoppedRenderingNode;
	GeometryStreamingService::callback_clientStartedRenderingNode = initializationSettings->clientStartedRenderingNode;
	setHeadPose = initializationSettings->headPoseSetter;
	processNewInputState = initializationSettings->newInputStateProcessing;
	processNewInputEvents = initializationSettings->newInputEventsProcessing;
	setControllerPose = initializationSettings->controllerPoseSetter;
	onDisconnect = initializationSettings->disconnect;
	processAudioInput = initializationSettings->processAudioInput;
	if(initializationSettings->getUnixTimestampNs)
		getUnixTimestampNs = initializationSettings->getUnixTimestampNs;
	else
		getUnixTimestampNs = &teleport::core::GetUnixTimeNs;
	processAudioInput = initializationSettings->processAudioInput;
	reportHandshake = initializationSettings->reportHandshake;

	if (!initializationSettings->signalingPorts)
	{
		TELEPORT_CERR << "Failed to identify ports as string was null.";
		return false;
	}
	std::string str(initializationSettings->signalingPorts);
	std::string::size_type pos_begin = {0}, pos_end = {0};
	std::set<uint16_t> ports;
	do
	{
		pos_end = str.find_first_of(",", pos_begin);
		std::string str2 = str.substr(pos_begin, pos_end - pos_begin);
		uint16_t p = std::stoi(str2);
		ports.insert(p);
		pos_begin = pos_end + 1;
	} while (str.find_first_of(",", pos_end) != std::string::npos);
	if (!ports.size())
	{
		TELEPORT_CERR << "Failed to identify ports from string " << initializationSettings->signalingPorts << "!\n";
		return false;
	}

	bool result = ClientManager::instance().initialize(ports, initializationSettings->start_unix_time_us, std::string(initializationSettings->clientIP));

	if (!result)
	{
		TELEPORT_CERR << "An error occurred while attempting to initalise clientManager!\n";
		return false;
	}

	result = httpService->initialize(initializationSettings->httpMountDirectory, initializationSettings->certDirectory, initializationSettings->privateKeyDirectory, 80);
	return true;
}
static OutputLogFn outputLogFn;
static THREAD_TYPE logging_thread=0;
void outp(const char *txt)
{
	static std::string out_str;
	out_str += txt;
	// if main thread, output. If not, accumulate
	if (GetThreadId() == logging_thread)
	{
		if(outputLogFn)
			outputLogFn(2, out_str.c_str());
		out_str.resize(0);
	}
}
void errf(const char *txt)
{
	static std::string err_str;
	err_str+=txt;
	if(GetThreadId()==logging_thread)
	{
		if (outputLogFn)
			outputLogFn(1, err_str.c_str());
		err_str.resize(0);
	}
}

void teleport::server::SetOutputLogCallback(OutputLogFn fn) 
{
	// ensure debug_buffer exists.
	ClientManager::instance();
	logging_thread=GetThreadId();
	outputLogFn = fn;
	debug_buffer->setOutputCallback(outp);
	debug_buffer->setErrorCallback(errf);
}


std::shared_ptr<ClientManager> clientManagerInstance;

ClientManager &ClientManager::instance()
{
	if(!clientManagerInstance)
		clientManagerInstance=std::make_shared<ClientManager>();
	return *(clientManagerInstance.get());
}

ClientManager::ClientManager()
{
	mLastTickTimestamp = avs::Platform::getTimestamp();
	if(!debug_buffer)	
		debug_buffer=std::make_shared<VisualStudioDebugOutput>(true, "teleport_server.log", 128);
}

ClientManager::~ClientManager()
{
	
}

avs::uid ClientManager::popFirstUnlinkedClientUid()
{
	if (unlinkedClientIDs.size() != 0)
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

bool ClientManager::initialize(std::set<uint16_t> signalPorts, int64_t start_unix_time_us, std::string client_ip_match, uint32_t maxClients)
{
	if (mInitialized)
	{
		return false;
	}
	TELEPORT_PROFILE_AUTOZONE;
	ClientManager::instance().unlinkedClientIDs.clear();
	if(!start_unix_time_us)
		start_unix_time_us=teleport::core::GetUnixTimeUs();
	startTimestamp_utc_unix_us = start_unix_time_us;
	// session id should be a random large hash.
	// generate a unique session id.
	// 
	static std::mt19937_64 m_mt;
	std::uniform_int_distribution<uint64_t> distro;
	m_mt.seed( std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
	sessionState.sessionId = distro.operator()(m_mt);
	if(!signalingService.initialize(signalPorts, client_ip_match))
	{
		TELEPORT_CERR << "An error occurred while attempting to initalise signalingService!\n";
		return false;
	}

	mMaxClients = maxClients;

	mInitialized = true;

	startAsyncNetworkDataProcessing();
	return true;
}

bool ClientManager::shutdown()
{
	std::lock_guard<std::mutex> videoLock(videoMutex);
	std::lock_guard<std::mutex> audioLock(audioMutex);

	stopAsyncNetworkDataProcessing(true);

	for (auto &uid : GetClientUids())
	{
		auto &client = GetClient(uid);
		if (!client)
			continue;
		if (client->GetConnectionState() != UNCONNECTED)
		{
			// This will add to lost clients and lost clients will be cleared below.
			// That's okay because the session is being stopped in Client_StopStreaming
			// and the clientServices map is being cleared below too.
			stopClient(uid);
		}
		else
		{
			client->clientMessaging->stopSession();
		}
	}
	if (mInitialized)
	{
		std::lock_guard<std::shared_mutex> lock(clientsMutex);
		clients.clear();

		mInitialized = false;
	}
	signalingService.shutdown();
	lostClients.clear();
	unlinkedClientIDs.clear();
	return true;
}

void ClientManager::startStreaming(avs::uid clientID)
{
	auto client = GetClient(clientID);
	if (!client)
	{
		TELEPORT_CERR << "Failed to start streaming to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	//not yet received client settings from the engine?
	if (clientSettings.find(clientID)==clientSettings.end())
	{
		TELEPORT_WARN_NOSPAM("Failed to start streaming to Client {0}. clientSettings is not found!  ",clientID);
		return;
	}

	client->StartStreaming(  connectionTimeout, sessionState.sessionId
		, getUnixTimestampNs, startTimestamp_utc_unix_us,httpService->isUsingSSL());
}

void ClientManager::tick(float deltaTime)
{
	TELEPORT_PROFILE_AUTOZONE;
	// Delete client data for clients who have been lost.
	for (avs::uid clientID : lostClients)
	{
		removeLostClient(clientID);
	}
	lostClients.clear();
	mLastTickTimestamp = avs::Platform::getTimestamp();
	{
		std::shared_lock<std::shared_mutex> lock(clientsMutex);
		for (auto& c : clients)
		{
			c.second->clientMessaging->handleEvents(deltaTime);
			std::string msg;
			if (signalingService.GetNextMessage(c.first, msg))
				c.second->clientMessaging->clientNetworkContext.NetworkPipeline.receiveStreamingControlMessage(msg);
			std::vector<uint8_t> bin;
			while (signalingService.GetNextBinaryMessage(c.first, bin))
			{
				c.second->clientMessaging->receiveSignaling(bin);
			}
			if (c.second->GetConnectionState() == DISCOVERED)
			{
				startStreaming(c.first);
			}
			c.second->tick(deltaTime);
		}
	}
	signalingService.tick();
	for (auto c : signalingService.getClientIds())
	{
		auto clientID = c;
		auto discoveryClient = signalingService.getSignalingClient(clientID);
		if (!discoveryClient)
			continue;
		if (discoveryClient->signalingState != core::SignalingState::ACCEPTED)
			continue;
		if (startSession(clientID, discoveryClient->ip_addr_port))
		{
			discoveryClient->signalingState = core::SignalingState::STREAMING;
		}
	}
}

bool ClientManager::startSession(avs::uid clientID, std::string clientIP)
{
	if (!clientID || clientIP.size() == 0)
		return false;
	{
		std::shared_lock<std::shared_mutex> lock(clientsMutex);
		if (clients.size() >= mMaxClients)
			return false;
	}
	TELEPORT_COUT << "Started session for clientID " << clientID << " at IP " << clientIP.c_str() << std::endl;
	std::lock_guard<std::mutex> videoLock(videoMutex);
	std::lock_guard<std::mutex> audioLock(audioMutex);

	//Check if we already have a session for a client with the passed ID.
	auto client = GetClient(clientID);
	if (!client)
	{
		std::shared_ptr<ClientMessaging> clientMessaging
			= std::make_shared<ClientMessaging>(signalingService, setHeadPose, setControllerPose, processNewInputState, processNewInputEvents, onDisconnect, connectionTimeout, reportHandshake,clientID);

		client = std::make_shared<ClientData>(clientID,clientMessaging);

		if (!clientMessaging->startSession(clientID, clientIP))
		{
			TELEPORT_CERR << "Failed to start session for Client " << clientID << "!\n";
			return false;
		}
		{
			std::lock_guard<std::shared_mutex> lock(clientsMutex);
			clients[clientID] = client;
			clientIDs.insert(clientID);
		}
	}
	else
	{
		if (!client->clientMessaging->isStartingSession() || client->clientMessaging->timedOutStartingSession())
		{
			client->clientMessaging->Disconnect();
			return false;
		}
		return true;
	}

	client->SetConnectionState(UNCONNECTED);
	if (client->clientMessaging->isInitialised())
	{
		client->clientMessaging->unInitialise();
	}
	client->clientMessaging->getClientNetworkContext()->Init(clientID, serverSettings.isReceivingAudio);

	///TODO: Initialize real delegates for capture component.
	CaptureDelegates delegates;
	delegates.startStreaming = [](ClientNetworkContext* context) {};
	delegates.requestKeyframe = [client]()
	{
		client->videoKeyframeRequired = true;
	};
	delegates.getClientCameraInfo = []()->CameraInfo&
	{
		static CameraInfo c;
		return c;
	};

	client->clientMessaging->initialize(delegates);

	{
		auto discoveryClient = signalingService.getSignalingClient(clientID);
		signalingService.sendResponseToClient(discoveryClient,clientID);
	}

	return true;
}

void ClientManager::stopClient(avs::uid clientID)
{
	auto client = GetClient(clientID);
	if (!client)
	{
		TELEPORT_CERR << "Failed to stop streaming to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	if (client->GetConnectionState() != UNCONNECTED)
		client->clientMessaging->stopSession();
	client->SetConnectionState(UNCONNECTED);

	// Delay deletion of clients.
	lostClients.insert(clientID);
}

void ClientManager::removeLostClient(avs::uid clientID)
{
	std::lock_guard<std::mutex> videoLock(videoMutex);
	std::lock_guard<std::mutex> audioLock(audioMutex);

	// Early-out if a client with this ID doesn't exist.
	auto client = GetClient(clientID);
	if (!client)
	{
		TELEPORT_CERR << "Failed to remove client from server! No client exists with ID " << clientID << "!\n";
		return;
	}
	client->clientMessaging->stopSession();
	clientIDs.erase(clientID);
	{
		std::lock_guard<std::shared_mutex> lock(clientsMutex);
		clients.erase(clientID);
	}
	clientSettings.erase(clientID);
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
void ClientManager::SetClientSettings(avs::uid clientID,const struct ClientSettings &c)
{
	clientSettings[clientID] = std::make_shared<ClientSettings>();
	*(clientSettings[clientID])=c;
}

bool ClientManager::hasClient(avs::uid clientID)
{
	std::shared_lock<std::shared_mutex> lock(clientsMutex);
	auto c = clients.find(clientID);
	if (c == clients.end())
		return false;
	return true;
}

std::shared_ptr<ClientData> ClientManager::GetClient(avs::uid clientID)
{
	std::shared_ptr<ClientData> client;
	std::shared_lock<std::shared_mutex> lock(clientsMutex);
	auto c = clients.find(clientID);
	if (c == clients.end())
		return client;
	client = c->second;
	return client;
}

const std::set<avs::uid> &ClientManager::GetClientUids() const
{
	return clientIDs;
}

bool ClientManager::hasHost() const
{
	return true;
}

avs::Timestamp ClientManager::getLastTickTimestamp() const
{
	return mLastTickTimestamp;
}

void ClientManager::startAsyncNetworkDataProcessing()
{
	if (mInitialized && !mAsyncNetworkDataProcessingActive)
	{
		mAsyncNetworkDataProcessingActive = true;
		if (!mNetworkThread.joinable())
		{
			mLastTickTimestamp = avs::Platform::getTimestamp();
			mNetworkThread = std::thread(&ClientManager::processNetworkDataAsync, this);
		}
	}
}

void ClientManager::stopAsyncNetworkDataProcessing(bool killThread)
{
	if (mAsyncNetworkDataProcessingActive)
	{
		mAsyncNetworkDataProcessingActive = false;
		if (killThread && mNetworkThread.joinable())
		{
			mNetworkThread.join();
		}
	}
	else if (mNetworkThread.joinable())
	{
		mNetworkThread.join();
	}
}

void ClientManager::InitializeVideoEncoder(avs::uid clientID, VideoEncodeParams &videoEncodeParams)
{
	std::lock_guard<std::mutex> lock(videoMutex);

	auto client = GetClient(clientID);
	if (!client)
	{
		TELEPORT_CERR << "Failed to initialise video encoder for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	avs::Queue *cq = &client->clientMessaging->getClientNetworkContext()->NetworkPipeline.ColorQueue;
	avs::Queue *tq = &client->clientMessaging->getClientNetworkContext()->NetworkPipeline.TagDataQueue;
	Result result = client->videoEncodePipeline->configure(serverSettings, videoEncodeParams, cq, tq);
	if (!result)
	{
		TELEPORT_CERR << "Failed to initialise video encoder for Client " << clientID << "! Error occurred when trying to configure the video encoder pipeline!\n";
		client->clientMessaging->video_encoder_initialized = false;
	}
	else
		client->clientMessaging->video_encoder_initialized = true;
}

void ClientManager::ReconfigureVideoEncoder(avs::uid clientID, VideoEncodeParams &videoEncodeParams)
{
	std::lock_guard<std::mutex> lock(videoMutex);

	auto client = GetClient(clientID);
	if (!client)
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

	/// TODO: Need to retrieve encoder settings from engine.
	CasterEncoderSettings encoderSettings{
		videoEncodeParams.encodeWidth,
		videoEncodeParams.encodeHeight,
		0, // not used
		0, // not used
		false,
		true,
		true,
		10000,
		0, 0, 0, 0};
	core::ReconfigureVideoCommand cmd;
	avs::VideoConfig &videoConfig = cmd.video_config;
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

void ClientManager::EncodeVideoFrame(avs::uid clientID, const uint8_t *tagData, size_t tagDataSize)
{
	std::lock_guard<std::mutex> lock(videoMutex);

	auto client = GetClient(clientID);
	if (!client)
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
	if (result)
	{
		client->videoKeyframeRequired = false;
	}
	else
	{
		TELEPORT_CERR << "Failed to encode video frame for Client " << clientID << "! Error occurred when trying to encode video!\n";

		// repeat the attempt for debugging purposes.
		result = client->videoEncodePipeline->encode(tagData, tagDataSize, client->videoKeyframeRequired);
		if (result)
		{
			client->videoKeyframeRequired = false;
		}
	}
}

void ClientManager::SendAudio(const uint8_t *data, size_t dataSize)
{
	// Only continue processing if the main thread hasn't hung.
	double elapsedTime = avs::Platform::getTimeElapsedInSeconds(ClientManager::instance().getLastTickTimestamp(), avs::Platform::getTimestamp());
	if (elapsedTime > 0.15f)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(audioMutex);

	for (avs::uid clientID : GetClientUids())
	{
		auto client = GetClient(clientID);
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


void ClientManager::processNetworkDataAsync()
{
	SetThisThreadName("TeleportServer_processNetworkDataAsync");
	mAsyncNetworkDataProcessingFailed = false;
	// Elapsed time since the main thread last ticked (seconds).
	avs::Timestamp timestamp;
	double elapsedTime;
	while (mAsyncNetworkDataProcessingActive)
	{
		// Only continue processing if the main thread hasn't hung.
		timestamp = avs::Platform::getTimestamp();
		{
			//std::lock_guard<std::mutex> lock(mDataMutex);
			elapsedTime = avs::Platform::getTimeElapsedInSeconds(mLastTickTimestamp, timestamp);
		}
		handleStoppedClients();
		// Proceed only if the main thread hasn't hung.
		if (elapsedTime < 1.0)
		{
			receiveMessages();
			handleStreaming();
		}
		std::this_thread::yield();
	}
}
void ClientManager::handleStoppedClients()
{
	std::set<avs::uid> remove_uids;
	{
		std::shared_lock<std::shared_mutex> lock(clientsMutex);
		for (auto c : clients)
		{
			if (c.second->clientMessaging->isStopped())
			{
				remove_uids.insert(c.first);
			}
		}
	}
	for (auto u : remove_uids)
	{
		removeLostClient(u);
	}
}
void ClientManager::receiveMessages()
{
}

void ClientManager::handleStreaming()
{
	TELEPORT_PROFILE_AUTOZONE;
	if (!clientsMutex.try_lock())
		return;
	for (auto &c : clients)
	{
		auto& client = c.second;
		if (client->clientMessaging->receivedHandshake)
		{
			if (!client->clientMessaging->clientNetworkContext.NetworkPipeline.process())
			{
				mAsyncNetworkDataProcessingFailed = true;
			}
		}
	}
	clientsMutex.unlock();
}
