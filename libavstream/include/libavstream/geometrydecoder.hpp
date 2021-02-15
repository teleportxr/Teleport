// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <libavstream/geometry/mesh_interface.hpp>

namespace avs
{
	/*!
	 * Geometry decoder node `[input-active, output-active, 1/1]`
	 *
	 * Reads packets of encoded geometry and outputs decoded data to a Geometry Target.
	 */
	class AVSTREAM_API GeometryDecoder final : public Node
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
		 * \param device Graphics API device handle (DirectX or OpenGL).
		 * \param frameWidth Expected video frame width in pixels.
		 * \param frameHeight Expected video frame height in pixels.
		 * \param params Additional decoder parameters.
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
		 * \sa Node::process()
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
		Result onInputLink(int slot, Node* node) override;
		Result onOutputLink(int slot, Node* node) override;
		void   onOutputUnlink(int slot, Node* node) override;
	};

} // avs