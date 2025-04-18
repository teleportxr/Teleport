// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#include <iostream>
#include "network/webrtc_common.h"
#include "libavstream/network/webrtc_networksource.h"
#include <ElasticFrameProtocol.h>
#include <libavstream/queue.hpp>
#include <logger.hpp>
#include <libavstream/timer.hpp>

#ifdef __ANDROID__
#include <pthread.h>
#include <sys/prctl.h>
#endif

#include <functional>

//rtc
#include <rtc/rtc.hpp>
#include <nlohmann/json.hpp>
#include "TeleportCore/ErrorHandling.h"
#if TELEPORT_CLIENT
#include <libavstream/httputil.hpp>
#endif
#include <ios>
#ifdef _MSC_VER
FILE _iob[] = { *stdin, *stdout, *stderr };

extern "C" FILE * __cdecl __iob_func(void)
{
	return _iob;
}
#endif
#pragma optimize("",off)
using namespace std;
using nlohmann::json;

constexpr uint16_t EVEN_ID(uint16_t id) { return id - (id % 2); }
constexpr uint16_t ODD_ID(uint16_t id) { return EVEN_ID(id) + 1; }

namespace avs
{
	static const char *stringOf(rtc::PeerConnection::IceState state)
	{
		switch(state)
		{
			case rtc::PeerConnection::IceState::New	:return "New";
			case rtc::PeerConnection::IceState::Checking		:return "Checking";
			case rtc::PeerConnection::IceState::Connected		:return "Connected";
			case rtc::PeerConnection::IceState::Completed		:return "Completed";
			case rtc::PeerConnection::IceState::Failed			:return "Failed";
			case rtc::PeerConnection::IceState::Disconnected	:return "Disconnected";
			case rtc::PeerConnection::IceState::Closed			:return "Closed";
			default:
			return "INVALID ";
		};
	};
	struct ClientDataChannel
	{
		ClientDataChannel() {}
		ClientDataChannel(const ClientDataChannel& dc)
		{
			rtcDataChannel = dc.rtcDataChannel;
		}
		shared_ptr<rtc::DataChannel> rtcDataChannel;
		atomic<size_t> bytesReceived = 0;
		atomic<size_t> bytesSent = 0;
		bool closed = false;
		bool readyToSend = false;
		std::vector<uint8_t> sendBuffer;
	};
	struct WebRtcNetworkSource::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(WebRtcNetworkSource, PipelineNode)
		std::vector<ClientDataChannel> dataChannels;
		std::shared_ptr<rtc::PeerConnection> rtcPeerConnection;
		std::unique_ptr<ElasticFrameProtocolReceiver> m_EFPReceiver;
		NetworkSourceCounters m_counters;
		bool onDataChannel(shared_ptr<rtc::DataChannel> dc);
		std::unordered_map<uint32_t, uint8_t> idToStreamIndex;
		//! Map input nodes to streams outgoing. Many to one relation.
		std::unordered_map<uint8_t, uint8_t> inputToStreamIndex;
		//! Map input nodes to streams outgoing. One to one relation.
		std::unordered_map<uint8_t, uint8_t> streamIndexToOutput;
		Result sendData(uint8_t id, const uint8_t* packet, size_t sz);
	};
}
avs::StreamingConnectionState ConvertConnectionState(rtc::PeerConnection::State rtcState)
{
	switch (rtcState)
	{
	case rtc::PeerConnection::State::New:
		return avs::StreamingConnectionState::NEW_UNCONNECTED;
	case rtc::PeerConnection::State::Connecting:
		return avs::StreamingConnectionState::CONNECTING;
	case rtc::PeerConnection::State::Connected:
		return avs::StreamingConnectionState::CONNECTED;
	case rtc::PeerConnection::State::Disconnected:
		return avs::StreamingConnectionState::DISCONNECTED;
	case rtc::PeerConnection::State::Failed:
		return avs::StreamingConnectionState::FAILED;
	case rtc::PeerConnection::State::Closed:
		return avs::StreamingConnectionState::CLOSED;
	default:
		return avs::StreamingConnectionState::ERROR_STATE;
	}
}

