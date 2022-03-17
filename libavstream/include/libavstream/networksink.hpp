// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

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
		uint32_t bandwidthInterval = 10000;
	};

	/*! Network sink counters. */
	struct NetworkSinkCounters
	{
		/*! Total bytes sent. */
		uint64_t bytesSent = 0;
		/*! Number of sent network packets. */
		uint64_t networkPacketsSent = 0;
		/*! Available bandwidth  */
		double bandwidth = 0;
		/*! Average bandwidth used */
		double avgBandwidthUsed = 0;
		/*! Minimum bandwidth used */
		double minBandwidthUsed = 0;
		/*! Maximum bandwidth used */
		double maxBandwidthUsed = 0;
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
	 * Network sink node `[passive, 0/1]`
	 *
	 * Reads data for each stream from a corresponding avs::Queue input node
	 * , assembles the data into payloads of network packets and sends the data
	 * to the client.
	 * 
	 * If data throttling is enabled for a stream, the seding of data may be spread 
	 * over time to reduce network congestion.
	 */
	class AVSTREAM_API NetworkSink final : public PipelineNode
	{
		AVSTREAM_PUBLICINTERFACE(NetworkSink)

		NetworkSink::Private *m_data;
	public:
		NetworkSink();
		virtual ~NetworkSink();

		/*!
		 * Configure network sink and bind to local UDP endpoint.
		 * \param streams Collection of configurations for each stream. 
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
		 * Send data for all streams to remote UDP endpoint.
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
		NetworkSinkCounters getCounters() const;

		/*!
		* Debug a particular stream.
		*/
		void setDebugStream(uint32_t);
		void setDebugNetworkPackets(bool);
		void setDoChecksums(bool);
		void setEstimatedDecodingFrequency(uint8_t estimatedDecodingFrequency);	
		void setProcessingEnabled(bool enable);
		bool isProcessingEnabled() const;
	};

} // avs