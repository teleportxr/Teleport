// libavstream
// (c) Copyright 2018-2023 Simul Software Ltd

#include "network/webrtc_networksink.h"
#include "network/webrtc_observers.h"
#include "network/packetformat.hpp"

#include <util/srtutil.h>

#include <iostream>
#include <cmath>
#include <api/peer_connection_interface.h>
#include "network/webrtc_common.h"
#include <functional>

//rtc
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/thread.h>
#include <ios>
#include <nlohmann/json.hpp>

#include "network/webrtc_observers.h"

#pragma optimize("",off)
using namespace std;
using nlohmann::json;
namespace avs
{
	// Unused mostly.
	struct WebRtcNetworkSink::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(WebRtcNetworkSink, PipelineNode)
	};
	struct WebRtcNetworkInternal
	{
		webrtc::PeerConnectionInterface::RTCConfiguration rtcConfiguration;
		webrtc::DataChannelInit data_channel_config;
	};
}
using namespace avs;

DataChannel::DataChannel(uint64_t stream_index, WebRtcNetworkSink *webRtcNetworkSink)
{
	data_channel_observer = (new DataChannelObserver(
		std::bind(&WebRtcNetworkSink::OnDataChannelMessage, webRtcNetworkSink, stream_index,std::placeholders::_1)));
}

DataChannel::~DataChannel()
{
	delete data_channel_observer;
}

WebRtcNetworkSink::WebRtcNetworkSink()
	: NetworkSink(new WebRtcNetworkSink::Private(this))
{
	webrtc::DataChannelInit data_channel_config;
	priv = new WebRtcNetworkInternal;
	// ensure that the peer connection factory exists.
	CreatePeerConnectionFactory();
	peer_connection_observer=(new PeerConnectionObserver(
		std::bind(&WebRtcNetworkSink::OnDataChannelCreated,this,std::placeholders::_1)
		,std::bind(&WebRtcNetworkSink::OnIceCandidate, this, std::placeholders::_1)
	));
	create_session_description_observer=(new CreateSessionDescriptionObserver(
		std::bind(&WebRtcNetworkSink::OnSessionDescriptionCreated, this, std::placeholders::_1)
		));
	set_session_description_observer=(new SetSessionDescriptionObserver());
}

// configure() is called when we have agreed to connect with a specific client.
Result WebRtcNetworkSink::configure(std::vector<NetworkSinkStream>&& streams, const char* local_bind, uint16_t localPort, const char* remote, uint16_t remotePort, const NetworkSinkParams& params)
{
	size_t numInputs = streams.size();
	if (numInputs == 0 || localPort == 0 || remotePort == 0)
	{
		return Result::Node_InvalidConfiguration;
	}
	if (!remote || !remote[0])
	{
		return Result::Node_InvalidConfiguration;
	}
	if (numInputs > (size_t)PacketFormat::MaxNumStreams)
	{
		return Result::Node_InvalidConfiguration;
	}

	webrtc::PeerConnectionInterface::IceServer ice_server;
	ice_server.uri = "stun:stun.l.google.com:19302";
	// Just DECLARING this on the stack crashes the program. Unless webrtc.lib is recompiled with ABSL_OPTION_USE_STD_OPTIONAL=0
	// to force it to use the absl::optional implementation globally, instead of sometimes treating absl::optional as absl::optional
	// and sometimes treating it as std::optional.
	webrtc::PeerConnectionInterface::RTCConfiguration rtcConfiguration;
	// should use this:?
//rtcConfiguration.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
	// DTLS SRTP has to be disabled for loopback to work.
//rtcConfiguration.enable_dtls_srtp = false;
	rtcConfiguration.servers.push_back(ice_server);
	
	peer_connection = g_peer_connection_factory->CreatePeerConnection(rtcConfiguration, nullptr, nullptr, peer_connection_observer);
	//peer_connection = ptr;
	priv->data_channel_config.ordered = false;
	priv->data_channel_config.maxRetransmits = 0;
	dataChannels.clear();
	for (size_t i = 0; i < streams.size(); i++)
	{
		assert(i == streams[i].id);
		dataChannels.try_emplace(streams[i].id, streams[i].id, this);
		DataChannel& dataChannel = dataChannels[streams[i].id];
		webrtc::DataChannelInit data_channel_config;
		data_channel_config.ordered = false;
		data_channel_config.maxRetransmits = 0;
		dataChannel.data_channel_interface = peer_connection->CreateDataChannel("dc", &data_channel_config);
		if(dataChannel.data_channel_interface)
			dataChannel.data_channel_interface->RegisterObserver(dataChannel.data_channel_observer);
	}
	webrtc::SdpParseError error;
	//webrtc::SessionDescriptionInterface* session_description(webrtc::CreateSessionDescription("offer", sdp, &error));

	webrtc::PeerConnectionInterface::RTCOfferAnswerOptions rtcOfferAnswerOptions;// (0, 0, false, true, true);

	// Create the "Offer". Ask STUN for "candidates" representing our network status, and create
	// an "offer" object to send to the client.
	// At this point, we need have no idea where the client is, what its address is etc.
	peer_connection->CreateOffer(create_session_description_observer, rtcOfferAnswerOptions);
	
	//peer_connection->SetRemoteDescription(set_session_description_observer.get(), session_description);
	//peer_connection->CreateAnswer(create_session_description_observer.get(), ebrtc::PeerConnectionInterface::RTCOfferAnswerOptions());

	setNumInputSlots(numInputs);

	m_streams = std::move(streams);

	// Having completed the above, we are not yet ready to actually send data.
	// We await the "offer" from webrtc locally and the "candidates" from the STUN server.
	// Then we must send these using the messaging channel (Websockets or Enet) to the client.
	return Result::OK;
}

