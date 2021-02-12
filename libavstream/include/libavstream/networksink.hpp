// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <libavstream/stream/parser_interface.hpp>

namespace avs
{
	/*! Network sink parameters. */
	struct NetworkSinkParams
	{
		/*!
		 * Operating system socket send buffer size.
		 */
		int socketBufferSize = 1024 * 1024;
		uint64_t throttleToRateKpS;
		uint32_t requiredLatencyMs;
		uint32_t connectionTimeout = 5000;
		uint32_t bandwidthInterval = 5000;
		bool calculateStats = true;
	};

	/*! Network sink counters. */
	struct NetworkSinkCounters
	{
		/*! Total bytes sent. */
		uint64_t bytesSent = 0;
		/*! Number of sent network packets. */
		uint64_t networkPacketsSent = 0;
		/*! Average packets sent per second over a user specified interval */
		uint32_t avgPacketsSentPerSec = 0;
		/*! Minimum packets sent per second over all intervals */
		uint32_t minPacketsSentPerSec = 0;
		/*! Maximum packets sent per second over all intervals */
		uint32_t maxPacketsSentPerSec = 0;
		/*! Average bandwidth required per second in the last interval  */
		uint32_t avgRequiredBandwidth = 0;
		/*! Minimum bandwidth required per second over all intervals */
		uint32_t minRequiredBandwidth = 0;
		/*! Maximum bandwidth required per second over all intervals */
		uint32_t maxRequiredBandwidth = 0;
	};

	/*! Network sink stream data. */
	struct NetworkSinkStream
	{
		/*! Stream index */
		uint64_t counter = 0;
		/*! Max size of the buffer */
		size_t chunkSize = 0;
		/*! id */
		uint32_t id = 0;
		/*! Stream parser type */
		StreamParserType parserType;
		/*! Data type */
		NetworkDataType dataType = NetworkDataType::HEVC;
		/* Whether to use a parser */
		bool useParser = false;
		/*! Whether there is a data limit per frame on this stream */
		bool isDataLimitPerFrame = false;
		/*! Buffer of data to be sent */
		std::vector<uint8_t> buffer;

	};
	

	/*!
	 * Network sink node `[passive, 1/0]`
	 *
	 * Sends video stream to a remote UDP endpoint.
	 */
	class AVSTREAM_API NetworkSink final : public Node
	{
		AVSTREAM_PUBLICINTERFACE(NetworkSink)

		NetworkSink::Private *m_data;
	public:
		NetworkSink();
		virtual ~NetworkSink();

		/*!
		 * Configure network sink and bind to local UDP endpoint.
		 * \param numInputs Number of input slots. This determines maximum number of multiplexed streams the node will support.
		 * \param localPort Local UDP endpoint port number.
		 * \param remote Remote UDP endpoint name or IP address.
		 * \param remotePort Remote UDP endpoint port number.
		 * \param params Additional network sink parameters.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_InvalidConfiguration if numInputs, localPort, or remotePort is zero, or if remote is either nullptr or empty string.
		 *  - Result::Network_BindFailed if failed to bind to local UDP socket.
		 */
		Result configure(std::vector<NetworkSinkStream>&& streams, const char* local_bind_addr, uint16_t localPort, const char* remote, uint16_t remotePort, const NetworkSinkParams& params = {});

		/*!
		 * Deconfigure network sink and release all associated resources.
		 * \return Always returns Result::OK.
		 */
		Result deconfigure() override;

		/*!
		 * Send all available video stream data to remote UDP endpoint.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if network sink has not been configured.
		 *  - Result::Network_ResolveFailed if failed to resolve the name of remote UDP endpoint.
		 *  - Result::Network_SendFailed on general network send failure.
		 */
		Result process(uint64_t timestamp, uint64_t deltaTime) override;

		/*!
		 * Get node display name (for reporting & profiling).
		 */
		const char* getDisplayName() const override { return "NetworkSink"; }

		/*!
		 * Get current counter values.
		 */
		NetworkSinkCounters getCounterValues() const;

		float getBandwidthKPerS() const;
		/*!
		* Debug a particular stream.
		*/
		void setDebugStream(uint32_t);
		void setDebugNetworkPackets(bool);
		void setDoChecksums(bool);
		void setEstimatedDecodingFrequency(uint8_t estimatedDecodingFrequency);	
	};

} // avs