static shared_ptr<rtc::PeerConnection> createClientPeerConnection(const rtc::Configuration& config,
avs::WebRtcNetworkSource *src)
{
	auto pc = std::make_shared<rtc::PeerConnection>(config);
	
	src->SetStreamingConnectionState(avs::StreamingConnectionState::NEW_UNCONNECTED);
	pc->onStateChange(
		[src,pc](rtc::PeerConnection::State state)
		{
			std::cout << "PeerConnection onStateChange to: " << state << std::endl;
			src->SetStreamingConnectionState(ConvertConnectionState(state));
			if(state==rtc::PeerConnection::State::Closed)
			{
				src->resetPeerConnection();
			}
		});

	pc->onIceStateChange([](rtc::PeerConnection::IceState state)
		{
			std::cout << "Ice State: " << avs::stringOf(state)<< std::endl;
		});

	pc->onGatheringStateChange([](rtc::PeerConnection::GatheringState state)
		{
			std::cout << "Gathering State: " << state << std::endl;
		});

	pc->onLocalDescription([src](rtc::Description description)
		{
			// This is our answer.
		//	std::bind(&WebRtcNetworkSource::sendConfigMessage, this, std::placeholders::_1), std::bind(&WebRtcNetworkSource::Private::onDataChannel, m_data, std::placeholders::_1), "1"
			json message = { {"id", "1"},
						{"teleport-signal-type", description.typeString()},
						{"sdp",  std::string(description)} };

			src->sendConfigMessage(message.dump());
		});

	pc->onLocalCandidate([ src](rtc::Candidate candidate)
		{
			json message = { {"id", "1"},
						{"teleport-signal-type", "candidate"},
						{"candidate", std::string(candidate)},
						{"mid", candidate.mid()} ,
						{"mlineindex", 0} };
			src->sendConfigMessage(message.dump());
		});

	pc->onDataChannel([src](shared_ptr<rtc::DataChannel> dc)
	{
		if(!src->m_data->onDataChannel(dc))
		{
			json message = { {"id", "1"},
						{"teleport-signal-type", "error"},
						{"message", "Bad data channel received."} };

			src->sendConfigMessage(message.dump());
		}
	});

	//peerConnectionMap.emplace(id, pc);
	return pc;
};

using namespace avs;


WebRtcNetworkSource::WebRtcNetworkSource()
	: NetworkSource(new WebRtcNetworkSource::Private(this))
{
	rtc::InitLogger(rtc::LogLevel::Warning);
	m_data = static_cast<WebRtcNetworkSource::Private*>(m_d);
}

WebRtcNetworkSource::~WebRtcNetworkSource()
{
	deconfigure();
}

