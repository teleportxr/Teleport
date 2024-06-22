// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <libavstream/memory.hpp>

namespace avs
{
	class AVSTREAM_API GenericEncoderBackendInterface
	{
	public:
		virtual bool mapOutputBuffer(void*& bufferPtr, size_t& bufferSizeInBytes) = 0;
		virtual void unmapOutputBuffer() = 0;
	};
	class AVSTREAM_API ClientServerMessageStack final:public avs::GenericEncoderBackendInterface
	{
	public:
		void PushBuffer(std::shared_ptr<std::vector<uint8_t>> b);
		bool mapOutputBuffer(void*& bufferPtr, size_t& bufferSizeInBytes) override;
		void unmapOutputBuffer() override;
		std::vector< std::shared_ptr<std::vector<uint8_t>>> buffers;
		std::mutex mutex;
	};
	/*!
	 * Video encoder node `[input-active, output-active, 1/1]`
	 *
	 * Encodes video frames from input surface and outputs compressed bitstream.
	 * - Compatible inputs : Any node implementing SurfaceInterface.
	 * - Compatible outputs: Any node implementing IOInterface.
	 */
	class AVSTREAM_API GenericEncoder final : public PipelineNode
	{
		AVSTREAM_PUBLICINTERFACE(GenericEncoder)
	public:
		/*!
		 * Constructor.
		 */
		explicit GenericEncoder();

		~GenericEncoder();

		/*!
		 * Configure encoder.
		 */
		Result configure(GenericEncoderBackendInterface *backend,const char *name);

		/*!
		 * Deconfigure encoder and release all associated resources.
		 */
		Result deconfigure() override;

		/*!
		 * Encode the current generic stack.
		 * \sa PipelineNode::process()
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
		const char* getDisplayName() const override { return "Generic Encoder"; }

	private:
		Result onInputLink(int slot, PipelineNode* node) override;
		Result onOutputLink(int slot, PipelineNode* node) override;
		void   onInputUnlink(int slot, PipelineNode* node) override;
	};

} // avs