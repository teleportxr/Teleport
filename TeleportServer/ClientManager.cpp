#include "ClientManager.h"

#include "ClientData.h"
#include "ClientMessaging.h"

#include <algorithm>
#include <iostream>

#include "enet/enet.h"
#include "libavstream/common_input.h"
#include "TeleportCore/CommonNetworking.h"


#include "SignalingService.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Threads.h"
#include "UnityPlugin/PluginClient.h"

using namespace teleport;
using namespace server;


ClientManager::ClientManager()
{
	mLastTickTimestamp = avs::Platform::getTimestamp();
}

ClientManager::~ClientManager()
{
	
}

bool ClientManager::initialize(std::set<uint16_t> signalPorts, std::string client_ip_match, uint32_t maxClients)
{
	if (mInitialized)
	{
		return false;
	}
	// session id should be a random large hash.
	// generate a unique session id.
	// 
	static std::mt19937 m_mt;
	std::uniform_int_distribution<uint64_t> distro;
	sessionID = distro.operator()(m_mt);
	if(!signalingService.initialize(signalPorts, client_ip_match))
	{
		TELEPORT_CERR << "An error occurred while attempting to initalise signalingService!\n";
		return false;
	}

	mMaxClients = maxClients;

	mInitialized = true;

	return true;
}

bool ClientManager::shutdown()
{
	if (mInitialized)
	{
		clients.clear();

		mInitialized = false;
	}
	signalingService.shutdown();
	return true;
}


void ClientManager::startStreaming(avs::uid clientID)
{
	auto client = clientManager.GetClient(clientID);
	if (!client)
	{
		TELEPORT_CERR << "Failed to start streaming to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	//not ready?
	if (!client->validClientSettings)
	{
		TELEPORT_CERR << "Failed to start streaming to Client " << clientID << ". validClientSettings is false!  " << clientID << "!\n";
		return;
	}

	client->StartStreaming(serverSettings,  connectionTimeout, sessionID, getUnixTimestamp, httpService->isUsingSSL());

}
void ClientManager::tick(float deltaTime)
{
	mLastTickTimestamp = avs::Platform::getTimestamp();

	for (auto& c : clients)
	{
		c.second->clientMessaging->handleEvents(deltaTime);
		std::string msg;
		//if (c.second->clientMessaging->clientNetworkContext.NetworkPipeline.getNextStreamingControlMessage(msg))
		{
		//	signalingService.sendToClient(c.first, msg);
		}
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
		c.second->clientMessaging->tick(deltaTime);
	}
	signalingService.tick();
	for (auto c : signalingService.getClientIds())
	{
		auto clientID = c;
		auto discoveryClient = signalingService.getSignalingClient(clientID);
		if (!discoveryClient)
			continue;
		if (discoveryClient->signalingState != SignalingState::ACCEPTED)
			continue;
		if (startSession(clientID, discoveryClient->ip_addr_port))
		{
			discoveryClient->signalingState = SignalingState::STREAMING;
		}
	}
}

bool ClientManager::startSession(avs::uid clientID, std::string clientIP)
{
	if (!clientID || clientIP.size() == 0)
		return false;
	if (clients.size() >= mMaxClients)
		return false;
	TELEPORT_COUT << "Started session for clientID " << clientID << " at IP " << clientIP.c_str() << std::endl;
	std::lock_guard<std::mutex> videoLock(videoMutex);
	std::lock_guard<std::mutex> audioLock(audioMutex);

	//Check if we already have a session for a client with the passed ID.
	auto client = GetClient(clientID);
	if (!client)
	{
		std::shared_ptr<ClientMessaging> clientMessaging
			= std::make_shared<ClientMessaging>(&serverSettings, signalingService, setHeadPose, setControllerPose, processNewInputState, processNewInputEvents, onDisconnect, connectionTimeout, reportHandshake, &clientManager);

		client = std::make_shared<ClientData>(clientMessaging);

		if (!clientMessaging->startSession(clientID, clientIP))
		{
			TELEPORT_CERR << "Failed to start session for Client " << clientID << "!\n";
			return false;
		}
		{
			std::lock_guard<std::mutex> lock(mNetworkMutex);
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

	signalingService.sendResponseToClient(clientID);

	return true;
}


void ClientManager::removeClient(avs::uid clientID)
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
	clients.erase(clientID);
}

bool ClientManager::hasClient(avs::uid clientID)
{
	auto c = clients.find(clientID);
	if (c == clients.end())
		return false;
	return true;
}

std::shared_ptr<ClientData> ClientManager::GetClient(avs::uid clientID)
{
	std::shared_ptr<ClientData> client;
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
	}
}
void ClientManager::handleStoppedClients()
{
	std::lock_guard<std::mutex> lock(mNetworkMutex);
	for (auto c : clients)
	{
		if (c.second->clientMessaging->isStopped())
		{
			removeClient(c.first);
		}
	}
}
void ClientManager::receiveMessages()
{
}

void ClientManager::handleStreaming()
{
	std::lock_guard<std::mutex> lock(mNetworkMutex);
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
}