Result WebRtcNetworkSource::configure(std::vector<NetworkSourceStream>&& in_streams,int numInputs, const NetworkSourceParams& params)
{
	name="Network Source";
	size_t numOutputs = in_streams.size();
	if (numOutputs == 0 )
	{
		return Result::Node_InvalidConfiguration;
	}
	if (!params.remoteIP || !params.remoteIP[0])
	{
		return Result::Node_InvalidConfiguration;
	}
	offer="";
	m_params = params;
	m_data->m_EFPReceiver.reset(new ElasticFrameProtocolReceiver(100, 0, nullptr, ElasticFrameProtocolReceiver::EFPReceiverMode::RUN_TO_COMPLETION));

	m_data->m_EFPReceiver->receiveCallback = [this](ElasticFrameProtocolReceiver::pFramePtr& rPacket, ElasticFrameProtocolContext* pCTX)->void
	{
		if (rPacket->mBroken)
		{
			AVSLOG(Warning) << "Received NAL-units of size: " << unsigned(rPacket->mFrameSize) <<
				" Stream ID: " << unsigned(rPacket->mStreamID) <<
				" PTS: " << unsigned(rPacket->mPts) <<
				" Corrupt: " << rPacket->mBroken <<
				" EFP connection: " << unsigned(rPacket->mSource) << "\n";
			std::lock_guard<std::mutex> guard(m_dataMutex);
			m_data->m_counters.incompleteDecoderPacketsReceived++;
		}
		else
		{
			std::lock_guard<std::mutex> guard(m_dataMutex);
			m_data->m_counters.decoderPacketsReceived++;
		}

		size_t bufferSize = sizeof(StreamPayloadInfo) + rPacket->mFrameSize;
		if (bufferSize > m_tempBuffer.size())
		{
			m_tempBuffer.resize(bufferSize);
		}

		StreamPayloadInfo frameInfo;
		frameInfo.frameID = rPacket->mPts;
		frameInfo.dataSize = rPacket->mFrameSize;
		frameInfo.connectionTime = TimerUtil::GetElapsedTimeS();
		frameInfo.broken = rPacket->mBroken;

		memcpy(m_tempBuffer.data(), &frameInfo, sizeof(StreamPayloadInfo));
		memcpy(&m_tempBuffer[sizeof(StreamPayloadInfo)], rPacket->pFrameData, rPacket->mFrameSize);
		// The mStreamID is encoded sender-side within the EFP wrapper.
		
		uint8_t streamIndex = m_data->idToStreamIndex[rPacket->mStreamID];// streamID is 20,40,60, etc
		uint8_t outputNodeIndex = m_data->streamIndexToOutput[streamIndex];
		IOInterface *outputNode = dynamic_cast<IOInterface*>(getOutput(outputNodeIndex));
		if (!outputNode)
		{
			AVSLOG(Warning) << "WebRtcNetworkSource EFP Callback: Invalid output node. Should be an avs::Queue.";
			return;
		}

		size_t numBytesWrittenToOutput;
		auto result = outputNode->write(m_data->q_ptr(), m_tempBuffer.data(), bufferSize, numBytesWrittenToOutput);

		if (!result)
		{
			AVSLOG(Warning) << "WebRtcNetworkSource EFP Callback: Failed to write to output node.";
			return;
		}

		if (numBytesWrittenToOutput < bufferSize)
		{
			AVSLOG(Warning) << "WebRtcNetworkSource EFP Callback: Incomplete frame written to output node.";
		}
	};

	setNumInputSlots(numInputs);
	setNumOutputSlots(numOutputs);

	m_streams = std::move(in_streams);
	streamStatus.resize(m_streams.size());

	// This will map from the stream id's - 20,40,60,80 etc 
	// to the index of outputs, 0,1,2,3,4 etc.
	m_data->dataChannels.resize(m_streams.size());
	m_data->idToStreamIndex.clear();

	// result lastResult.
	setResult(avs::Result::OK);

	return Result::OK;
}

Result WebRtcNetworkSource::onInputLink(int slot, PipelineNode* node)
{
	std::string name=node->getDisplayName();
	for (int i=0;i<m_streams.size();i++)
	{
		auto& stream = m_streams[i];
		if(stream.inputName==name)
			m_data->inputToStreamIndex[slot] = i;
	}
	IOInterface *nodeIO = dynamic_cast<IOInterface *>(node);
	if(slot>=inputInterfaces.size())
	{
		inputInterfaces.resize(slot+1);
	}
	inputInterfaces[slot]=nodeIO;
	return Result::OK;
}

Result WebRtcNetworkSource::onOutputLink(int slot, PipelineNode* node)
{
	std::string name = node->getDisplayName();
	for (int i = 0; i < m_streams.size(); i++)
	{
		auto& stream = m_streams[i];
		if (stream.outputName == name)
			m_data->streamIndexToOutput[i] = slot;
	}
	return Result::OK;
}
void WebRtcNetworkSource::resetPeerConnection()
{
	m_data->rtcPeerConnection.reset();
}

