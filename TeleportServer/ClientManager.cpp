#include "ClientManager.h"

#include "ClientMessaging.h"

#include <algorithm>
#include <iostream>

#include "enet/enet.h"
#include "libavstream/common_input.h"
#include "TeleportCore/CommonNetworking.h"


#include "DiscoveryService.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Threads.h"

using namespace teleport;
using namespace server;


	ClientManager::ClientManager()
	{
		mLastTickTimestamp = avs::Platform::getTimestamp();
	}

	ClientManager::~ClientManager()
	{
		
	}

	bool ClientManager::initialize(int32_t listenPort, uint32_t maxClients)
	{
		if (mInitialized)
		{
			return false;
		}

		ENetAddress ListenAddress = {};
		ListenAddress.host = ENET_HOST_ANY;
		ListenAddress.port = listenPort;

		// ServerHost will live for the lifetime of the session.
		if(!mHost)
			mHost = enet_host_create(&ListenAddress, maxClients, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_NumChannels), 0, 0);
		if (!mHost)
		{
			ListenAddress.port ++;
			mHost = enet_host_create(&ListenAddress, maxClients, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_NumChannels), 0, 0);
			if(mHost)
			{
				std::cerr << "Error: port "<<listenPort<<" is in use.\n";
				enet_host_destroy(mHost);
				mHost = nullptr;
			}
			std::cerr << "Session: Failed to create ENET server host!\n";
			DEBUG_BREAK_ONCE;
			return false;
		}
		
		mPorts.resize(maxClients);
		std::fill(mPorts.begin(), mPorts.end(), false);

		mListenPort = listenPort;
		mMaxClients = maxClients;

		mInitialized = true;

		return true;
	}

	bool ClientManager::shutdown()
	{
		if (!mInitialized)
		{
			return false;
		}

		if (mHost)
		{
			enet_host_destroy(mHost);
			mHost = nullptr;
		}

		mClients.clear();

		mInitialized = false;

		return true;
	}

	void ClientManager::tick(float deltaTime)
	{
		mLastTickTimestamp = avs::Platform::getTimestamp();
	}

	void ClientManager::addClient(ClientMessaging* client)
	{
		if (mClients.size() == mMaxClients)
		{
			return;
		}

		std::lock_guard<std::mutex> lock(mNetworkMutex);
		
		uint16_t port = mHost->address.port + 2;
		for (int i = 0; i < (int)mPorts.size(); ++i)
		{
			if (!mPorts[i])
			{
				client->streamingPort = port + i;
				mPorts[i] = true;
				break;
			}
		}
		mClients.push_back(client);
	}

	void ClientManager::removeClient(ClientMessaging* client)
	{
		for (int i = 0; i < (int)mClients.size(); ++i)
		{
			if (mClients[i]->clientID == client->clientID)
			{
				if (mClients[i]->peer)
				{
					TELEPORT_COUT << "Stopping session." << std::endl;
					enet_peer_reset(mClients[i]->peer);
					mClients[i]->peer = nullptr;
				}
				mClients.erase(mClients.begin() + i);
				int index = client->streamingPort - (mHost->address.port + 2);
				if (index >= 0)
				{
					mPorts[index] = false;
					client->streamingPort = 0;
				}
				break;
			}
		}
	}

	bool ClientManager::hasClient(avs::uid clientID)
	{
		for (auto client : mClients)
		{
			if (client->clientID == clientID)
			{
				return true;
			}
		}
		return false;
	}

	bool ClientManager::hasHost() const
	{
		return mHost;
	}

	uint16_t ClientManager::getServerPort() const
	{
		assert(mHost);

		return mHost->address.port;
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
		for (auto client : mClients)
		{
			if (client->isStopped())
			{
				removeClient(client);
			}
		}
	}
	void ClientManager::receiveMessages()
	{
		ENetEvent event;
		try
		{
		// TODO: Can hang in enet_host_service. Why?
			int res = 0;
			do
			{
				res = enet_host_service(mHost, &event, 0);
				if(res>0)
				if (event.type != ENET_EVENT_TYPE_NONE)
				{
					std::lock_guard<std::mutex> lock(mNetworkMutex);
					for (auto client : mClients)
					{
						if(event.peer==client->peer||(event.type==ENET_EVENT_TYPE_CONNECT&&client->peer==nullptr))
						{
							char peerIP[20];
							enet_address_get_host_ip(&event.peer->address, peerIP, sizeof(peerIP));
							// Was the message from this client?
							if (client->clientIP == std::string(peerIP))
							{
								// thread-safe queue
								client->eventQueue.push(event);
								break;
							}
						}
					}
				}
				if (res < 0)
				{
#ifdef _MSC_VER
					int err = WSAGetLastError();
					TELEPORT_CERR << "enet_host_service failed with error " << err << "\n";
#endif
				}
			} while (res > 0);
		}
		catch (...)
		{
		}
	}

	void ClientManager::handleStreaming()
	{
		std::lock_guard<std::mutex> lock(mNetworkMutex);
		for (auto client : mClients)
		{
			if (client->receivedHandshake)
			{
				if (!client->clientNetworkContext.NetworkPipeline.process())
				{
					mAsyncNetworkDataProcessingFailed = true;
				}
			}
		}
	}
