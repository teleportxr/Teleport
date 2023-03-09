// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <optional>
#include "networksource.h"

namespace avs
{
	/*!
	 * Network source node `[passive, 0/1]`
	 *
	 * Receives video stream from a remote UDP endpoint.
	 */
	class AVSTREAM_API SrtEfpNetworkSource final : public NetworkSource
	{
		AVSTREAM_PUBLICINTERFACE(SrtEfpNetworkSource)
	public:
		SrtEfpNetworkSource();

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
		const char* getDisplayName() const override { return "SrtEfpNetworkSource"; }

		/*!
		 * Get current counter values.
		 */
		NetworkSourceCounters getCounterValues() const override;

		void setDebugStream(uint32_t);
		void setDoChecksums(bool);
		void setDebugNetworkPackets(bool s);
		size_t getSystemBufferSize() const;
#if IS_CLIENT
		std::queue<HTTPPayloadRequest>& GetHTTPRequestQueue();
#endif

	private:
		Private *m_data; 

		void sendAck(avs::NetworkPacket &packet);
		void asyncReceivePackets();
		void asyncProcessPackets();
		void processPackets();
		void closeSocket();
		void receiveHTTPFile(const char* buffer, size_t bufferSize);
	};

} // avs