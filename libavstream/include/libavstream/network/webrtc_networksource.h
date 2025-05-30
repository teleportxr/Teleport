// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <optional>
#include "networksource.h"
#include <unordered_map>
#include <libavstream/httputil.hpp>

namespace avs
{
	/*!
	 * Network source node `[passive, 0/1]`
	 *
	 * Receives video stream from a remote UDP endpoint.
	 */
	class AVSTREAM_API WebRtcNetworkSource final : public NetworkSource
	{
		AVSTREAM_PUBLICINTERFACE(WebRtcNetworkSource)
		Private * m_data = nullptr;
	public:
		WebRtcNetworkSource();
		virtual ~WebRtcNetworkSource();
		/*!
		 * Configure network node and bind to local UDP endpoint.
		 * \param in_streams Collection of configurations for each stream. 
		 * \param numOutputs Number of output slots. This determines maximum number of multiplexed streams the node will support.
		 * \param params Additional network source parameters.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_InvalidConfiguration if numOutputs, localPort, or remotePort is zero, or if remote is either nullptr or empty string.
		 *  - Result::Network_BindFailed if failed to bind to local UDP socket.
		 */
		Result configure(std::vector<NetworkSourceStream>&& in_streams,int numOutputs, const NetworkSourceParams& params) override;
		
		//! Debugging: make it seem as though the connection was lost.
		void kill() override;
		/*!
		 * Deconfigure network source and release all associated resources.
		 * \return Always returns Result::OK.
		 */
		Result deconfigure() override;

		/*!
		 * Send and receive data for all streams to and from remote UDP endpoint.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if this network node has not been configured.
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
		void setDebugStream(uint32_t)override {}
		void setDebugNetworkPackets(bool s)override {}
		size_t getSystemBufferSize() const override {
			return 0;
		}

		void receiveStreamingControlMessage(const std::string& str) override;
		//! IF there is a message to send reliably to the peer, this will fill it in.
		bool getNextStreamingControlMessage(std::string& msg) override;
		Result onInputLink(int slot, PipelineNode* node) override;
		Result onOutputLink(int slot, PipelineNode* node) override;
		
		void sendConfigMessage(const std::string& str) override;

		StreamingConnectionState GetStreamingConnectionState() const override
		{
			return webRtcState;
		}
		void SetStreamingConnectionState(StreamingConnectionState s);
		void resetPeerConnection();
	protected:
		StreamingConnectionState webRtcState=StreamingConnectionState::NEW_UNCONNECTED;
		std::vector<std::string> messagesToSend;
		void receiveHTTPFile(const char* buffer, size_t bufferSize);
		NetworkSourceParams m_params;
		mutable std::mutex m_networkMutex;
		mutable std::mutex m_dataMutex;
		std::vector<char> m_tempBuffer;
		mutable std::mutex messagesMutex;
		void receiveOffer(const std::string& offer);
		void receiveCandidate(const std::string& candidate, const std::string& mid,int mlineindex);
		std::string offer;
		std::vector<IOInterface *> inputInterfaces;
		struct Candidate
		{
			std::string candidate;
			std::string mid;
			int mlineindex;
		};
		std::vector<Candidate> cachedCandidates;
	};

} // avs
