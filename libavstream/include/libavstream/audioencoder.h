// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <libavstream/audio/audio_interface.h>

namespace avs
{

/*! Audio encoder backend type. */
enum class AudioEncoderBackend
{
	Any,    /*!< Any backend (auto-detect during configuration). */
	Custom, /*!< Custom external backend. */
};

/*!
 * Audio encoder node `[input-active, output-active, 1/1]`
 *
 * Encodes audio 
 * - Compatible outputs: Any node implementing IOInterface.
 */
class AVSTREAM_API AudioEncoder final : public PipelineNode
{
	AVSTREAM_PUBLICINTERFACE(AudioEncoder)
public:
	/*!
	 * Constructor.
	 * \param backend Encoder backend type to use.
	 */
	explicit AudioEncoder(AudioEncoderBackend backend = AudioEncoderBackend::Any);

	/*!
	 * Constructor.
	 * Encoder node takes ownership of the backend instance.
	 * \param backend Custom encoder backend instance.
	 */
	explicit AudioEncoder(AudioEncoderBackendInterface* backend);

	~AudioEncoder();

	/*!
	 * Configure audio encoder.
	 * \param params Encoder parameters.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_AlreadyConfigured if encoder was already in configured state.
	 *  - Result::Encoder_NoSuitableBackendFound if there's no usable encoder backend on the system.
	 *  - Any error result returned by EncoderBackendInterface::initialize(). 
	 */
	Result configure(const AudioEncoderParams& params);

	/*!
	 * Deconfigure encoder and release all associated resources.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_NotConfigured if encoder was not in configured state.
	 *  - Any error result returned by EncoderBackendInterface::shutdown().
	 */
	Result deconfigure() override;


	/*!
	 * Encode single video frame from input surface and write resulting bitstream to output.
	 * \sa PipelineNode::process()
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_NotConfigured if encoder was not in configured state.
	 *  - Any error result returned by EncoderBackendInterface::encodeFrame().
	 */
	Result process(uint64_t timestamp, uint64_t deltaTime) override;

	/*!
	 * Write data to the output stream
	 * \sa PipelineNode::process()
	 * \param extraDataBuffer
	 * \param bufferSize
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_NotConfigured if encoder was not in configured state.
	 *  - Result::Node_InvalidOutput if no compatible output node is linked to output slot 0.
	 *  - Result::Encoder_IncompleteFrame if encoded bitstream was only partially written to output node.
	 *  - Any error result returned by EncoderBackendInterface::mapOutputBuffer().
	 */
	Result writeOutput(const uint8_t* extraDataBuffer, size_t bufferSize);

	/*!
	 * Unregister the encoder's surface texture 
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_NotConfigured if encoder was not in configured state.
	 *  - Result::Encoder_SurfaceNotRegistered if no input surface is registered.
	 *  - Any error result returned by EncoderBackendInterface::unregisterSurface().
	 */
	Result unregisterSurface();

	/*!
	 * Set custom encoder backend.
	 * Encoder node takes ownership of the backend instance.
	 * \param backend Custom encoder backend instance.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_AlreadyConfigured if encoder was already in configured state.
	 */
	Result setBackend(AudioEncoderBackendInterface* backend);
	
	/*!
	 * Get node display name (for reporting & profiling).
	 */
	const char* getDisplayName() const override { return "AudioEncoder"; }

private:
	Result onInputLink(int slot, PipelineNode* node) override;
	Result onOutputLink(int slot, PipelineNode* node) override;
	void   onInputUnlink(int slot, PipelineNode* node) override;
};

} // avs