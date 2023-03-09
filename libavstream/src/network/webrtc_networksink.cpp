// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd
// This prevents crashes - although defined as 2 in options.h, the lib appears to have been compiled using 0.
//#define ABSL_OPTION_USE_STD_OPTIONAL 2
//#define _HAS_CXX17 1
//#define _HAS_CXX20 0
#include "network/webrtc_networksink.h"
#include "network/webrtc_observers.h"
#include <network/packetformat.hpp>

#include <util/srtutil.h>

#include <iostream>
#include <cmath>
#include <api/peer_connection_interface.h>
#include <api/create_peerconnection_factory.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <media/engine/multiplex_codec_factory.h>
#include <media/engine/internal_encoder_factory.h>
#include <media/engine/internal_decoder_factory.h>
#include "network/webrtc_observers.h"
#include <functional>

//rtc
#include <rtc_base/physical_socket_server.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/thread.h>
#pragma optimize("",off)
using namespace avs;
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

DataChannel::DataChannel(uint64_t stream_index, WebRtcNetworkSink *webRtcNetworkSink)
{
	data_channel_observer = (new DataChannelObserver(
		std::bind(&WebRtcNetworkSink::OnDataChannelMessage, webRtcNetworkSink, stream_index,std::placeholders::_1)));
}

DataChannel::~DataChannel()
{
	delete data_channel_observer;
}

static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> g_peer_connection_factory;
// The socket that the signaling thread and worker thread communicate on.
rtc::PhysicalSocketServer socket_server;
// The separate thread where all of the WebRTC code runs
std::unique_ptr<rtc::Thread> g_worker_thread;
std::unique_ptr<rtc::Thread> g_signaling_thread;
void CreatePeerConnectionFactory()
{
	if (g_peer_connection_factory == nullptr)
	{
		rtc::LogMessage::LogToDebug(rtc::LS_INFO);
		g_worker_thread.reset(new rtc::Thread(&socket_server));
		g_worker_thread->Start();
		g_signaling_thread.reset(new rtc::Thread(&socket_server));
		g_signaling_thread->Start();
		// No audio or video decoders: for now, all streams will be treated as plain data.
		// But WebRTC requires non-null factories:
		g_peer_connection_factory = webrtc::CreatePeerConnectionFactory(
			g_worker_thread.get(), g_worker_thread.get(), g_signaling_thread.get(),
			nullptr, webrtc::CreateBuiltinAudioEncoderFactory(),
			webrtc::CreateBuiltinAudioDecoderFactory(),
			std::unique_ptr<webrtc::VideoEncoderFactory>(new webrtc::MultiplexEncoderFactory(absl::make_unique<webrtc::InternalEncoderFactory>())),
			std::unique_ptr<webrtc::VideoDecoderFactory>(new webrtc::MultiplexDecoderFactory(absl::make_unique<webrtc::InternalDecoderFactory>())),
			nullptr, nullptr);
	}
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
		std::bind(&WebRtcNetworkSink::OnAnswerCreated, this, std::placeholders::_1)
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
	rtcConfiguration.servers.push_back(ice_server);
	
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> ptr(g_peer_connection_factory->CreatePeerConnection(rtcConfiguration, webrtc::PeerConnectionDependencies(peer_connection_observer)));
	peer_connection = ptr;
	priv->data_channel_config.ordered = false;
	priv->data_channel_config.maxRetransmits = 0;
	dataChannels.clear();
	for (size_t i = 0; i < streams.size(); i++)
	{
		assert(i == streams[i].id);
		dataChannels.emplace(streams[i].id, DataChannel(streams[i].id, this));
		DataChannel& d = dataChannels[streams[i].id];
		d.data_channel_interface = peer_connection->CreateDataChannel("dc", &priv->data_channel_config);
		d.data_channel_interface->RegisterObserver(d.data_channel_observer);
	}
	webrtc::SdpParseError error;
	//webrtc::SessionDescriptionInterface* session_description(webrtc::CreateSessionDescription("offer", sdp, &error));
	//peer_connection->SetRemoteDescription(set_session_description_observer.get(), session_description);
	//peer_connection->CreateAnswer(create_session_description_observer.get(), ebrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
	//peer_connection->onicecandidate = onIceCandidate;
	webrtc::PeerConnectionInterface::RTCOfferAnswerOptions o(0, 0, false, true, true);
	peer_connection->CreateOffer(create_session_description_observer,o);

	setNumInputSlots(numInputs);

	m_streams = std::move(streams);

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
	dataChannels[inputNodeIndex].data_channel_interface->Send(db);
	return Result::OK;
}

void WebRtcNetworkSink::OnDataChannelCreated(webrtc::DataChannelInterface* channel)
{
}

void WebRtcNetworkSink::OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
{
}

void WebRtcNetworkSink::OnDataChannelMessage(uint64_t data_stream_index,const webrtc::DataBuffer& buffer)
{
}

void WebRtcNetworkSink::OnAnswerCreated(webrtc::SessionDescriptionInterface* desc)
{
}