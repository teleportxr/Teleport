#include "NetworkPipeline.h"

#include <algorithm>
#include <iostream>

#include "ErrorHandling.h"
#include "CasterSettings.h"

namespace
{
	constexpr double networkPipelineStatInterval = 60000000.0; // 1s
	constexpr int networkPipelineSocketBufferSize = 16 * 1024 * 1024; // 16MiB
}

namespace teleport
{
	NetworkPipeline::NetworkPipeline(const CasterSettings* settings)
		: mSettings(settings), mPrevProcResult(avs::Result::OK)
	{
	}

	NetworkPipeline::~NetworkPipeline()
	{
		release();
	}

	void NetworkPipeline::initialise(const CasterNetworkSettings& inNetworkSettings, avs::Queue* videoQueue, avs::Queue* tagDataQueue, avs::Queue* geometryQueue, avs::Queue* audioQueue)
	{
		avs::NetworkSinkParams SinkParams = {};
		SinkParams.socketBufferSize = networkPipelineSocketBufferSize;
		SinkParams.throttleToRateKpS = std::min(mSettings->throttleKpS, static_cast<int64_t>(inNetworkSettings.clientBandwidthLimit));// Assuming 60Hz on the other size. k per sec
		SinkParams.socketBufferSize = inNetworkSettings.clientBufferSize;
		SinkParams.requiredLatencyMs = inNetworkSettings.requiredLatencyMs;
		SinkParams.connectionTimeout = inNetworkSettings.connectionTimeout;

		mPipeline.reset(new avs::Pipeline);
		mNetworkSink.reset(new avs::NetworkSink);


		char remoteIP[20];
		size_t stringLength = wcslen(inNetworkSettings.remoteIP);
		//Convert wide character string to multibyte string.
		wcstombs_s(&stringLength, remoteIP, inNetworkSettings.remoteIP, 20);

		std::vector<avs::NetworkSinkStream> streams;

		// Video
		{
			avs::NetworkSinkStream stream;
			stream.parserType = avs::StreamParserType::AVC_AnnexB;
			stream.useParser = false;
			stream.isDataLimitPerFrame = false;
			stream.counter = 0;
			stream.chunkSize = 64 * 1024;
			stream.id = 20;
			stream.dataType = avs::NetworkDataType::HEVC;
			streams.emplace_back(std::move(stream));
		}

		// Tag Data
		{
			avs::NetworkSinkStream stream;
			stream.parserType = avs::StreamParserType::None;
			stream.useParser = false;
			stream.isDataLimitPerFrame = false;
			stream.counter = 0;
			stream.chunkSize = 200;
			stream.id = 40;
			stream.dataType = avs::NetworkDataType::VideoTagData;
			streams.emplace_back(std::move(stream));
		}

		// Audio
		{
			avs::NetworkSinkStream stream;
			stream.parserType = avs::StreamParserType::Audio;
			stream.useParser = false;
			stream.isDataLimitPerFrame = false;
			stream.counter = 0;
			stream.chunkSize = 2048;
			stream.id = 60;
			stream.dataType = avs::NetworkDataType::Audio;
			streams.emplace_back(std::move(stream));
		}

		// Geometry
		{
			avs::NetworkSinkStream stream;
			stream.parserType = avs::StreamParserType::Geometry;
			stream.useParser = true;
			stream.isDataLimitPerFrame = true;
			stream.counter = 0;
			stream.chunkSize = 64 * 1024;
			stream.id = 80;
			stream.dataType = avs::NetworkDataType::Geometry;
			streams.emplace_back(std::move(stream));
		}

		if (!mNetworkSink->configure(std::move(streams), nullptr, inNetworkSettings.localPort, remoteIP, inNetworkSettings.remotePort, SinkParams))
		{
			TELEPORT_CERR << "Failed to configure network sink!" << std::endl;
			return;
		}

		// Video
		{
			if (!avs::PipelineNode::link(*videoQueue, *mNetworkSink))
			{
				TELEPORT_CERR << "Failed to configure network pipeline for video!" << std::endl;
				return;
			}
			mPipeline->add(videoQueue);
		}

		// Tag Data
		{
			if (!avs::PipelineNode::link(*tagDataQueue, *mNetworkSink))
			{
				TELEPORT_CERR << "Failed to configure network pipeline for video tag data!" << std::endl;
				return;
			}
			mPipeline->add(tagDataQueue);
		}

		// Audio
		{
			if (!avs::PipelineNode::link(*audioQueue, *mNetworkSink))
			{
				TELEPORT_CERR << "Failed to configure network pipeline for audio!" << std::endl;
				return;
			}
			mPipeline->add(audioQueue);
		}
		
		// Geometry
		{
			if (!avs::PipelineNode::link(*geometryQueue, *mNetworkSink))
			{
				TELEPORT_CERR << "Failed to configure network pipeline for geometry!" << std::endl;
				return;
			}
			mPipeline->add(geometryQueue);
		}

		mPipeline->add(mNetworkSink.get());

#if WITH_REMOTEPLAY_STATS
		mLastTimestamp = avs::PlatformWindows::getTimestamp();
#endif // WITH_REMOTEPLAY_STATS

		mPrevProcResult = avs::Result::OK;
	}

	void NetworkPipeline::release()
	{
		mPipeline.reset();
		if (mNetworkSink)
			mNetworkSink->deconfigure();
		mNetworkSink.reset();
	}

	bool NetworkPipeline::process()
	{
		const avs::Result result = mPipeline->process();
		// Prevent spamming of errors from NetworkSink. This happens when there is a connection issue.
		if (!result && result != avs::Result::IO_Empty)
		{
			if (result != mPrevProcResult)
			{
				TELEPORT_CERR << "Network pipeline processing encountered an error!" << std::endl;
				mPrevProcResult = result;
			}
			return false;
		}

		mPrevProcResult = result;

#if 0
		avs::Timestamp timestamp = avs::PlatformWindows::getTimestamp();
		if (avs::PlatformWindows::getTimeElapsed(lastTimestamp, timestamp) >= networkPipelineStatInterval)
		{
			const avs::NetworkSinkCounters counters = mNetworkSink->getCounterValues();
			TELEPORT_COUT << "DP: " << counters.decoderPacketsQueued << " | NP: " << counters.networkPacketsSent << " | BYTES: " << counters.bytesSent << std::endl;
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
			TELEPORT_CERR << "Can't return counters because network sink is null." << std::endl;
			return avs::Result::Node_Null;
		}
		return avs::Result::OK;
	}
}