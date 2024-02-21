#include "SignalingService.h"

#include "TeleportCore/ErrorHandling.h"
#include "TeleportServer/ClientData.h"
#include "TeleportServer/ServerSettings.h"    
#include "TeleportCore/TeleportUtility.h" 
#include "UnityPlugin/PluginMain.h"
#include "UnityPlugin/PluginClient.h"
#include <rtc/websocket.hpp>
#include <rtc/websocketserver.hpp>
#include <functional>
#include <regex>
using namespace std::string_literals;
using namespace std::string_view_literals;

using nlohmann::json;

using namespace teleport;
using namespace server;
extern ServerSettings casterSettings;
TELEPORT_EXPORT void Server_AddUnlinkedClientID(avs::uid clientID);

SignalingClient::~SignalingClient()
{
	if (webSocket)
	{
		webSocket->resetCallbacks();
		if (webSocket.use_count() > 1)
		{
			TELEPORT_COUT << ": info: Websocket " << clientID << " remains after deletion with " << webSocket.use_count() << " uses.\n";
		}
	}
	TELEPORT_COUT << ": info: ~SignalingClient " << clientID << " destroyed.\n";
}

bool SignalingService::initialize(std::set<uint16_t> discoPorts,  std::string desIP)
{
	discoveryPorts = discoPorts;
	if (discoveryPorts.empty())
	{
		discoveryPorts.insert(8080);
	}
	webSocketServers.clear();
	for(auto p: discoveryPorts)
	{
		rtc::WebSocketServer::Configuration config;
		config.port = p;
		std::shared_ptr<rtc::WebSocketServer> webSocketServer;
		try
		{
			webSocketServer = std::make_shared<rtc::WebSocketServer>(config);
		}
		catch(...)
		{
			TELEPORT_CERR<<"Failed to create WebSockets server on port "<<p<<"\n";
			continue;
		}
		webSocketServers[p] = webSocketServer;
		auto onWebSocketClient =std::bind(&SignalingService::OnWebSocket,this,std::placeholders::_1);
		webSocketServer->onClient(onWebSocketClient);
	}
	desiredIP = desIP;
	return true;
}

void SignalingService::ReceiveWebSocketsMessage(avs::uid clientID, std::string msg)
{
	//TELEPORT_CERR << "SignalingService::ReceiveWebSocketsMessage." << std::endl;
	std::lock_guard<std::mutex> lock(webSocketsMessagesMutex);
	auto &c=signalingClients[clientID];
	if(c)
		c->messagesReceived.push_back(msg);
	else
	{
		TELEPORT_CERR << ": info: Websocket message received but already removed the SignalingClient " << clientID << std::endl;
	}
}

void SignalingService::ReceiveBinaryWebSocketsMessage(avs::uid clientID, std::vector<std::byte> &bin)
{
	//TELEPORT_CERR << "SignalingService::ReceiveWebSocketsMessage." << std::endl;
	std::lock_guard<std::mutex> lock(webSocketsMessagesMutex);
	auto& c = signalingClients[clientID];
	if (c)
	{
		std::vector<uint8_t> b(bin.size());
		memcpy(b.data(), bin.data(), b.size());
		c->binaryMessagesReceived.push(b);
	}
	else
	{
		TELEPORT_CERR << "Websocket message received but already removed the SignalingClient " << clientID << std::endl;
	}
}

bool SignalingService::GetNextMessage(avs::uid clientID, std::string& msg)
{
	std::lock_guard<std::mutex> lock(webSocketsMessagesMutex);
	auto& c = signalingClients[clientID];
	if (c&&!c->messagesToPassOn.empty())
	{
		msg = c->messagesToPassOn.front();
		c->messagesToPassOn.pop();
		return true;
	}
	return false;
}
bool SignalingService::GetNextBinaryMessage(avs::uid clientID, std::vector<uint8_t>& bin)
{
	std::lock_guard<std::mutex> lock(webSocketsMessagesMutex);
	auto& c = signalingClients[clientID];
	if (c && !c->binaryMessagesReceived.empty())
	{
		bin = c->binaryMessagesReceived.front();
		c->binaryMessagesReceived.pop();
		return true;
	}
	return false;
}