WebRtcNetworkSink::~WebRtcNetworkSink()
{
	delete peer_connection_observer;
	delete create_session_description_observer;
	delete set_session_description_observer;
	deconfigure();
	delete priv;
}

NetworkSinkCounters WebRtcNetworkSink::getCounters() const
{
	std::lock_guard<std::mutex> lock(m_countersMutex);
	return m_counters;
}

void WebRtcNetworkSink::setProcessingEnabled(bool enable)
{
	enabled = enable;
}

bool WebRtcNetworkSink::isProcessingEnabled() const
{
	return enabled ;
}

Result WebRtcNetworkSink::deconfigure()
{
	peer_connection = nullptr;
	m_parsers.clear();
	m_streams.clear();

	if (getNumInputSlots() <= 0)
		return Result::OK;
	setNumInputSlots(0);


	return Result::OK;
}

Result WebRtcNetworkSink::process(uint64_t timestamp, uint64_t deltaTime)
{
	if (getNumInputSlots() == 0 )
	{
		return Result::Node_NotConfigured;
	}

	// Called to get the data from the input nodes
	auto readInput = [this](uint32_t inputNodeIndex, size_t& numBytesRead) -> Result
	{
		PipelineNode* node = getInput(inputNodeIndex);
		auto& stream = m_streams[inputNodeIndex];

		assert(node);
		assert(stream.buffer.size() >= stream.chunkSize);

		if (IOInterface* nodeIO = dynamic_cast<IOInterface*>(node))
		{
			size_t bufferSize = stream.buffer.size();
			Result result = nodeIO->read(this, stream.buffer.data(), bufferSize, numBytesRead);
			if (result == Result::IO_Retry)
			{
				stream.buffer.resize(bufferSize);
				result = nodeIO->read(this, stream.buffer.data(), bufferSize, numBytesRead);
			}

			numBytesRead = std::min(bufferSize, numBytesRead);

			return result;
		}
		else
		{
			assert(false);
			return Result::Node_Incompatible;
		}
	};
	for (int i = 0; i < (int)getNumInputSlots(); ++i)
	{
		const NetworkSinkStream& stream = m_streams[i];
		size_t numBytesRead = 0;
		try
		{
			Result result = readInput(i, numBytesRead);
			if (result != Result::OK)
			{
				if (result != Result::IO_Empty)
				{
					AVSLOG(Error) << "SrtEfpNetworkSink: Failed to read from input node: " << i << "\n";
					continue;
				}
			}
		}
		catch (const std::bad_alloc&)
		{
			return Result::IO_OutOfMemory;
		}
		if (numBytesRead == 0)
		{
			continue;
		}
		Result res = Result::OK;
		if (stream.useParser && m_parsers.find(i) != m_parsers.end())
		{
			res = m_parsers[i]->parse((const char*)stream.buffer.data(), numBytesRead);
		}
		else
		{
			res = packData(stream.buffer.data(), numBytesRead, i);
		}
		if (!res)
		{
			return res;
		}
	}

	return Result::OK;
}

