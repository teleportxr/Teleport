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

#include <functional>

//rtc
#if TELEPORT_DATACHANNEL_WEBRTC
#include <rtc/rtc.hpp>
#include <nlohmann/json.hpp>
#include "TeleportCore/ErrorHandling.h"
#if TELEPORT_CLIENT
#include <libavstream/httputil.hpp>
#endif
#include <ios>

FILE _iob[] = { *stdin, *stdout, *stderr };

extern "C" FILE * __cdecl __iob_func(void)
{
	return _iob;
}

#pragma optimize("",off)
using namespace std;
using nlohmann::json;

shared_ptr<rtc::PeerConnection> createPeerConnection(const rtc::Configuration& config,
	std::function<void(const std::string&)> sendMessage, std::string id)
{
	auto pc = std::make_shared<rtc::PeerConnection>(config);
	
	pc->onStateChange(
		[](rtc::PeerConnection::State state)
		{
			std::cout << "State: " << state << std::endl;
		});

	pc->onGatheringStateChange([](rtc::PeerConnection::GatheringState state)
		{
			std::cout << "Gathering State: " << state << std::endl;
		});

	pc->onLocalDescription([sendMessage,id](rtc::Description description)
		{
			// This is our answer.
			json message = { {"id", id},
						{"type", description.typeString()},
						{"sdp",  std::string(description)} };

			sendMessage(message.dump());
		});

	pc->onLocalCandidate([sendMessage, id](rtc::Candidate candidate)
		{
			json message = { {"id", id},
						{"type", "candidate"},
						{"candidate", std::string(candidate)},
						{"mid", candidate.mid()} ,
						{"mlineindex", 0} };

			sendMessage(message.dump());
		});

	pc->onDataChannel([sendMessage,id](shared_ptr<rtc::DataChannel> dc)
	{
		std::cout << "DataChannel from " << id << " received with label \"" << dc->label() << "\""<< std::endl;

		dc->onOpen([sendMessage]()
			{
				sendMessage("Hello" );
			});

	dc->onClosed([id]() { std::cout << "DataChannel from " << id << " closed" << std::endl; });

	dc->onMessage([id](auto data) {
		// data holds either std::string or rtc::binary
		if (std::holds_alternative<std::string>(data))
		std::cout << "Message from " << id << " received: " << std::get<std::string>(data)
			<< std::endl;
		else
			std::cout << "Binary message from " << id
			<< " received, size=" << std::get<rtc::binary>(data).size() << std::endl;
		});

			//dataChannelMap.emplace(id, dc);
		});

	//peerConnectionMap.emplace(id, pc);
	return pc;
};
namespace avs
{
	struct SourceDataChannel
	{
		SourceDataChannel(uint64_t stream_index = 0, WebRtcNetworkSource* webRtcNetworkSink = nullptr)
		{
		}
		~SourceDataChannel()
		{}
		shared_ptr<rtc::DataChannel> rtcDataChannel;
	};
	struct WebRtcNetworkSource::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(WebRtcNetworkSource, PipelineNode)
		std::unordered_map<uint64_t, SourceDataChannel> dataChannels;
		std::shared_ptr<rtc::PeerConnection> rtcPeerConnection;
#if TELEPORT_CLIENT
		HTTPUtil m_httpUtil;
#endif
	};
}

using namespace avs;


WebRtcNetworkSource::WebRtcNetworkSource()
	: NetworkSource(new WebRtcNetworkSource::Private(this))
{
	m_data = static_cast<WebRtcNetworkSource::Private*>(m_d);
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
	m_params = params;
	rtc::Configuration config;
	std::string stunServer = "stun:stun.l.google.com:19302";
	config.iceServers.emplace_back(stunServer);
	m_data->rtcPeerConnection = createPeerConnection(config, std::bind(&WebRtcNetworkSource::SendConfigMessage, this, std::placeholders::_1), "1");
	
	setNumOutputSlots(numOutputs);

	m_streams = std::move(streams);

	/*for (size_t i = 0; i < numOutputs; ++i)
	{
		const auto& stream = m_streams[i];
		m_streamNodeMap[stream.id] = i;
	}*/

#if TELEPORT_CLIENT
	HTTPUtilConfig httpUtilConfig;
	httpUtilConfig.remoteIP = params.remoteIP;
	httpUtilConfig.remoteHTTPPort = params.remoteHTTPPort;
	httpUtilConfig.maxConnections = params.maxHTTPConnections;
	httpUtilConfig.useSSL = params.useSSL;
	auto f = std::bind(&WebRtcNetworkSource::receiveHTTPFile, this, std::placeholders::_1, std::placeholders::_2);
	return m_data->m_httpUtil.initialize(httpUtilConfig, std::move(f));
#else
	return Result::OK;
#endif
}
void WebRtcNetworkSource::receiveOffer(const std::string& offer)
{
	m_data->rtcPeerConnection->setRemoteDescription(offer);
}

void WebRtcNetworkSource::receiveCandidate(const std::string& candidate, const std::string& mid, int mlineindex)
{
	m_data->rtcPeerConnection->addRemoteCandidate(rtc::Candidate(candidate,mid));
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

#if TELEPORT_CLIENT
	return m_data->m_httpUtil.shutdown();
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

#if TELEPORT_CLIENT
	return m_data->m_httpUtil.process();
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

#if TELEPORT_CLIENT
std::queue<HTTPPayloadRequest>& WebRtcNetworkSource::GetHTTPRequestQueue()
{
	return m_data->m_httpUtil.GetRequestQueue();
}
#endif

void WebRtcNetworkSource::SendConfigMessage(const std::string& str)
{
	messagesToSend.push_back(str);
}

bool WebRtcNetworkSource::getNextStreamingControlMessage(std::string& msg)
{
	if (!messagesToSend.size())
		return false;
	msg = messagesToSend[0];
	messagesToSend.erase(messagesToSend.begin());
	return true;
}

void WebRtcNetworkSource::receiveStreamingControlMessage(const std::string& str)
{
	json message = json::parse(str);
	auto it = message.find("type");
	if (it == message.end())
		return;
	try
	{
		auto &type=it->get<std::string>();
		if (type == "offer")
		{
			auto o = message.find("sdp");
			receiveOffer(o->get<std::string>());
		}
		else if (type == "candidate")
		{
			auto c = message.find("candidate");
			string & candidate=c->get<std::string>();
			auto m= message.find("mid");
			std::string mid;
			if (m != message.end())
				mid = m->get<std::string>();
			auto l = message.find("mlineindex");
			int mlineindex;
			if (l != message.end())
				mlineindex = l->get<int>();
			receiveCandidate(candidate,mid,mlineindex);
		}
	}
	catch (...)
	{
	}
}
#endif