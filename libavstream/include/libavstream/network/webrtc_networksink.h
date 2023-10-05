#pragma once
#define ABSL_USES_STD_OPTIONAL 0
// (c) Copyright 2018-2023 Simul Software Ltd

#include "networksink.h"
#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <unordered_map>
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
	class AVSTREAM_API WebRtcNetworkSink final : public NetworkSink
	{
		AVSTREAM_PUBLICINTERFACE(WebRtcNetworkSink)
		Private* m_data = nullptr;
	public:
		WebRtcNetworkSink();
		virtual ~WebRtcNetworkSink();
		virtual StreamingConnectionState getConnectionState() const override;

		/*!
		 * Configure network node and bind to local UDP endpoint.
		 * \param streams Collection of configurations for each stream.
		 * \param local_bind_addr Address this endpoint is bound to.
		 * \param localPort Local UDP endpoint port number.
		 * \param remote Remote UDP endpoint name or IP address.
		 * \param remotePort Remote UDP endpoint port number.
		 * \param params Additional network sink parameters.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_InvalidConfiguration if numInputs, localPort, or remotePort is zero, or if remote is either nullptr or empty string.
		 *  - Result::Network_BindFailed if failed to bind to local UDP socket.
		 */
		Result configure(std::vector<NetworkSinkStream>&& streams,  const NetworkSinkParams& params = {}) override;

		/*!
		 * Deconfigure network sink and release all associated resources.
		 * \return Always returns Result::OK.
		 */
		Result deconfigure() override;

		/*!
		 * Send and receive data for all streams to and from remote UDP endpoint.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if this network node has not been configured.
		 *  - Result::Network_ResolveFailed if failed to resolve the name of remote UDP endpoint.
		 *  - Result::Network_SendFailed on general network send failure.
		 */
		Result process(uint64_t timestamp, uint64_t deltaTime) override;


		/*!
		 * Get node display name (for reporting & profiling).
		 */
		const char* getDisplayName() const override { return "WebRtcNetworkSink"; }

		/*!
		 * Get current counter values.
		 */
		NetworkSinkCounters getCounters() const override;
		void setProcessingEnabled(bool enable) override;
		bool isProcessingEnabled() const override;

		//! IF there is a message to send reliably to the peer, this will fill it in.
		bool getNextStreamingControlMessage(std::string &msg) override;
		void receiveStreamingControlMessage(const std::string&) override;

		// std::function targets
		void OnDataChannelStateChange(uint64_t data_stream_index);
		void OnDataChannelBufferedAmountChange(uint64_t data_stream_index, uint64_t previous);

		void SendConfigMessage(const std::string& str);

		Result onInputLink(int slot, PipelineNode* node) override;
		Result onOutputLink(int slot, PipelineNode* node) override;
	protected:
		void CreatePeerConnection();
		Result packData(const uint8_t* buffer, size_t bufferSize, uint32_t inputNodeIndex) override;
		Result sendEfpData(const std::vector<uint8_t>& subPacket);
		Result sendData(uint8_t id, const uint8_t* packet, size_t sz);
		std::vector<NetworkSinkStream> m_streams;
		NetworkSinkCounters m_counters;
		mutable std::mutex m_countersMutex;
		bool enabled = true;

		// map from the stream indices to the channels.
		void receiveAnswer(const std::string& answer);
		void receiveCandidate(const std::string& candidate, const std::string& mid,int mlineindex);
		std::vector<std::string> messagesToSend;
		mutable std::mutex messagesMutex;
	//	std::unordered_map<uint32_t, int> m_streamNodeMap;
		NetworkSinkParams  m_params;
	};

} // avs
