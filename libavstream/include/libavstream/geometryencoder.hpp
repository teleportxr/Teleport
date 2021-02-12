// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>

namespace avs
{
	/*!
	 * Video encoder node `[input-active, output-active, 1/1]`
	 *
	 * Encodes video frames from input surface and outputs compressed bitstream.
	 * - Compatible inputs : Any node implementing SurfaceInterface.
	 * - Compatible outputs: Any node implementing IOInterface.
	 */
	class AVSTREAM_API GeometryEncoder final : public Node
	{
		AVSTREAM_PUBLICINTERFACE(GeometryEncoder)
	public:
		/*!
		 * Constructor.
		 */
		explicit GeometryEncoder();

		~GeometryEncoder();

		/*!
		 * Configure encoder.
		 */
		Result configure(GeometryEncoderBackendInterface *backend);

		/*!
		 * Deconfigure encoder and release all associated resources.
		 */
		Result deconfigure() override;

		/*!
		 * Encode the current geometry stack.
		 * \sa Node::process()
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if encoder was not in configured state.
		 *  - Result::Node_InvalidInput if no compatible input node is linked to input slot 0.
		 *  - Result::Node_InvalidOutput if no compatible output node is linked to output slot 0.
		 *  - Result::Encoder_IncompleteFrame if encoded bitstream was only partially written to output node.
		 */
		Result process(uint64_t timestamp, uint64_t deltaTime) override;

		/*!
		 * Get node display name (for reporting & profiling).
		 */
		const char* getDisplayName() const override { return "Geometry Encoder"; }

	private:
		Result onInputLink(int slot, Node* node) override;
		Result onOutputLink(int slot, Node* node) override;
		void   onInputUnlink(int slot, Node* node) override;
	};

} // avs