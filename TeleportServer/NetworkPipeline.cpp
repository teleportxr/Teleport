#include "NetworkPipeline.h"

#include <algorithm>
#include <iostream>
#include "network/webrtc_networksink.h"
#if TELEPORT_SUPPORT_SRT
#include "network/srt_efp_networksink.h"
#endif
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/StringFunctions.h"
#include "ServerSettings.h"

namespace
{
	//constexpr double networkPipelineStatInterval = 60000000.0; // 1s
	constexpr int networkPipelineSocketBufferSize = 16 * 1024 * 1024; // 16MiB
}

using namespace teleport;
using namespace server;

NetworkPipeline::NetworkPipeline()
{
	ColorQueue.configure(200000, 16, "ColorQueue");
	TagDataQueue.configure(200, 16, "TagDataQueue");
	GeometryQueue.configure(200000, 16, "GeometryQueue");
	AudioQueue.configure(8192, 120, "AudioQueue");
	CommandQueue.configure(8192, 120, "command");
	MessageQueue.configure(8192, 120, "command");
}


NetworkPipeline::~NetworkPipeline()
{
	ColorQueue.deconfigure();
	TagDataQueue.deconfigure();
	GeometryQueue.deconfigure();
	AudioQueue.deconfigure();
	CommandQueue.deconfigure();
	MessageQueue.deconfigure();
	release();
}

void NetworkPipeline::initialise(const ServerSettings *settings,const ServerNetworkSettings& inNetworkSettings)
{
	// Sending
	if (initialized)
		return;
	mSettings = settings;

	avs::NetworkSinkParams SinkParams = {};
	SinkParams.socketBufferSize = networkPipelineSocketBufferSize;
	SinkParams.throttleToRateKpS = std::min(mSettings->throttleKpS, static_cast<int64_t>(inNetworkSettings.clientBandwidthLimit));// Assuming 60Hz on the other size. k per sec
	SinkParams.socketBufferSize = inNetworkSettings.clientBufferSize;
	SinkParams.requiredLatencyMs = inNetworkSettings.requiredLatencyMs;
	SinkParams.connectionTimeout = inNetworkSettings.connectionTimeout;

	mPipeline.reset(new avs::Pipeline);

	// What transport protocol will we use to stream data?
	switch (inNetworkSettings.streamingTransportLayer )
	{
#if TELEPORT_SUPPORT_SRT
	case avs::StreamingTransportLayer::SRT_EFP:
		mNetworkSink.reset(new avs::SrtEfpNetworkSink);
		break;
		#endif
	case avs::StreamingTransportLayer::WEBRTC:
		mNetworkSink.reset(new avs::WebRtcNetworkSink);
		break;
	default:
		mNetworkSink.reset(new avs::NullNetworkSink);
		break;
	}

	std::string remoteIP=core::WStringToString(inNetworkSettings.remoteIP);
	std::vector<avs::NetworkSinkStream> streams;

	// Video
	{
		avs::NetworkSinkStream stream;
		stream.label = "video";
		stream.parserType = avs::StreamParserType::AVC_AnnexB;
		//stream.useParser = false; default
//			stream.isDataLimitPerFrame = false;
//			stream.counter = 0;
		stream.chunkSize = 64 * 1024;
		stream.id = 20;
		//stream.dataType = avs::NetworkDataType::HEVC;
		streams.emplace_back(std::move(stream));
	}

	// Tag Data
	{
		avs::NetworkSinkStream stream;
		stream.label = "video_tags";
		stream.parserType = avs::StreamParserType::None;
		stream.useParser = false;
		stream.isDataLimitPerFrame = false;
		stream.counter = 0;
		stream.chunkSize = 200;
		stream.id = 40;
		stream.dataType = avs::NetworkDataType::Framed;
		streams.emplace_back(std::move(stream));
	}

	// Audio
	{
		avs::NetworkSinkStream stream;
		stream.label = "audio_server_to_client";
		stream.parserType = avs::StreamParserType::Audio;
		stream.useParser = false;
		stream.isDataLimitPerFrame = false;
		stream.counter = 0;
		stream.chunkSize = 2048;
		stream.id = 60;
		stream.canReceive = true;
		stream.dataType = avs::NetworkDataType::Framed;
		streams.emplace_back(std::move(stream));
	}

	// Geometry
	{
		avs::NetworkSinkStream stream;
		stream.label = "geometry";
		stream.parserType = avs::StreamParserType::Geometry;
		stream.useParser = true;
		stream.isDataLimitPerFrame = true;
		stream.counter = 0;
		stream.chunkSize = 64 * 1024;
		stream.id = 80;
		stream.dataType = avs::NetworkDataType::Framed;
		streams.emplace_back(std::move(stream));
	}

	// Commands
	{
		avs::NetworkSinkStream stream;
		stream.label = "command";
		stream.parserType = avs::StreamParserType::None;
		stream.useParser = false;
		stream.isDataLimitPerFrame = true;
		stream.counter = 0;
		stream.chunkSize = 64 * 1024;
		stream.id = 100;
		stream.canReceive = true;
		stream.dataType = avs::NetworkDataType::Generic;
		streams.emplace_back(std::move(stream));
	}
	avs::NetworkSink* networkSink = mNetworkSink.get();
	if (!networkSink->configure(std::move(streams), nullptr, inNetworkSettings.localPort, remoteIP.c_str(), inNetworkSettings.remotePort, SinkParams))
	{
		TELEPORT_CERR << "Failed to configure network sink!" << "\n";
		initialized = false;
		return;
	}

	// Video
	{
		if (!avs::PipelineNode::link(ColorQueue, *mNetworkSink))
		{
			TELEPORT_CERR << "Failed to configure network pipeline for video!" << "\n";
			initialized = false;
			return;
		}
		mPipeline->add(&ColorQueue);
	}

	// Tag Data
	{
		if (!avs::PipelineNode::link(TagDataQueue, *mNetworkSink))
		{
			TELEPORT_CERR << "Failed to configure network pipeline for video tag data!" << "\n";
			initialized = false;
			return;
		}
		mPipeline->add(&TagDataQueue);
	}

	// Audio
	{
		if (!avs::PipelineNode::link(AudioQueue, *mNetworkSink))
		{
			TELEPORT_CERR << "Failed to configure network pipeline for audio!" << "\n";
			initialized = false;
			return;
		}
		mPipeline->add(&AudioQueue);
	}

	// Geometry
	{
		if (!avs::PipelineNode::link(GeometryQueue, *mNetworkSink))
		{
			TELEPORT_CERR << "Failed to configure network pipeline for geometry!" << "\n";
			initialized = false;
			return;
		}
		mPipeline->add(&GeometryQueue);
	}

	// Command
	{
		if (!avs::PipelineNode::link(CommandQueue, *mNetworkSink))
		{
			TELEPORT_CERR << "Failed to configure network pipeline for commands!" << "\n";
			initialized = false;
			return;
		}
		mPipeline->add(&CommandQueue);
	}
	// Messages
	{
		if (!avs::PipelineNode::link(*mNetworkSink, MessageQueue))
		{
			TELEPORT_CERR << "Failed to configure network pipeline for messages!" << "\n";
			initialized = false;
			return;
		}
		mPipeline->add(&MessageQueue);
	}
	mPipeline->add(mNetworkSink.get());

#if WITH_REMOTEPLAY_STATS
	mLastTimestamp = avs::Platform::getTimestamp();
#endif // WITH_REMOTEPLAY_STATS

	mPrevProcResult = avs::Result::OK;
	initialized = true;
}

