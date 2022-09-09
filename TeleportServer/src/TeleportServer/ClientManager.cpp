#include "ClientManager.h"

#include "ClientMessaging.h"

#include <algorithm>
#include <iostream>

#include "enet/enet.h"
#include "libavstream/common_input.h"
#include "libavstream/common_networking.h"


#include "DiscoveryService.h"
#include "TeleportCore/ErrorHandling.h"

namespace teleport
{
	ClientManager::ClientManager()
		: mHost(nullptr)
		, mAsyncNetworkDataProcessingActive(false)
		, mAsyncNetworkDataProcessingFailed(false)
		, mInitialized(false)
		, mListenPort(0)
		, mMaxClients(0)
	{
		mLastTickTimestamp = avs::PlatformWindows::getTimestamp();
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
			mHost = enet_host_create(&ListenAddress, maxClients, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_NumChannels), 0, 0);
		if (!mHost)
		{
			ListenAddress.port ++;
			mHost = enet_host_create(&ListenAddress, maxClients, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_NumChannels), 0, 0);
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
		std::lock_guard<std::mutex> guard(mDataMutex);
		mLastTickTimestamp = avs::PlatformWindows::getTimestamp();
	}

	void ClientManager::addClient(ClientMessaging* client)
	{
		if (mClients.size() == mMaxClients)
		{
			return;
		}

		std::lock_guard<std::mutex> lock(mNetworkMutex);
		
		uint16_t port = mHost->address.port + 2;
		for (int i = 0; i < mPorts.size(); ++i)
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
		std::lock_guard<std::mutex> lock(mNetworkMutex);
		for (int i = 0; i < mClients.size(); ++i)
		{
			if (mClients[i]->clientID == client->clientID)
			{
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
		assert(host);

		return mHost->address.port;
	}

	avs::Timestamp ClientManager::getLastTickTimestamp() const
	{
		std::lock_guard<std::mutex> guard(mDataMutex);
		return mLastTickTimestamp;
	}

	void ClientManager::startAsyncNetworkDataProcessing()
	{
		if (mInitialized && !mAsyncNetworkDataProcessingActive)
		{
			mAsyncNetworkDataProcessingActive = true;
			if (!mNetworkThread.joinable())
			{
				mLastTickTimestamp = avs::PlatformWindows::getTimestamp();
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
		mAsyncNetworkDataProcessingFailed = false;
		// Elapsed time since the main thread last ticked (seconds).
		avs::Timestamp timestamp;
		double elapsedTime;
		while (mAsyncNetworkDataProcessingActive)
		{
			// Only continue processing if the main thread hasn't hung.
			timestamp = avs::PlatformWindows::getTimestamp();
			{
				std::lock_guard<std::mutex> lock(mDataMutex);
				elapsedTime = avs::PlatformWindows::getTimeElapsedInSeconds(mLastTickTimestamp, timestamp);
			}

			// Proceed only if the main thread hasn't hung.
			if (elapsedTime < 1.0)
			{
				std::lock_guard<std::mutex> lock(mNetworkMutex);
				handleMessages();
				handleStreaming();
			}
		}
	}

	void ClientManager::handleMessages()
	{
		ENetEvent event;
		try
		{
		// TODO: Can hang in enet_host_service. Why?
			while (enet_host_service(mHost, &event, 0) > 0)
			{
				if (event.type != ENET_EVENT_TYPE_NONE)
				{
					for (auto client : mClients)
					{
						if(event.peer==client->peer||(event.type==ENET_EVENT_TYPE_CONNECT&&client->peer==nullptr))
						{
							char peerIP[20];
							enet_address_get_host_ip(&event.peer->address, peerIP, sizeof(peerIP));
							// Was the message from this client?
							if (client->clientIP == std::string(peerIP))
							{
								client->eventQueue.push(event);
								break;
							}
						}
					}
				}
			}
		}
		catch (...)
		{
		}
	}

	void ClientManager::handleStreaming()
	{
		for (auto client : mClients)
		{
			if (client->receivedHandshake)
			{
				if (!client->casterContext->NetworkPipeline->process())
				{
					mAsyncNetworkDataProcessingFailed = true;
				}
			}
		}
	}
}