void SignalingService::OnWebSocket(std::shared_ptr<rtc::WebSocket> ws)
{
	TELEPORT_CERR << "SignalingService::OnWebSocket." << std::endl;

	std::optional<std::string> addr = ws->remoteAddress();
	if (!addr.has_value())
	{
		return;
	}
	avs::uid clientID = TeleportUtility::GenerateID();
	std::string addr_string= addr.value();
	// Seems to be of the form ::ffff:192.168.3.40:59282
	// But for example, localhost would give us ::1:59297
	std::regex re("([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)(:[0-9]+)?", std::regex_constants::icase | std::regex::extended);
	std::smatch match;
	std::string ip_addr_port;
	if (std::regex_search(addr_string, match, re))
	{
		ip_addr_port = match.str(0);
	}
	else
	{
		std::regex re_local(":([0-9]+)$", std::regex_constants::icase | std::regex::extended);
		if (std::regex_search(addr_string, match, re_local))
		{
			ip_addr_port = "localhost:"s+match.str(1); //"127.0.0.1:";?
		}
		else
		{
			TELEPORT_CERR << "Websocket connection from " << addr_string << " - couldn't decode address." << std::endl;
			return;
		}
	}
	std::lock_guard lock(signalingClientsMutex);
	signalingClients[clientID] = std::make_shared<SignalingClient>();
	auto& c = signalingClients[clientID];
	c->clientID = clientID;
	c->webSocket = ws;
	c->ip_addr_port = ip_addr_port;
	c->path=ws->path().has_value()?ws->path().value():"";
	SetCallbacks(c);
	clientUids.insert(clientID);
}
void SignalingService::SetCallbacks(std::shared_ptr<SignalingClient> &signalingClient)
{
	signalingClient->webSocket->onMessage([this, signalingClient](rtc::message_variant message) {
		if (std::holds_alternative<std::string>(message))
		{
			std::string msg = std::get<std::string>(message);
			ReceiveWebSocketsMessage(signalingClient->clientID, msg);
		}
		else if (std::holds_alternative<rtc::binary>(message))
		{
			rtc::binary bin= std::get<rtc::binary>(message);
			ReceiveBinaryWebSocketsMessage(signalingClient->clientID, bin);
		}
		});
	signalingClient->webSocket->onError([this, signalingClient](std::string error)
		{
			TELEPORT_CERR << "Websocket error: " << error << std::endl;
			; });
}

void SignalingService::shutdown()
{
	TELEPORT_COUT<< ": info: SignalingService::shutdown" << std::endl;
	{
		std::lock_guard lock(signalingClientsMutex);
		for (auto c : signalingClients)
		{
			if (c.second && c.second->webSocket)
			{
				c.second->webSocket->resetCallbacks();
			}
		}
		signalingClients.clear();
	}
	for(auto &w:webSocketServers)
	{
		w.second->stop();
		w.second.reset();
	}
	webSocketServers.clear();
	clientUids.clear();
	clientRemapping.clear();
}

void SignalingService::processDisconnection(avs::uid clientID, std::shared_ptr<SignalingClient> &signalingClient)
{
	auto c = ClientManager::instance().GetClient(clientID);
	if (c)
	{
		c->SetConnectionState(UNCONNECTED);
		c->clientMessaging->Disconnect();
		// Close the Websocket, so that on reconnection, a new one creates a new signalingClient.
		signalingClient->webSocket->close();
		signalingClients.erase(clientID);
		
	}
}

void SignalingService::processInitialRequest(avs::uid uid, std::shared_ptr<SignalingClient>& signalingClient,json& content)
{
	if (content.find("clientID") != content.end())
	{
		avs::uid clientID = 0;
		auto& j_clientID = content["clientID"];
		clientID = j_clientID.get<unsigned long long>();
		if (clientID == 0)
			clientID = uid;
		else
		{
			std::lock_guard lock(signalingClientsMutex);
			if (signalingClients.find(clientID) == signalingClients.end())
			{
				// sent us a client ID that isn't valid. Ignore it, don't waste bandwidth..?
				// or instead, send the replacement ID in the response, leave it up to
				// client whether they accept the new ID or abandon the connection.

			}
			// identifies as a previous client. Discard the new client ID.
			//TODO: we're taking the client's word for it that it is clientID. Some kind of token/hash?
			signalingClients[clientID] = signalingClient;
			signalingClient->clientID = clientID;
			clientUids.insert(clientID);
			if (uid != clientID)
			{
				TELEPORT_COUT << ": info: Remapped from " << uid << " to " << clientID << std::endl;
				TELEPORT_COUT << ": info: signalingClient has " << signalingClient->clientID << std::endl;
				signalingClients[uid] = nullptr;
				clientUids.erase(uid);
				uid = clientID;
			}
		}
		std::string ipAddr;
		ipAddr = signalingClient->ip_addr_port;
		TELEPORT_COUT << "Received connection request from " << ipAddr << " identifying as client " << clientID << " .\n";

		//Skip clients we have already added.
		if (signalingClient->signalingState == core::SignalingState::START)
			signalingClient->signalingState = core::SignalingState::REQUESTED;
		// if signalingState is START, we should not have a client...
		if (ClientManager::instance().hasClient(clientID))
		{
			// ok, we've received a connection request from a client that WE think we already have.
			// Apparently the CLIENT thinks they've disconnected.
			// The client might, as far as we know, have lost the information it needs to continue the connection.
			// THerefore we should resend everything required.
			signalingClient->signalingState = core::SignalingState::STREAMING;
			TELEPORT_COUT << "Warning: Client " << clientID << " reconnected, but we didn't know we'd lost them." << std::endl;
			// It may be just that the connection request was already in flight when we accepted its predecessor.
			sendResponseToClient(clientID);
			return;
		}
		if (signalingClient->signalingState != core::SignalingState::REQUESTED)
			return;
		//Ignore connections from clients with the wrong IP, if a desired IP has been set.
		if (desiredIP.length() != 0)
		{
			//Create new wide-string with clientIP, and add new client if there is no difference between the new client's IP and the desired IP.
			if (desiredIP.compare(0, ipAddr.size(), { ipAddr.begin(), ipAddr.end() }) == 0)
			{
				signalingClient->signalingState = core::SignalingState::ACCEPTED;
			}
		}
		else
		{
			signalingClient->signalingState = core::SignalingState::ACCEPTED;
		}
	}
}

