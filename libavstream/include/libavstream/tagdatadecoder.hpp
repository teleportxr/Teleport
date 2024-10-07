// libavstream
// (c) Copyright 2018-2021 Teleport XR Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <functional>

namespace avs
{

	struct TagDataDecoderStats
	{
		size_t framesReceived = 0;
		float framesReceivedPerSec = 0;
	};

	/*!
	 * Video tag data decoder node `[input-active, 1/0]`
	 *
	 * Reads packets of encoded video tag data and outputs result to the application.
	 * - Compatible inputs: A queue of taga data.
	 */
	class AVSTREAM_API TagDataDecoder final : public PipelineNode
	{
		AVSTREAM_PUBLICINTERFACE(TagDataDecoder)
	public:
		/*!
		 * Constructor.
		 * \param backend TagDataDecoder backend type to use.
		 */
		TagDataDecoder();

		~TagDataDecoder();

		/*!
		 * Configure decoder.
		 * \param streamId Which stream.
		 * \param onReceiveDataCallback callback for extracting extra video data
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_AlreadyConfigured if decoder was already in configured state.
		 */
		Result configure(uint8_t streamId, std::function<void(const uint8_t* data, size_t dataSize)> onReceiveDataCallback);


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
		 * Process received video tag data
		 * \sa PipelineNode::process()
		 * \param timestamp When this is happening.
		 * \param deltaTime Time since the last call.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if decoder was not in configured state.
		 *  - Result::Node_InvalidInput if no compatible input node is linked to input slot 0 or most recently read input packet is invalid.
		 *  - Result::Node_InvalidOutput if no compatible output node is linked to output slot 0.
		 *  - Result::IO_Empty if no input data is available for decoding.
		 */
		Result process(uint64_t timestamp, uint64_t deltaTime) override;

		TagDataDecoderStats GetStats() const
		{
			return m_stats;
		}

		/*!
		 * Get node display name (for reporting & profiling).
		 */
		const char* getDisplayName() const override { return "TagDataDecoder"; }

	private:
		Result onInputLink(int slot, PipelineNode* node) override;
		Result onOutputLink(int slot, PipelineNode* node) override;
		void onOutputUnlink(int slot, PipelineNode* node) override;
		
		TagDataDecoderStats m_stats = {};
		std::vector<uint8_t> m_frameBuffer;
		bool m_configured = false;
		std::function<void(const uint8_t * data, size_t dataSize)> m_onReceiveDataCallback;
	};

} // avs