void WebRtcNetworkSource::receiveOffer(const std::string& sdp)
{
	rtc::Description rtcDescription(sdp,"offer");
	rtc::Configuration config;
	// Enable TCP for e.g. Heroku
	//config.enableIceTcp=true;
	config.enableIceUdpMux=true;
	for(size_t i=0;i<1000;i++)
	{
		const char *srv=iceServers[i];
		if(!srv)
			break;
		config.iceServers.emplace_back(srv);
	}
	if(!m_data->rtcPeerConnection)
	{
		m_data->rtcPeerConnection = createClientPeerConnection(config, this);
	}
	else if(m_data->rtcPeerConnection->state()==rtc::PeerConnection::State::Closed)
	{
		m_data->rtcPeerConnection = createClientPeerConnection(config, this);
	}
	try
	{
		m_data->rtcPeerConnection->setRemoteDescription(rtcDescription);
	}
	catch(std::runtime_error &r)
	{
		AVSLOG(Error)<<"IceState: runtime_error "<<(r.what()?r.what():"")<<" for description: "<<sdp<<".\n";
	}
	catch(std::exception &e)
	{
		AVSLOG(Error)<<"IceState: exception "<<(e.what()?e.what():"")<<" for description: "<<sdp<<".\n";
	}
	catch(...)
	{
		AVSLOG(Error)<<"IceState: unknown exception for description: "<<sdp<<".\n";
	}
	rtc::PeerConnection::IceState iceState=m_data->rtcPeerConnection->iceState();
	//if(iceState==rtc::PeerConnection::IceState::Closed)
	{
		AVSLOG(Info)<<"IceState: "<<stringOf(iceState)<<".\n";
	}
	if(offer.length())
	{
		if(offer!=sdp)
		{
			AVSLOG(Error) << "WebRtcNetworkSource: Received remote offer that doesn't match previous offer." << std::endl;
	
		}
		else
		{
			AVSLOG(Error) << "WebRtcNetworkSource: Received remote offer that matches previous offer." << std::endl;
		}
	}
	else
		AVSLOG(Info) << ": info: WebRtcNetworkSource: Received remote offer." << std::endl;
	offer=sdp;
	for(int j=0;j<cachedCandidates.size();j++)
	{
		const auto &c=cachedCandidates[j];
		receiveCandidate(c.candidate,c.mid,c.mlineindex);
	}
	cachedCandidates.clear();
}
void WebRtcNetworkSource::receiveCandidate(const std::string& candidate, const std::string& mid, int mlineindex)
{
	if(!m_data->rtcPeerConnection||!m_data->rtcPeerConnection->remoteDescription().has_value())
	{
		cachedCandidates.push_back({candidate,mid,mlineindex});
		return;
	}
	try
	{
		rtc::PeerConnection::IceState iceState=m_data->rtcPeerConnection->iceState();
		//if(iceState==rtc::PeerConnection::IceState::Failed)
		{
			AVSLOG(Info)<<"IceState: "<<stringOf(iceState)<<".\n";
		}
		m_data->rtcPeerConnection->addRemoteCandidate(rtc::Candidate(candidate, mid));
	}
	catch (std::logic_error err)
	{
		if (err.what())
			std::cerr << err.what() << std::endl;
		else
			std::cerr << "std::logic_error." << std::endl;
	}
	catch (...)
	{
		std::cerr << "rtcPeerConnection->addRemoteCandidate exception." << std::endl;
	}
}

void WebRtcNetworkSource::receiveHTTPFile(const char* buffer, size_t bufferSize)
{
	uint8_t streamIndex = m_data->idToStreamIndex[m_params.httpStreamID];
	uint8_t nodeIndex = m_data->streamIndexToOutput[streamIndex];

	IOInterface * outputNode = dynamic_cast<IOInterface*>(getOutput(nodeIndex));
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
		m_data->m_counters.httpFilesReceived++;
	}
}

