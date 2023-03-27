// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <libavstream/memory.hpp>

namespace avs
{
	class AVSTREAM_API GenericTargetInterface 
	{
	public:
		virtual Result decode(const void* buffer, size_t bufferSizeInBytes) = 0;
	};
	/*!
	 * Generic decoder node `[input-active, output-active, 1/1]`
	 *
	 * Reads generic packets    outputs  to a Generic Target.
	 */
	class AVSTREAM_API GenericDecoder final : public PipelineNode
	{
		AVSTREAM_PUBLICINTERFACE(GenericDecoder)
	public:
		//!
		explicit GenericDecoder();

		~GenericDecoder();

		/*!
		 * Configure decoder.
		 * \param backend The object that performas the actual decoding.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_AlreadyConfigured if decoder was already in configured state.
		 *  - Result::Decoder_NoSuitableBackendFound if there's no usable decoder backend on the system.
		 *  - Any error result returned by DecoderBackendInterface::initialize().
		 */
		Result configure(GenericTargetInterface* t);

		/*!
		 * Deconfigure decoder and release all associated resources.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if decoder was not in configured state.
		 *  - Any error result returned by DecoderBackendInterface::shutdown().
		 */
		Result deconfigure() override;

		Result process(uint64_t timestamp, uint64_t deltaTime) override;

		//! Get node display name (for reporting & profiling).
		const char* getDisplayName() const override { return "Generic Decoder"; }

	private:
		GenericTargetInterface*m_target=nullptr;
	
		std::vector<uint8_t> m_buffer;
		bool m_configured = false;
		Result onInputLink(int slot, PipelineNode* node) override;
		Result onOutputLink(int slot, PipelineNode* node) override;
		void   onOutputUnlink(int slot, PipelineNode* node) override;
		Result processPayload(const uint8_t* buffer, size_t bufferSize);
	};

} // avs