// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#include <iostream>
#include "libavstream/network/webrtc_networksource.h"
#include <ElasticFrameProtocol.h>
#include <libavstream/queue.hpp>
#include <libavstream/timer.hpp>

#ifdef __ANDROID__
#include <pthread.h>
#include <sys/prctl.h>
#endif

#include <util/srtutil.h>
#include <api/scoped_refptr.h>
#include <api/peer_connection_interface.h>
#include <api/create_peerconnection_factory.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <media/engine/multiplex_codec_factory.h>
#include <media/engine/internal_encoder_factory.h>
#include <media/engine/internal_decoder_factory.h>
#include "network/webrtc_common.h"
#include <functional>

//rtc
#include <rtc_base/physical_socket_server.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/thread.h>
#include "TeleportCore/ErrorHandling.h"
#if IS_CLIENT
#include <libavstream/httputil.hpp>
#endif
#include <ios>
#include "network/webrtc_observers.h"

#pragma optimize("",off)
namespace avs
{
	struct SourceDataChannel
	{
		SourceDataChannel(uint64_t stream_index = 0, WebRtcNetworkSource* webRtcNetworkSink = nullptr)
		{
		}
		~SourceDataChannel()
		{}
		class DataChannelObserver* data_channel_observer = nullptr;
		rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_interface;
	};
	struct WebRtcNetworkSource::Private final : public PipelineNode::Private, public webrtc::PeerConnectionObserver
	{
		AVSTREAM_PRIVATEINTERFACE(WebRtcNetworkSource, PipelineNode)
		webrtc::PeerConnectionInterface::RTCConfiguration rtcConfiguration;
		webrtc::DataChannelInit data_channel_config;
		std::unordered_map<uint64_t, SourceDataChannel> dataChannels;
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection;
		// webrtc::PeerConnectionObserver.
		void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState /* new_state */);
		void OnAddStream(webrtc::MediaStreamInterface* /* stream */);
		void OnRemoveStream(webrtc::MediaStreamInterface* /* stream */);
		void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);
		void OnRenegotiationNeeded();
		void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState /* new_state */);
		void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState /* new_state */);
		void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
	};
}

using namespace avs;


WebRtcNetworkSource::WebRtcNetworkSource()
	: NetworkSource(new WebRtcNetworkSource::Private(this))
{
	m_data = static_cast<WebRtcNetworkSource::Private*>(m_d);
	// ensure that the peer connection factory exists.
	CreatePeerConnectionFactory();
	create_session_description_observer = (new CreateSessionDescriptionObserver(
		std::bind(&WebRtcNetworkSource::OnAnswerCreated, this, std::placeholders::_1)
	));
	set_session_description_observer = (new SetSessionDescriptionObserver());
}

Result WebRtcNetworkSource::configure(std::vector<NetworkSourceStream>&& streams, const NetworkSourceParams& params)
{
	size_t numOutputs = streams.size();

	if (numOutputs == 0 || params.remotePort == 0 || params.remoteHTTPPort == 0)
	{
		return Result::Node_InvalidConfiguration;
	}
	if (!params.remoteIP || !params.remoteIP[0])
	{
		return Result::Node_InvalidConfiguration;
	}
	try
	{
	}
	catch (const std::exception& e)
	{
		AVSLOG(Error) << "WebRtcNetworkSource: Failed to configure: " << e.what();
		return Result::Network_BindFailed;
	}
	m_params = params;
	webrtc::PeerConnectionInterface::IceServer ice_server;
	ice_server.uri = "stun:stun.l.google.com:19302";
	// Just DECLARING this on the stack crashes the program. Unless webrtc.lib is recompiled with ABSL_OPTION_USE_STD_OPTIONAL=0
	// to force it to use the absl::optional implementation globally, instead of sometimes treating absl::optional as absl::optional
	// and sometimes treating it as std::optional.
	webrtc::PeerConnectionInterface::RTCConfiguration rtcConfiguration;
	rtcConfiguration.servers.push_back(ice_server);

	return Result::OK;
	// bind *this* as the peer_connection_observer.
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> ptr(g_peer_connection_factory->CreatePeerConnection(rtcConfiguration,webrtc::PeerConnectionDependencies(m_data)));
	m_data->peer_connection = ptr;
	m_data->data_channel_config.ordered = false;
	m_data->data_channel_config.maxRetransmits = 0;
	m_data->dataChannels.clear();
	for (size_t i = 0; i < streams.size(); i++)
	{
		assert(i == streams[i].id);
		m_data->dataChannels.emplace(streams[i].id, SourceDataChannel(streams[i].id, this));
		SourceDataChannel& d = m_data->dataChannels[streams[i].id];
		d.data_channel_interface = m_data->peer_connection->CreateDataChannel("dc", &m_data->data_channel_config);
		d.data_channel_interface->RegisterObserver(d.data_channel_observer);
	}
	webrtc::SdpParseError error;
	webrtc::PeerConnectionInterface::RTCOfferAnswerOptions o(0, 0, false, true, true);
	m_data->peer_connection->CreateOffer(create_session_description_observer, o);

	setNumOutputSlots(numOutputs);

	m_streams = std::move(streams);

	for (size_t i = 0; i < numOutputs; ++i)
	{
		const auto& stream = m_streams[i];
		m_streamNodeMap[stream.id] = i;
	}



#if IS_CLIENT
	HTTPUtilConfig httpUtilConfig;
	httpUtilConfig.remoteIP = params.remoteIP;
	httpUtilConfig.remoteHTTPPort = params.remoteHTTPPort;
	httpUtilConfig.maxConnections = params.maxHTTPConnections;
	httpUtilConfig.useSSL = params.useSSL;
	auto f = std::bind(&WebRtcNetworkSource::receiveHTTPFile, this, std::placeholders::_1, std::placeholders::_2);
	return m_httpUtil.initialize(httpUtilConfig, std::move(f));
#else
	return Result::OK;
#endif
}

