// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <libavstream/encoders/enc_interface.hpp>

namespace avs
{

/*! Encoder backend type. */
enum class EncoderBackend
{
	Any,    /*!< Any backend (auto-detect during configuration). */
	Custom, /*!< Custom external backend. */
	NVIDIA, /*!< NVIDIA NVENC backend. */
};

/*! Encoder performance stats. */
struct EncoderStats
{
	/*! Number of frames submitted for encoding in the current session. */
	size_t framesSubmitted = 0;
	/*! Number of frames submitted for encoding per second in the current session. */
	float framesSubmittedPerSec = 0;
	/*! Number of frames encoded in the current session. */
	size_t framesEncoded = 0;
	/*! Number of frames encoded per second in the current session. */
	float framesEncodedPerSec = 0;
};

/*!
 * Video encoder node `[input-active, output-active, 1/1]`
 *
 * Encodes video frames from input surface and outputs compressed bitstream.
 * - Compatible inputs : Any node implementing SurfaceInterface.
 * - Compatible outputs: Any node implementing IOInterface.
 */
class AVSTREAM_API Encoder final : public PipelineNode
{
	AVSTREAM_PUBLICINTERFACE(Encoder)
public:
	/*!
	 * Constructor.
	 * \param backend Encoder backend type to use.
	 */
	explicit Encoder(EncoderBackend backend = EncoderBackend::Any);

	/*!
	 * Constructor.
	 * Encoder node takes ownership of the backend instance.
	 * \param backend Custom encoder backend instance.
	 */
	explicit Encoder(EncoderBackendInterface* backend);

	~Encoder();

	/*!
	 * Configure encoder.
	 * \param device Graphics API device handle (DirectX or OpenGL).
	 * \param frameWidth Expected video frame width in pixels.
	 * \param frameHeight Expected video frame height in pixels.
	 * \param params Additional encoder configuration settings.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_AlreadyConfigured if encoder was already in configured state.
	 *  - Result::Encoder_NoSuitableBackendFound if there's no usable encoder backend on the system.
	 *  - Any error result returned by EncoderBackendInterface::initialize(). 
	 */
	Result configure(const DeviceHandle& device, int frameWidth, int frameHeight, const EncoderParams& params);

	/*!
	 * Configure encoder.
	 * \param frameWidth Expected video frame width in pixels.
	 * \param frameHeight Expected video frame height in pixels.
	 * \param params Additional encoder configuration settings.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_NotConfigured if encoder was not in configured state.
	 *  - Any error result returned by EncoderBackendInterface::reconfigure().
	 */
	Result reconfigure(int frameWidth, int frameHeight, const EncoderParams& params);

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
	 * Write data to the output stream.
	 * \param tag data buffer
	 * \param tag data buffer size
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_NotConfigured if encoder was not in configured state.
	 *  - Result::Node_InvalidOutput if no compatible output node is linked to output slot 0.
	 *  - Result::Encoder_IncompleteFrame if encoded bitstream was only partially written to output node.
	 *  - Any error result returned by EncoderBackendInterface::mapOutputBuffer().
	 */
	Result writeOutput();

	/*!
	 * Unregister the encoder's surface texture. 
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
	Result setBackend(EncoderBackendInterface* backend);
	
	/*!
	 * Get node display name (for reporting & profiling).
	 */
	const char* getDisplayName() const override { return "Encoder"; }

	/*!
	 * Tell encoder to encode the next frame as an IDR frame
	 */
	void setForceIDR(bool forceIDR);

	/*!
	 * Returns true if the encoder is encoding asynchronously on a separate thread.
	 */
	bool isEncodingAsynchronously();

	/*!
	 * Gets encoder performance stats for the current session.
	 */
	EncoderStats getStats() const;

	/*!
	 * Gets the encoding capabilities of the GPU.
	 *
	 * \param device Graphics API device handle (DirectX or OpenGL).
	 * \param params Additional encoder configuration settings.
	 * \param capabilities Reference to the capabilities structure that will store the output.
	 * \return
	 *  - Result::OK on success.
	 */
	static Result getEncodeCapabilities(const DeviceHandle& device, const EncoderParams& params, avs::EncodeCapabilities& capabilities);

private:
	Result onInputLink(int slot, PipelineNode* node) override;
	Result onOutputLink(int slot, PipelineNode* node) override;
	void   onInputUnlink(int slot, PipelineNode* node) override;

	/*!
	* Register the encoder's surface texture
	* \param Pointer to surface interface
	* \return
	*  - Result::OK on success.
	*  - Result::Encoder_SurfaceAlreadyRegistered if an input surface is already registered.
	*  - Any error result returned by EncoderBackendInterface::registerSurface().
	*/
	Result registerSurface(SurfaceInterface* surface);

	void writeOutputAsync();
	void StartEncodingThread();
	void StopEncodingThread();
};

} // avs