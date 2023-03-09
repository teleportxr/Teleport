#pragma once
#define ABSL_USES_STD_OPTIONAL 0
// (c) Copyright 2018-2023 Simul Software Ltd

#include "networksink.h"
#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <unordered_map>
#include <libavstream/stream/parser_interface.hpp>
#include <api/scoped_refptr.h>

namespace webrtc
{
	class DataChannelInterface;
	class IceCandidateInterface;
	struct DataBuffer;
	class SessionDescriptionInterface;
	class PeerConnectionInterface;
	class DataChannelInterface;
}

namespace avs
{
	struct WebRtcNetworkInternal;
	class WebRtcNetworkSink;
	struct DataChannel
	{
		DataChannel(uint64_t stream_index=0, WebRtcNetworkSink* webRtcNetworkSink=nullptr);
		~DataChannel();
		class DataChannelObserver* data_channel_observer = nullptr;
		rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_interface;
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
	class AVSTREAM_API WebRtcNetworkSink final : public NetworkSink
	{
		AVSTREAM_PUBLICINTERFACE(WebRtcNetworkSink)
		WebRtcNetworkInternal* priv = nullptr;
	public:
		WebRtcNetworkSink();
		virtual ~WebRtcNetworkSink();

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

		//! Get node display name (for reporting & profiling).
		const char* getDisplayName() const override { return "WebRtcNetworkSink"; }

		NetworkSinkCounters getCounters() const;
		void setProcessingEnabled(bool enable);
		bool isProcessingEnabled() const;

		// std::function targets
		void OnDataChannelMessage(uint64_t data_stream_index, const webrtc::DataBuffer& buffer);
	protected:
		Result packData(const uint8_t* buffer, size_t bufferSize, uint32_t inputNodeIndex);
		std::vector<NetworkSinkStream> m_streams;
		std::unordered_map<uint32_t, std::unique_ptr<StreamParserInterface>> m_parsers;
		NetworkSinkCounters m_counters;
		mutable std::mutex m_countersMutex;
		bool enabled = true;
		class PeerConnectionObserver * peer_connection_observer=nullptr;
		class CreateSessionDescriptionObserver *create_session_description_observer = nullptr;
		class SetSessionDescriptionObserver* set_session_description_observer = nullptr;

		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection;

		// map from the stream indices to the channels.
		std::unordered_map<uint64_t, DataChannel> dataChannels;
		void OnDataChannelCreated(webrtc::DataChannelInterface* channel);
		void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
		void OnAnswerCreated(webrtc::SessionDescriptionInterface* desc);
	};

} // avs