void WebRtcNetworkSource::kill()
{
	if(m_data->rtcPeerConnection)
		m_data->rtcPeerConnection->close();
}

Result WebRtcNetworkSource::deconfigure()
{
	if (getNumOutputSlots() <= 0)
	{
		return Result::Node_NotConfigured;
	}
	offer="";
	inputInterfaces.clear();
	webRtcState=StreamingConnectionState::NEW_UNCONNECTED;
	setNumOutputSlots(0);
	// This should clear out the rtcDataChannel shared_ptrs, so that rtcPeerConnection can destroy them.
	m_data->dataChannels.clear();
	m_data->rtcPeerConnection = nullptr;
	m_streams.clear();
	m_data->m_counters = {};
	m_data->idToStreamIndex.clear();
	m_streams.clear();

	m_data->m_EFPReceiver.reset();
	return Result::OK;
}

Result WebRtcNetworkSource::process(uint64_t timestamp, uint64_t deltaTime)
{
	if (getNumOutputSlots() == 0 )
	{
		return Result::Node_NotConfigured;
	}
	// Can't recover from a disconnection, must reset.
	if(getLastResult()==Result::Network_Disconnection)
	{
		return Result::Network_Disconnection;
	}
	PipelineNode::process(timestamp,deltaTime);
	// receiving data from the network is handled elsewhere.
	// Here we only handle receiving data from inputs to be sent onward.

	// Called to get the data from the input nodes
	auto readInput = [this](uint8_t inputNodeIndex, uint8_t streamIndex,
		std::vector<uint8_t> &buffer,size_t& numBytesRead) -> Result
	{
		PipelineNode* node = getInput(inputNodeIndex);
		auto& stream = m_streams[streamIndex];

		assert(node);
		//assert(buffer.size() >= stream.chunkSize);
		static uint64_t frameID = 0;
		IOInterface *nodeIO = inputInterfaces[inputNodeIndex];
		if (nodeIO)
		{
			size_t bufferSize = buffer.size();
			Result result = nodeIO->read(this, buffer.data(), bufferSize, numBytesRead);
			if (result == Result::IO_Retry)
			{
				buffer.resize(bufferSize);
				result = nodeIO->read(this, buffer.data(), bufferSize, numBytesRead);
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
	bool disconnected=false;
	for (uint32_t i = 0; i < (uint32_t)getNumInputSlots(); ++i)
	{
		uint32_t streamIndex = m_data->inputToStreamIndex[i];
		if (streamIndex >= m_streams.size())
			continue;
		PipelineNode* node = getInput(i);
		if (!node)
			continue;
		const auto& stream = m_streams[streamIndex];
		ClientDataChannel& dataChannel = m_data->dataChannels[streamIndex];
		// If channel is backed-up in WebRTC, don't grab data off the queue.
		if (!dataChannel.readyToSend)
			continue;
		size_t numBytesRead = 0;
		Result result = Result::OK;
		while (result == Result::OK)
		{
			try
			{
				result = readInput(i, streamIndex, dataChannel.sendBuffer, numBytesRead);
				if (result != Result::OK)
				{
					if (result != Result::IO_Empty)
					{
						AVSLOG(Error) << "WebRtcNetworkSink: Failed to read from input node: " << i << "\n";
						break;
					}
				}
			}
			catch (const std::bad_alloc&)
			{
				return Result::IO_OutOfMemory;
			}
			if (numBytesRead == 0)
			{
				break;
			}
			Result res = Result::OK;
			//if (stream.useParser && m_data->m_parsers.find(i) != m_data->m_parsers.end())
			//{
			//res = m_data->m_parsers[i]->parse(((const char*)stream.buffer.data()), numBytesRead);
			//}
			//else
			{
				res = m_data->sendData(stream.id, dataChannel.sendBuffer.data(), numBytesRead);
			}
			if(res==Result::Network_Disconnection)
				disconnected=true;
			if (!res)
			{
				break;
			}
		}
	}
	if(disconnected)
	{
		return Result::Network_Disconnection;
	}
	static float intro = 0.01f;
	// update the stream stats.
	if(deltaTime>0)
	for (int i=0;i<m_data->dataChannels.size();i++)
	{
		ClientDataChannel& dc = m_data->dataChannels[i];
		size_t b = dc.bytesReceived;
		dc.bytesReceived= 0;
		size_t o = dc.bytesSent;
		dc.bytesSent = 0;
		streamStatus[i].inwardBandwidthKps *=1.0f-intro;
		streamStatus[i].inwardBandwidthKps +=intro* (float)b / float(deltaTime)*(1000.0f/1024.0f);
		if (m_streams[i].outgoing)
		{
			streamStatus[i].outwardBandwidthKps *= 1.0f - intro;
			streamStatus[i].outwardBandwidthKps += intro * (float)o / float(deltaTime) * (1000.0f / 1024.0f);
		}
	}
	return Result::OK;
}

NetworkSourceCounters WebRtcNetworkSource::getCounterValues() const
{
	return m_data->m_counters;
}

#ifndef _MSC_VER
__attribute__((optnone))
#endif


void WebRtcNetworkSource::sendConfigMessage(const std::string &str)
{
	std::lock_guard<std::mutex> lock(messagesMutex);
	messagesToSend.push_back(str);
}

void WebRtcNetworkSource::SetStreamingConnectionState(StreamingConnectionState s)
{
	webRtcState=s;
	if(webRtcState!=StreamingConnectionState::CONNECTED&&webRtcState!=StreamingConnectionState::CONNECTING&&webRtcState!=StreamingConnectionState::NEW_UNCONNECTED)
	{
		offer="";
	}
	if(webRtcState==StreamingConnectionState::CONNECTED)
	{
		// Recover from disconnection, allow processing again!
		setResult(avs::Result::OK);
	}
}

bool WebRtcNetworkSource::getNextStreamingControlMessage(std::string& msg)
{
	std::lock_guard<std::mutex> lock(messagesMutex);
	if (!messagesToSend.size())
		return false;
	msg = messagesToSend[0];
	messagesToSend.erase(messagesToSend.begin());
	return true;
}

void WebRtcNetworkSource::receiveStreamingControlMessage(const std::string& str)
{
	if(!str.length())
		return;
	json message = json::parse(str);
	auto it = message.find("teleport-signal-type");
	if (it == message.end())
		return;
	std::string type= message["teleport-signal-type"];

	try
	{
		auto type=it->get<std::string>();
		if (type == "offer")
		{
			auto o = message.find("sdp");
			string sdp= o->get<std::string>();
			receiveOffer(sdp);
		}
		else if (type == "candidate")
		{
			AVSLOG(Info) << ": info: WebRtcNetworkSource: Received remote candidate " ;
			auto c = message.find("candidate");
			string candidate=c->get<std::string>();
AVSLOG(Info) << "---"<< candidate<<" "<< std::endl;
			auto m= message.find("mid");
			std::string mid;
			if (m != message.end())
			{
				mid = m->get<std::string>();
			}
			auto l = message.find("mlineindex");
			int mlineindex;
			if (l != message.end())
				mlineindex = l->get<int>();
			receiveCandidate(candidate,mid,mlineindex);
		}
	}
	catch (std::invalid_argument inv)
	{
		if(inv.what())
			std::cerr << inv.what() << std::endl;
		else
			std::cerr << "std::invalid_argument." << std::endl;
	}
	catch (...)
	{
		std::cerr << "receiveStreamingControlMessage exception." << std::endl;
	}
}

bool WebRtcNetworkSource::Private::onDataChannel(shared_ptr<rtc::DataChannel> dc)
{
	string label=dc->label();
	int forced_id=0;
	bool force_unframed=false;
	if(label=="video")
		forced_id=20;
	else if(label=="video_tags")
		forced_id=40;
	else if(label=="audio_server_to_client")
		forced_id=60;
	else if(label=="geometry")
		forced_id=80;
	else if(label=="geometry_unframed")
	{
		forced_id=80;
		force_unframed=true;
		label="geometry";
	}
	else if(label=="reliable")
		forced_id=1020;
	else if(label=="unreliable")
		forced_id=120;
	else
		return false;
	std::cout<<"onDataChannel: "<<dc->label()<<" id "<<forced_id<<"\n";
	// make the id even.
	uint16_t id = EVEN_ID(forced_id);
	// find the dataChannel whose label matches this channel's label.
	int dcIndex = -1;
	for (int i = 0; i < dataChannels.size(); i++)
	{
		auto& stream = q_ptr()->m_streams[i];
		if (stream.label ==label)
		{
			dcIndex = i;
			if(force_unframed)
			{
				stream.framed=false;
				stream.expects_frame_info=true;
			}
		}
	}
	if (dcIndex < 0)
	{
		// TODO: Inform the server that a channel has not been recognized - don't send data on it.
		
		std::cerr << "Bad dataChannel index "<<dcIndex<<"\n";
		return false;
	}
	idToStreamIndex[id] = dcIndex;
	ClientDataChannel& dataChannel = dataChannels[dcIndex];
	dataChannel.readyToSend = false;
	dataChannel.rtcDataChannel = dc;
	std::cout << "ClientDataChannel from " << id << " received with label \"" << dc->label() << "\"" << std::endl;

	dc->onOpen([this,dcIndex]()
		{
			ClientDataChannel& dataChannel = dataChannels[dcIndex];
			dataChannel.readyToSend = true;
			std::cout << "DataChannel opened" << std::endl;
		});
	dc->onBufferedAmountLow([this, dcIndex]()
		{
			ClientDataChannel& dataChannel = dataChannels[dcIndex];
			dataChannel.readyToSend = true;
			std::cout << "DataChannel onBufferedAmountLow" << std::endl;
		});
	dc->onClosed([id]()
		{
			std::cout << "DataChannel from " << id << " closed" << std::endl;
		});
	dc->onMessage([this,&dataChannel,id](rtc::binary b)
		{
		// data holds either std::string or rtc::binary
			uint8_t streamIndex = idToStreamIndex[id];
			auto o = streamIndexToOutput.find(streamIndex);
			if (o == streamIndexToOutput.end())
				return;
			uint8_t outputNodeIndex = o->second;
			dataChannel.bytesReceived += b.size();
			auto & stream=q_ptr()->m_streams[streamIndex];
			if (!stream.framed)
			{
				size_t numBytesWrittenToOutput=0;
				avs::IOInterface *outputNode = dynamic_cast<avs::IOInterface*>(q_ptr()->getOutput(outputNodeIndex));
				if (!outputNode)
				{
					AVSLOG(Warning) << "WebRtcNetworkSource EFP Callback: Invalid output node. Should implement avs::IOInterface.\n";
					return;
				}
				avs::Result result=avs::Result::OK;
				if(stream.expects_frame_info)
				{
					StreamPayloadInfo frameInfo;
					frameInfo.frameID = 0;
					frameInfo.dataSize = b.size();
					frameInfo.connectionTime = TimerUtil::GetElapsedTimeS();
					frameInfo.broken = false;
					size_t bufferSize = sizeof(StreamPayloadInfo) + b.size()-sizeof(size_t);
					if (bufferSize > q_ptr()->m_tempBuffer.size())
					{
						q_ptr()->m_tempBuffer.resize(bufferSize);
					}

					memcpy(q_ptr()->m_tempBuffer.data(), &frameInfo, sizeof(StreamPayloadInfo));
					//skip over the payload size, go straight to the payload type:
					memcpy(&q_ptr()->m_tempBuffer[sizeof(StreamPayloadInfo)], b.data()+sizeof(size_t), b.size()-sizeof(size_t));
					result = outputNode->write(q_ptr(), (const void*)q_ptr()->m_tempBuffer.data(), bufferSize, numBytesWrittenToOutput);
				}
				else
				{
				result = outputNode->write(q_ptr(), (const void*)b.data(), b.size(), numBytesWrittenToOutput);
				}
				if (numBytesWrittenToOutput!=b.size())
				{
					AVSLOG_NOSPAM(Warning,"WebRtcNetworkSource EFP onMessage: {0}: failed to write all to output Node.\n",stream.label);
					return;
				}
#if TELEPORT_LIBAV_MEASURE_PIPELINE_BANDWIDTH
				q_ptr()->bytes_received+=numBytesWrittenToOutput;
#endif
			}
			else
			{
				auto val = m_EFPReceiver->receiveFragmentFromPtr((const uint8_t*)b.data(), b.size(), 0);
				if (val != ElasticFrameMessages::noError)
				{
					std::cerr << "EFP Error: Invalid data fragment received" << "\n";
				}
				else
				{
#if TELEPORT_LIBAV_MEASURE_PIPELINE_BANDWIDTH
					q_ptr()->bytes_received+=b.size();
#endif
				}
			}
			//std::cout << "Binary message from " << id
			//	<< " received, size=" << std::get<rtc::binary>(data).size() << std::endl;
		},[this, &dataChannel,id](rtc::string s) {});
		
		return true;
}

Result WebRtcNetworkSource::Private::sendData(uint8_t id, const uint8_t* packet, size_t sz)
{
	auto index = idToStreamIndex[id];
	auto& dataChannel = dataChannels[idToStreamIndex[id]];
	auto c = dataChannel.rtcDataChannel;
	if (c)
	{
		try
		{
			if (c->isOpen())
			{
				// From Google's comments - assume the same applies to libdatachannel.
				// Send() sends |data| to the remote peer. If the data can't be sent at the SCTP
				// level (due to congestion control), it's buffered at the data channel level,
				// up to a maximum of 16MB. If Send is called while this buffer is full, the
				// data channel will be closed abruptly.
				//
				// So, it's important to use buffered_amount() and OnBufferedAmountChange to
				// ensure the data channel is used efficiently but without filling this
				// buffer.
				if (c->bufferedAmount() + sz >= 1024 * 1024 * 16)
				{
					dataChannel.readyToSend = false;
					std::cerr << "WebRTC: channel " << (int)id << ", failed to send packet of size " << sz << " as it would overflow the webrtc buffer.\n";
					return Result::Failed;
				}
				// Can't send a buffer greater than 262144. even 64k is dodgy:
				if (sz >= c->maxMessageSize())
				{
					std::cerr << "WebRTC: channel " << (int)id << ", failed to send packet of size " << sz << " as it is too large for a webrtc data channel.\n";
					return Result::Failed;
				}
				if (!c->send((std::byte*)packet, sz))
				{
					dataChannel.readyToSend = false;
					std::cerr << "WebRTC: channel " << (int)id << ", failed to send packet of size " << sz << ", buffered amount is " << c->bufferedAmount() << ", available is " << c->availableAmount() << ".\n";
					return Result::Failed;
				}
				dataChannel.bytesSent += sz;
			}
			else
			{
 				std::cerr << "WebRTC: channel " << (int)id << ", failed to send packet of size " << sz << ", channel is closed. Should reset WebRTC connection.\n";
				return Result::Network_Disconnection;
			}
		}
		catch (std::runtime_error err)
		{
			if (err.what())
				std::cerr << err.what() << std::endl;
			else
				std::cerr << "std::runtime_error." << std::endl;
			return Result::Failed;
		}
		catch (std::logic_error err)
		{
			if (err.what())
				std::cerr << err.what() << std::endl;
			else
				std::cerr << "std::logic_error." << std::endl;
			return Result::Failed;
		}

	}
	return Result::OK;
}
