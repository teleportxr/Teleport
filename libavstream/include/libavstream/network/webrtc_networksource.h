// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <optional>
#include "networksource.h"
#include <unordered_map>
#if IS_CLIENT
#include <libavstream/httputil.hpp>
#endif

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
	class WebRtcNetworkSource;
	/*!
	 * Network source node `[passive, 0/1]`
	 *
	 * Receives video stream from a remote UDP endpoint.
	 */
	class AVSTREAM_API WebRtcNetworkSource final : public NetworkSource
	{
		AVSTREAM_PUBLICINTERFACE(WebRtcNetworkSource)
		WebRtcNetworkSource::Private * m_data = nullptr;
	public:
		WebRtcNetworkSource();

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
		Result configure(std::vector<NetworkSourceStream>&& streams, const NetworkSourceParams& params) override;

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
		Result process(uint64_t timestamp, uint64_t deltaTime) override;

		/*!
		 * Get node display name (for reporting & profiling).
		 */
		const char* getDisplayName() const override { return "WebRtcNetworkSource"; }

		/*!
		 * Get current counter values.
		 */
		NetworkSourceCounters getCounterValues() const override;


		// std::function
		void OnAnswerCreated(webrtc::SessionDescriptionInterface* desc);
#if IS_CLIENT
		std::queue<HTTPPayloadRequest>& GetHTTPRequestQueue();
#endif

		void setDebugStream(uint32_t)override {}
		void setDoChecksums(bool)override {}
		void setDebugNetworkPackets(bool s)override {}
		size_t getSystemBufferSize() const override {
			return 0;
		}
	protected:
		class CreateSessionDescriptionObserver* create_session_description_observer = nullptr;
		class SetSessionDescriptionObserver* set_session_description_observer = nullptr;
		std::vector<NetworkSourceStream> m_streams;
		void receiveHTTPFile(const char* buffer, size_t bufferSize);
#if IS_CLIENT
		HTTPUtil m_httpUtil;
#endif
		std::unordered_map<uint32_t, int> m_streamNodeMap;
		NetworkSourceParams m_params;
		NetworkSourceCounters m_counters;
		mutable std::mutex m_networkMutex;
		mutable std::mutex m_dataMutex;
	};

} // avs