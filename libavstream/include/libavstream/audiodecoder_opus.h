// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <libavstream/audio/audio_interface.h>

namespace avs
{
	/*!
	 * Opus Audio Decoder node `[input-active, output-active, 1/1]`
	 *
	 * Decodes Opus RTP frames (raw Opus payloads, post-depacketization) to
	 * 48 kHz int16 PCM mono. The node reads Opus packets from an input queue
	 * and pushes decoded PCM to an Audio Target.
	 *
	 * Thread-safe for frame input on one thread and processing on another.
	 */
	class AVSTREAM_API OpusAudioDecoder final : public PipelineNode
	{
		AVSTREAM_PUBLICINTERFACE(OpusAudioDecoder)
	public:
		/*!
		 * Constructor.
		 */
		explicit OpusAudioDecoder();

		~OpusAudioDecoder();

		/*!
		 * Configure decoder.
		 * \param channels - number of channels (1 for mono, 2 for stereo, etc.)
		 * \param sampleRate - sample rate in Hz (typically 48000)
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_AlreadyConfigured if decoder was already in configured state.
		 */
		Result configure(int channels, int sampleRate);

		/*!
		 * Deconfigure decoder and release all associated resources.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if decoder was not in configured state.
		 */
		Result deconfigure() override;

		/*!
		 * Submit a raw Opus frame for decoding. Thread-safe.
		 * \param opusData - pointer to Opus-encoded data
		 * \param opusDataSize - size in bytes
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if not yet configured.
		 */
		Result submitOpusFrame(const uint8_t* opusData, size_t opusDataSize);

		/*!
		 * Process decoder: consume Opus frames and output PCM.
		 * \sa PipelineNode::process()
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if decoder was not in configured state.
		 *  - Result::Node_InvalidOutput if no compatible output node is linked.
		 *  - Result::IO_Empty if no Opus frames are queued.
		 */
		Result process(uint64_t timestamp, uint64_t deltaTime) override;

		/*!
		 * Get node display name (for reporting & profiling).
		 */
		const char* getDisplayName() const override { return "Opus Audio Decoder"; }

	private:
		Result onInputLink(int slot, PipelineNode* node) override;
		Result onOutputLink(int slot, PipelineNode* node) override;
		void onOutputUnlink(int slot, PipelineNode* node) override;
	};

} // avs
