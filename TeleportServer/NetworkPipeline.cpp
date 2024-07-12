#include "NetworkPipeline.h"

#include <algorithm>
#include <iostream>
#include "network/webrtc_networksink.h"
#if TELEPORT_SUPPORT_SRT
#include "network/srt_efp_networksink.h"
#endif
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/StringFunctions.h"
#include "TeleportCore/Profiling.h"
#include "ServerSettings.h"

namespace
{
	//constexpr double networkPipelineStatInterval = 60000000.0; // 1s
	constexpr int networkPipelineSocketBufferSize = 16 * 1024 * 1024; // 16MiB
}

using namespace teleport;
using namespace server;

static const char *COLOR_QUEUE = "ColorQueue";
static const char *TAG_DATA_QUEUE = "TagDataQueue";
static const char *GEOMETRY_QUEUE = "GeometryQueue";
static const char *AUDIO_QUEUE = "AudioQueue";
static const char *RELIABLE_SEND_QUEUE = "Reliable Send Queue";
static const char *UNRELIABLE_RECEIVE_QUEUE = "Unreliable Receive Queue";
static const char *UNRELIABLE_SEND_QUEUE = "Unreliable Send Queue";
static const char *RELIABLE_RECEIVE_QUEUE = "Reliable Receive Queue";

NetworkPipeline::NetworkPipeline()
{
	ColorQueue.configure(200000, 16, COLOR_QUEUE);
	TagDataQueue.configure(200, 16, TAG_DATA_QUEUE);
	GeometryQueue.configure(200000, 16, GEOMETRY_QUEUE);
	AudioQueue.configure(8192, 120, AUDIO_QUEUE);
	reliableSendQueue.configure(8192, 120, RELIABLE_SEND_QUEUE);
	unreliableReceiveQueue.configure(8192, 120, UNRELIABLE_RECEIVE_QUEUE);

	unreliableSendQueue.configure(8192, 120, UNRELIABLE_SEND_QUEUE);
	reliableReceiveQueue.configure(8192, 120, RELIABLE_RECEIVE_QUEUE);
}


NetworkPipeline::~NetworkPipeline()
{
	ColorQueue.deconfigure();
	TagDataQueue.deconfigure();
	GeometryQueue.deconfigure();
	AudioQueue.deconfigure();
	reliableSendQueue.deconfigure();
	unreliableReceiveQueue.deconfigure();

	unreliableSendQueue.deconfigure();
	reliableReceiveQueue.deconfigure();
	release();
}

void NetworkPipeline::initialise(const ServerNetworkSettings& inNetworkSettings)
{
	// Sending
	if (initialized)
		return;

	avs::NetworkSinkParams SinkParams = {};
	SinkParams.socketBufferSize = networkPipelineSocketBufferSize;
	SinkParams.throttleToRateKpS = static_cast<int64_t>(inNetworkSettings.clientBandwidthLimit);// Assuming 60Hz on the other size. k per sec
	SinkParams.socketBufferSize = inNetworkSettings.clientBufferSize;
	SinkParams.requiredLatencyMs = inNetworkSettings.requiredLatencyMs;
	SinkParams.connectionTimeout = inNetworkSettings.connectionTimeout;

	mPipeline.reset(new avs::Pipeline);

	mNetworkSink.reset(new avs::WebRtcNetworkSink);
	std::vector<avs::NetworkSinkStream> streams;

	// Video
	{
		avs::NetworkSinkStream stream;
		stream.label = "video";
		stream.inputName = COLOR_QUEUE;
		stream.parserType = avs::StreamParserType::AVC_AnnexB;
		stream.chunkSize = 64 * 1024;
		stream.id = 20;
		//stream.dataType = avs::NetworkDataType::HEVC;
		streams.emplace_back(std::move(stream));
	}

	// Tag Data
	{
		avs::NetworkSinkStream stream;
		stream.label = "video_tags";
		stream.inputName = TAG_DATA_QUEUE;
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
		stream.inputName = AUDIO_QUEUE;
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
		stream.inputName = GEOMETRY_QUEUE;
		stream.parserType = avs::StreamParserType::Geometry;
		stream.useParser = true;
		stream.isDataLimitPerFrame = true;
		stream.counter = 0;
		stream.chunkSize = 64 * 1024;
		stream.id = 80;
		stream.dataType = avs::NetworkDataType::Framed;
		streams.emplace_back(std::move(stream));
	}

	// Reliable Commands and responses
	{
		avs::NetworkSinkStream stream;
		stream.label = "reliable";
		stream.inputName = RELIABLE_SEND_QUEUE;
		stream.outputName = RELIABLE_RECEIVE_QUEUE;
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
	// Unreliable messaging
	{
		avs::NetworkSinkStream stream;
		stream.label = "unreliable";
		stream.inputName = UNRELIABLE_SEND_QUEUE;
		stream.outputName = UNRELIABLE_RECEIVE_QUEUE;
		stream.parserType = avs::StreamParserType::None;
		stream.useParser = false;
		stream.isDataLimitPerFrame = true;
		stream.counter = 0;
		stream.chunkSize = 64 * 1024;
		stream.id = 120;
		stream.canReceive = true;
		stream.reliable = false;
		stream.dataType = avs::NetworkDataType::Generic;
		streams.emplace_back(std::move(stream));
	}
	// TODO: connect the other two queues.
	avs::NetworkSink* networkSink = mNetworkSink.get();
	if (!networkSink->configure(std::move(streams), SinkParams))
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
		if (!avs::PipelineNode::link(reliableSendQueue, *mNetworkSink))
		{
			TELEPORT_CERR << "Failed to configure network pipeline for commands!" << "\n";
			initialized = false;
			return;
		}
		mPipeline->add(&reliableSendQueue);
	}
	// Messages
	{
		if (!avs::PipelineNode::link(*mNetworkSink, unreliableReceiveQueue))
		{
			TELEPORT_CERR << "Failed to configure network pipeline for messages!" << "\n";
			initialized = false;
			return;
		}
		mPipeline->add(&unreliableReceiveQueue);
	}
	mPipeline->add(mNetworkSink.get());

#if WITH_TELEPORT_STATS
	mLastTimestamp = avs::Platform::getTimestamp();
#endif // WITH_TELEPORT_STATS

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
	TELEPORT_PROFILE_AUTOZONE;
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