Result WebRtcNetworkSink::packData(const uint8_t* buffer, size_t bufferSize, uint32_t inputNodeIndex)
{
	webrtc::DataBuffer db(rtc::CopyOnWriteBuffer(buffer, bufferSize),true);
	if(dataChannels[inputNodeIndex].data_channel_interface)
		dataChannels[inputNodeIndex].data_channel_interface->Send(db);
	return Result::OK;
}

void WebRtcNetworkSink::OnDataChannelCreated(webrtc::DataChannelInterface* channel)
{
	dataChannels.begin()->second.data_channel_interface= channel;
	channel->RegisterObserver(dataChannels.begin()->second.data_channel_observer);
	std::cerr << "OnDataChannelCreated\n";
	std::cerr << channel->id() << std::endl;
}

void WebRtcNetworkSink::OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
{
	std::string candidate_str;
	candidate->ToString(&candidate_str);
	std::cerr << "OnIceCandidate: " << candidate_str.c_str() << "\n";
	json message = {	{"type",		"candidate"},
						{"candidate",	candidate_str},
						{"mid",	candidate->sdp_mid()},
						{"mlineindex", candidate->sdp_mline_index()}
					};
	m_setupMessages.push_back({ message.dump() });
}

void WebRtcNetworkSink::OnDataChannelMessage(uint64_t data_stream_index,const webrtc::DataBuffer& buffer)
{
	dataChannels[data_stream_index].data_channel_interface->Send(buffer);
}

void WebRtcNetworkSink::OnSessionDescriptionCreated(webrtc::SessionDescriptionInterface* desc)
{
	std::cerr << "OnSessionDescriptionCreated\n";
	// This call sets-off the OnIceCandidate() callbacks:
	peer_connection->SetLocalDescription(set_session_description_observer, desc);
	std::string sdp;
	if (desc->ToString(&sdp))
	{
		json message = { {"type", "offer"},
							{"sdp", sdp} };
		std::cerr << sdp.c_str() << std::endl;
		m_setupMessages.push_back({ message.dump() });
	}
}

void WebRtcNetworkSink::receiveAnswer(const std::string& sdp)
{
	webrtc::SdpParseError error;
	webrtc::SessionDescriptionInterface* session_description(webrtc::CreateSessionDescription("answer", sdp, &error));
	peer_connection->SetRemoteDescription(set_session_description_observer, session_description);
}

void WebRtcNetworkSink::receiveCandidate(const std::string& candidate, const std::string& mid,int mlineindex)
{
	webrtc::SdpParseError error;
	auto candidate_object = webrtc::CreateIceCandidate(mid, mlineindex, candidate, &error);
	peer_connection->AddIceCandidate(candidate_object);
}


bool WebRtcNetworkSink::getNextStreamingControlMessage(std::string& msg)
{
	if (!m_setupMessages.size())
		return false;
	msg=m_setupMessages[0].text;
	m_setupMessages.erase(m_setupMessages.begin());
	return true;
}

void WebRtcNetworkSink::receiveStreamingControlMessage(const std::string& msg)
{
	json message = json::parse(msg);
	auto it = message.find("type");
	if (it == message.end())
		return;
	try
	{
		auto& type = it->get<std::string>();
		if (type == "answer")
		{
			auto o = message.find("sdp");
			receiveAnswer(o->get<std::string>());
		}
		else if (type == "candidate")
		{
			auto c = message.find("candidate");
			string& candidate = c->get<std::string>();
			auto m = message.find("mid");
			std::string mid;
			if (m != message.end())
				mid = m->get<std::string>();
			auto l = message.find("mlineindex");
			int mlineindex;
			if (l != message.end())
				mlineindex = l->get<int>();
			receiveCandidate(candidate, mid, mlineindex);
		}
	}
	catch (...)
	{
	}
}