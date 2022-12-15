// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <libavstream/geometry/GeometryParserInterface.h>

namespace avs
{
	/*!
	 * Geometry decoder node `[input-active, output-active, 1/1]`
	 *
	 * Reads packets of encoded geometry and outputs decoded data to a Geometry Target.
	 */
	class AVSTREAM_API GeometryDecoder final : public PipelineNode
	{
		AVSTREAM_PUBLICINTERFACE(GeometryDecoder)
	public:
		/*!
		 * Constructor.
		 * \param backend Decoder backend type to use.
		 */
		explicit GeometryDecoder();

		~GeometryDecoder();

		/*!
		 * Configure decoder.
		 * \param backend The object that performas the actual decoding.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_AlreadyConfigured if decoder was already in configured state.
		 *  - Result::Decoder_NoSuitableBackendFound if there's no usable decoder backend on the system.
		 *  - Any error result returned by DecoderBackendInterface::initialize().
		 */
		Result configure(uint8_t streamId, GeometryDecoderBackendInterface* backend);

		/*!
		 * Deconfigure decoder and release all associated resources.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if decoder was not in configured state.
		 *  - Any error result returned by DecoderBackendInterface::shutdown().
		 */
		Result deconfigure() override;

		uint8_t getStreamId() const;
		/*!
		 * Process as much encoded video data as available on input and decode zero or more frames.
		 * \sa PipelineNode::process()
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if decoder was not in configured state.
		 *  - Result::Node_InvalidInput if no compatible input node is linked to input slot 0 or most recently read input packet is invalid.
		 *  - Result::Node_InvalidOutput if no compatible output node is linked to output slot 0.
		 *  - Result::IO_Empty if no input data is available for decoding.
		 *  - Any error result returned by PacketInterface::readPacket().
		 *  - Any error result returned by DecoderBackendInterface::decode().
		 */
		Result process(uint64_t timestamp, uint64_t deltaTime) override;

		/*!
		 * Set custom decoder backend.
		 * \note Decoder node does not take ownership of the backend instance.
		 * \param backend Custom decoder backend instance.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_AlreadyConfigured if decoder was already in configured state.
		 */
		Result setBackend(GeometryDecoderBackendInterface* backend);

		/*!
		 * Get node display name (for reporting & profiling).
		 */
		const char* getDisplayName() const override { return "Geometry Decoder"; }

	private:
		GeometryDecoderBackendInterface *m_backend=nullptr;
		// non-owned backend
		std::unique_ptr<GeometryParserInterface> m_parser;
		std::vector<uint8_t> m_buffer;
		bool m_configured = false;
		int m_streamId = 0;;
		Result onInputLink(int slot, PipelineNode* node) override;
		Result onOutputLink(int slot, PipelineNode* node) override;
		void   onOutputUnlink(int slot, PipelineNode* node) override;
		Result processPayload(const uint8_t* buffer, size_t bufferSize, GeometryPayloadType payloadType, GeometryTargetInterface *target);
	};

} // avs