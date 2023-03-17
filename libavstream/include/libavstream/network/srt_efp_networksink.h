#pragma once
// (c) Copyright 2018-2023 Simul Software Ltd

#include "networksink.h"
#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <libavstream/stream/parser_interface.hpp>

namespace avs
{
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
	class AVSTREAM_API SrtEfpNetworkSink final : public NetworkSink
	{
		AVSTREAM_PUBLICINTERFACE(SrtEfpNetworkSink)

		SrtEfpNetworkSink::Private * m_data;
	public:
		SrtEfpNetworkSink();
		virtual ~SrtEfpNetworkSink();

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
		Result configure(std::vector<NetworkSinkStream>&& streams, const char* local_bind_addr, uint16_t localPort, const char* remote, uint16_t remotePort, const NetworkSinkParams& params = {}) override;

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

		//! Get node display name (for reporting & profiling).
		const char* getDisplayName() const override { return "SrtEfpNetworkSink"; }

		//! Get current counter values.
		NetworkSinkCounters getCounters() const override;

		//! Debug a particular stream.
		void setDebugStream(uint32_t);
		void setDebugNetworkPackets(bool);
		void setDoChecksums(bool);
		void setEstimatedDecodingFrequency(uint8_t estimatedDecodingFrequency);
		void setProcessingEnabled(bool enable) override;
		bool isProcessingEnabled() const override;

	protected:
		Result packData(const uint8_t* buffer, size_t bufferSize, uint32_t inputNodeIndex);
		void sendData(const std::vector<uint8_t>& subPacket);
		void closeConnection();
		void updateCounters(uint64_t timestamp, uint32_t deltaTime);
	public:
		void sendOrCacheData(const std::vector<uint8_t>& subPacket);
	};

} // avs