void WebRtcNetworkSource::receiveHTTPFile(const char* buffer, size_t bufferSize)
{
	int nodeIndex = m_streamNodeMap[m_params.httpStreamID];

	auto outputNode = dynamic_cast<Queue*>(getOutput(nodeIndex));
	if (!outputNode)
	{
		AVSLOG(Warning) << "WebRtcNetworkSource HTTP Callback: Invalid output node. Should be an avs::Queue.";
		return;
	}

	size_t numBytesWrittenToOutput;
	auto result = outputNode->write(this, buffer, bufferSize, numBytesWrittenToOutput);

	if (!result)
	{
		AVSLOG(Warning) << "WebRtcNetworkSource HTTP Callback: Failed to write to output node.";
		return;
	}

	if (numBytesWrittenToOutput < bufferSize)
	{
		AVSLOG(Warning) << "WebRtcNetworkSource HTTP Callback: Incomplete payload written to output node.";
		return;
	}

	{
		std::lock_guard<std::mutex> guard(m_dataMutex);
		m_counters.httpFilesReceived++;
	}
}

Result WebRtcNetworkSource::deconfigure()
{
	if (getNumOutputSlots() <= 0)
	{
		return Result::Node_NotConfigured;
	}
	setNumOutputSlots(0);

	m_counters = {};
	m_streamNodeMap.clear();
	m_streams.clear();

#if IS_CLIENT
	return m_httpUtil.shutdown();
#else
	return Result::OK;
#endif
}

Result WebRtcNetworkSource::process(uint64_t timestamp, uint64_t deltaTime)
{
	if (getNumOutputSlots() == 0 )
	{
		return Result::Node_NotConfigured;
	}

#if IS_CLIENT
	return m_httpUtil.process();
#else
	return Result::OK;
#endif
}

NetworkSourceCounters WebRtcNetworkSource::getCounterValues() const
{
	return m_counters;
}

#ifndef _MSC_VER
__attribute__((optnone))
#endif

#if IS_CLIENT
std::queue<HTTPPayloadRequest>& WebRtcNetworkSource::GetHTTPRequestQueue()
{
	return m_httpUtil.GetRequestQueue();
}
#endif

//webrtc::PeerConnectionObserver
// *****************************

void WebRtcNetworkSource::Private::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state)
{
}

void WebRtcNetworkSource::Private::OnAddStream(webrtc::MediaStreamInterface* stream)
{
}

void WebRtcNetworkSource::Private::OnRemoveStream(webrtc::MediaStreamInterface* stream)
{
}

void WebRtcNetworkSource::Private::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
{
}

void WebRtcNetworkSource::Private::OnRenegotiationNeeded()
{
}

void WebRtcNetworkSource::Private::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state)
{
}

void WebRtcNetworkSource::Private::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state)
{
}

void WebRtcNetworkSource::Private::OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
{
}
//*********************************
void WebRtcNetworkSource::OnAnswerCreated(webrtc::SessionDescriptionInterface* desc)
{
}