void SignalingService::tick()
{
	if (webSocketServers.size() == 0 )
	{
		TELEPORT_INTERNAL_CERR("Attempted to call tick on client discovery service without initalizing!",0);
		return;
	}

	avs::uid clientID = 0; //Newly received ID.

	//Retrieve all packets received since last call, and add any new clients.
	signalingClientsMutex.lock();
	for (auto c : signalingClients)
	{
		std::shared_ptr<SignalingClient> signalingClient = c.second;
		if (!signalingClient)
			continue;
		std::lock_guard lock(webSocketsMessagesMutex);
		signalingClientsMutex.unlock();
		while (signalingClient->messagesReceived.size())
		{
			std::string msg = signalingClient->messagesReceived[0];
			signalingClient->messagesReceived.erase(signalingClient->messagesReceived.begin());
			if (!msg.length())
				continue;
			json message = json::parse(msg);
			if (!message.contains("teleport-signal-type"))
				continue;
			std::string teleport_signal_type = message["teleport-signal-type"]; 
			if (teleport_signal_type == "request")
				processInitialRequest(c.first, signalingClient, message["content"]);
			else if (teleport_signal_type == "disconnect")
				processDisconnection(c.first, signalingClient);
			else
				signalingClient->messagesToPassOn.push(msg);
		}
		signalingClientsMutex.lock();
	}
	for (auto c : signalingClients)
	{
		std::lock_guard lock(webSocketsMessagesMutex);
		auto clientID = c.first;
		if (!c.second)
		{
			signalingClients.erase(clientID);
			clientUids.erase(clientID);
			break;
		}
	}
	signalingClientsMutex.unlock();
}

const std::set<avs::uid> &SignalingService::getClientIds() const
{
	return clientUids;
}

std::shared_ptr<SignalingClient> SignalingService::getSignalingClient(avs::uid u)
{
	std::lock_guard lock(signalingClientsMutex);
	return signalingClients[u];
}

void SignalingService::sendResponseToClient(uint64_t clientID)
{
	auto c = signalingClients.find(clientID);
	if(c == signalingClients.end())
	{
		TELEPORT_CERR << "No client with ID: " << clientID << " is trying to connect.\n";
		return;
	}
	json message = {
						{"teleport-signal-type","request-response"},
						{"content",
							{
								{"clientID", clientID}
							}
						}
	};
	try
	{
		std::lock_guard lock(signalingClientsMutex);
		signalingClients[clientID]->webSocket->send(message.dump());
		discoveryCompleteForClient(clientID);
	}
	catch (...)
	{
	}
	TELEPORT_COUT << "Sending server discovery response to client ID: " << clientID << std::endl;
}

void SignalingService::sendToClient(avs::uid clientID, std::string str)
{
	std::lock_guard lock(signalingClientsMutex);
	auto c = signalingClients.find(clientID);
	if (c == signalingClients.end())
	{
		TELEPORT_CERR << "No client with ID: " << clientID << " is trying to connect.\n";
		return;
	}
	try
	{
		signalingClients[clientID]->webSocket->send(str);
		TELEPORT_CERR << "webSocket->send: " << str << "  .\n";
	}
	catch (...)
	{
	}
}

bool SignalingService::sendBinaryToClient(avs::uid clientID, std::vector<uint8_t> bin)
{
	std::lock_guard lock(signalingClientsMutex);
	auto c = signalingClients.find(clientID);
	if (c == signalingClients.end())
	{
		TELEPORT_CERR << "No client with ID: " << clientID << " is trying to connect.\n";
		return false;
	}
	try
	{
		signalingClients[clientID]->webSocket->send((std::byte*)bin.data(),bin.size());
		TELEPORT_CERR << "webSocket->send: " << bin.size() << " binary bytes .\n";
		return true;
	}
	catch (...)
	{
		return false;
	}
}

extern std::set<avs::uid> unlinkedClientIDs; 

void SignalingService::discoveryCompleteForClient(uint64_t clientID)
{
	auto c = ClientManager::instance().GetClient(clientID);
	if (c)
	{
		c->SetConnectionState(DISCOVERED);
		unlinkedClientIDs.insert(clientID);
	}
}