void NetworkPipeline::release()
{
	mPipeline.reset();
	if (mNetworkSink)
		mNetworkSink->deconfigure();
	mNetworkSink.reset();
	initialized = false;
}

bool NetworkPipeline::process()
{
	const avs::Result result = mPipeline->process();
	// Prevent spamming of errors from NetworkSink. This happens when there is a connection issue.
	if (!result && result != avs::Result::IO_Empty)
	{
		if (result != mPrevProcResult)
		{
			TELEPORT_CERR << "Network pipeline processing encountered an error!" << "\n";
			mPrevProcResult = result;
		}
		return false;
	}

	mPrevProcResult = result;

#if 0
	avs::Timestamp timestamp = avs::Platform::getTimestamp();
	if (avs::Platform::getTimeElapsed(lastTimestamp, timestamp) >= networkPipelineStatInterval)
	{
		const avs::NetworkSinkCounters counters = mNetworkSink->getCounterValues();
		TELEPORT_COUT << "DP: " << counters.decoderPacketsQueued << " | NP: " << counters.networkPacketsSent << " | BYTES: " << counters.bytesSent << "\n";
		lastTimestamp = timestamp;
	}
	mNetworkSink->setDebugStream(mSettings->debugStream);
	mNetworkSink->setDebugNetworkPackets(mSettings->enableDebugNetworkPackets);
	mNetworkSink->setDoChecksums(mSettings->enableChecksums);
	mNetworkSink->setEstimatedDecodingFrequency(mSettings->estimatedDecodingFrequency);
#endif // WITH_REMOTEPLAY_STATS
	return true;
}

avs::Pipeline* NetworkPipeline::getAvsPipeline() const
{
	return mPipeline.get();
}

avs::Result NetworkPipeline::getCounters(avs::NetworkSinkCounters& counters) const
{
	if (mNetworkSink)
	{
		counters = mNetworkSink->getCounters();
	}
	else
	{
		TELEPORT_CERR << "Can't return counters because network sink is null." << "\n";
		return avs::Result::Node_Null;
	}
	return avs::Result::OK;
}

void NetworkPipeline::setProcessingEnabled(bool enable)
{
	if (mNetworkSink)
		mNetworkSink->setProcessingEnabled(enable);
}

bool NetworkPipeline::isProcessingEnabled() const
{
	if (mNetworkSink)
		return mNetworkSink->isProcessingEnabled();
	return false;
}

bool NetworkPipeline::getNextStreamingControlMessage(std::string &str)
{
	if (mNetworkSink)
		return mNetworkSink->getNextStreamingControlMessage(str);
	return false;
}

void NetworkPipeline::receiveStreamingControlMessage(const std::string& str)
{
	if (mNetworkSink)
		return mNetworkSink->receiveStreamingControlMessage(str);
}
