// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <optional>

namespace avs
{
	struct NetworkPacket;
	class NetworkPacketMap;

	/*! Network source stream data. */
	struct NetworkSourceStream
	{
		/*! id */
		uint32_t id = 0;
	};

	/*! Network source parameters. */
	struct NetworkSourceParams
	{
		/*!
		 * Fragment garbage collector time-to-live.
		 * Time in ns after which fragments of incomplete decoder packets are considered stale and are removed from incoming fragment map.
		 *
		 * \note Settings this to twice the ideal frame time seems to produce the best results.
		 */
		uint32_t gcTTL = (1000*1000) / 60 * 20;

		/*!
		 * Nominal jitter buffer length in ms.
		 */
		uint32_t nominalJitterBufferLength = 0;

		/*!
		 * Max jitter buffer length in ms.
		 */
		uint32_t maxJitterBufferLength = 0;

		/*!
		 * Operating system socket recv buffer size.
		 
		 */
		int socketBufferSize = 4*1024 * 1024;

		int32_t requiredLatencyMs=0;
	};

	/*! Network source counters. */
	struct NetworkSourceCounters
	{
		/*! Total bytes received. */
		uint64_t bytesReceived = 0;
		/*! Number of received network packets. */
		uint64_t networkPacketsReceived = 0;
		/*! Number of successfully assembled decoder packets. */
		uint64_t decoderPacketsReceived = 0;
		/*! Number of network packets dropped due to GC timeout. */
		uint64_t networkPacketsDropped = 0;
		/*! Number of decoder packets dropped due to GC timeout. */
		uint64_t decoderPacketsDropped = 0;
		/*! Number of incomplete decoder packets received. */
		uint64_t incompleteDecoderPacketsReceived = 0;
		/*! Current length of jitter buffer in ms. */
		uint32_t jitterBufferLength = 0;
		/*! Number of jitter buffer push calls this frame. */
		uint32_t jitterBufferPush = 0;
		/*! Number of jitter buffer pop calls this frame. */
		uint32_t jitterBufferPop = 0;
		//! Number of NetworkPackets unprocessed at the start of polling.
		uint32_t m_packetMapOrphans = 0;
		uint32_t m_maxAge = 0;
		float decoderDropped = 0.0f;
		float networkDropped = 0.0f;

		float bandwidthKPS = 0.0f;
	};

	/*!
	 * Network source node `[passive, 0/1]`
	 *
	 * Receives video stream from a remote UDP endpoint.
	 */
	class AVSTREAM_API NetworkSource final : public Node
	{
		AVSTREAM_PUBLICINTERFACE(NetworkSource)
	public:
		NetworkSource();

		/*!
		 * Configure network source and bind to local UDP endpoint.
		 * \param numOutputs Number of output slots. This determines maximum number of multiplexed streams the node will support.
		 * \param localPort Local UDP endpoint port number.
		 * \param remote Remote UDP endpoint name or IP address.
		 * \param remotePort Remote UDP endpoint port number.
		 * \param params Additional network source parameters.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_InvalidConfiguration if numOutputs, localPort, or remotePort is zero, or if remote is either nullptr or empty string.
		 *  - Result::Network_BindFailed if failed to bind to local UDP socket.
		 */
		Result configure(std::vector<NetworkSourceStream>&& streams, uint16_t localPort, const char* remote, uint16_t remotePort, const NetworkSourceParams& params = {});

		/*!
		 * Deconfigure network source and release all associated resources.
		 * \return Always returns Result::OK.
		 */
		Result deconfigure() override;

		/*!
		 * Receive and process incoming network packets.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if network source has not been configured.
		 *  - Result::Network_ResolveFailed if failed to resolve the name of remote UDP endpoint.
		 *  - Result::Network_RecvFailed on general network receive failure.
		 */
		Result process(uint32_t timestamp) override;

		/*!
		 * Get node display name (for reporting & profiling).
		 */
		const char* getDisplayName() const override { return "NetworkSource"; }

		/*!
		 * Get current counter values.
		 */
		NetworkSourceCounters getCounterValues() const;

		void setDebugStream(uint32_t);
		void setDoChecksums(bool);
		void setDebugNetworkPackets(bool s);
		size_t getSystemBufferSize() const;

	private:
		Private *m_data; 

		//remove
		//ElasticFrameType2 m_type2Frame;
		//size_t m_size;

		void sendAck(avs::NetworkPacket &packet);
		void accumulatePackets();
		void asyncRecvPackets();
		void closeSocket();
		void pollData();
	};

} // avs