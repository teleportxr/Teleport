// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/memory.hpp>
#include <libavstream/surfaces/surface_interface.hpp>

namespace avs
{

enum class RateControlMode 
{
	RC_CONSTQP = 0, /**< Constant QP mode */
	RC_VBR = 1, /**< Variable bitrate mode */
	RC_CBR = 2 /**< Constant bitrate mode */
};

/*!
 * Video encoder parameters.
 */
struct EncoderParams
{
	/*! Video codec. */
	VideoCodec codec = VideoCodec::Any;
	/*! Encoder preset. */
	VideoPreset preset = VideoPreset::Default;
	/*! Pereferred input format. */
	SurfaceFormat inputFormat = SurfaceFormat::Unknown;
	/*! Target frame rate in frames per second. */
	uint32_t targetFrameRate = 60;
	/*! How often should frames be encoded as IDR picture type (0 is disabled). */
	uint32_t idrInterval = 60;
	/*! The rate control algorithm used by the encoder. */
	RateControlMode rateControlMode = RateControlMode::RC_CBR;
	/*! Average encoded stream bitrate (0 is automatic). */
	uint32_t averageBitrate = 0;
	/*! Maximum encoded stream bitrate (0 is automatic). */
	uint32_t maxBitrate = 0;
	/*! If true, average and max bit rates are calculated automatically. */
	bool autoBitRate = true;
	/*! Size of the vbv buffer in frames. Smaller values reduce latency but affect performance. */
	uint32_t vbvBufferSizeInFrames = 2;
	/*! If true, output is delayed until next Encoder::process() call; this improves pipelining at the expense of additional latency. */
	bool deferOutput = true;
	/*! Near clipping plane for depth remapping for encoding non-linear depth values from R16 sources. */
	float depthRemapNear = 0.0f;
	/*! Far clipping plane for depth remapping for encoding non-linear depth values from R16 sources (0 disables remapping). */
	float depthRemapFar  = 0.0f;
	/*! If true, configures the encoder to encode asynchronously. */
	bool useAsyncEncoding = true;
	/*! If true, configures the encoder to do 10 bit encoding. */
	bool use10BitEncoding = true;
	/*! If using a YUV input format, the decoder will use chroma format YUV444 if true and YUV420 if false.*/
	bool useYUV444ChromaFormat = false;
	/*! Determines if alpha of texture is encoded in a separate alpha layer. Only supported on HEVC with modern GPUs. */
	bool useAlphaLayerEncoding = true;
};

struct EncodeCapabilities
{
	uint32_t maxEncodeLevel = 0;
	uint32_t isAsyncCapable = 0;
	uint32_t is10BitCapable = 0;
	uint32_t isYUV444Capable = 0;
	uint32_t isAlphaLayerSupported = 0;
};

/*!
 * Common encoder backend interface.
 *
 * Encoder backend is responsible for encoding pictures using a particular hardware encoder.
 */
class EncoderBackendInterface : public UseInternalAllocator
{
public:
	virtual ~EncoderBackendInterface() = default;

	/*!
	 * Initialize hardware encoder.
	 * Encoder backend must be succesfully initialized before it's ready to encode pictures.
	 * \param device Graphics API device handle (DirectX or OpenGL).
	 * \param frameWidth Expected video frame width in pixels.
	 * \param frameHeight Expected video frame height in pixels.
	 * \param params Additional encoder parameters.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::EncoderBackend_InvalidDevice if passed device handle is invalid or otherwise unsuitable for this particular encoder.
	 *  - Result::EncoderBackend_NoSuitableCodecFound if the hardware encoder does not support encoding with chosen video codec or parameters.
	 *  - Result::EncoderBackend_OutOfMemory if the encoder failed to allocate internal resources.
	 *  - Result::EncoderBackend_InitFailed on general initialization failure.
	 */
	virtual Result initialize(const DeviceHandle& device, int frameWidth, int frameHeight, const EncoderParams& params) = 0;

	/*!
	 * Reconfigure hardware encoder.
	 * Encoder backend must be succesfully initialized before this function can be called.
	 * \param frameWidth Expected video frame width in pixels.
	 * \param frameHeight Expected video frame height in pixels.
	 * \param params Additional encoder parameters.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::EncoderBackend_NotInitialized if the encoder is not initialized
	 *  - Result::EncoderBackend_NoSuitableCodecFound if the hardware encoder does not support encoding with chosen video codec or parameters.
	 *  - Result::EncoderBackend_OutOfMemory if the encoder failed to allocate internal resources.
	 *  - Result::EncoderBackend_ReconfigFailed on general initialization failure.
	 */
	virtual Result reconfigure(int frameWidth, int frameHeight, const EncoderParams& params) = 0;

	/*!
	 * Shutdown hardware encoder and release all associated resources.
	 * After encoder backend is shutdown it must be initialized again to be ready to encode pictures.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::EncoderBackend_FlushError if failed to flush hardware encoder before shutting down.
	 *  - Result::EncoderBackend_ShutdownFailed on general failure.
	 */
	virtual Result shutdown() = 0;

	/*!
	 * Register surface as source for pictures (video frames) to encode.
	 * The surface must be compatible with the graphics API device handle passed to initialize().
	 * \note Encoder backend does not take ownership of the registered surface.
	 * \param surface Surface backend to register as input.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::EncoderBackend_NotInitialized if encoder backend has not yet been initialized.
	 *  - Result::EnocderBackend_SurfaceAlreadyRegistered if surface backend has already been registered with this encoder backend.
	 *  - Result::EncoderBackend_InvalidSurface if passed surface backend is invalid or otherwise unsuitable for this encoder.
	 *  - Result::EncoderBackend_InvalidDevice if the device this encoder backend has been initialized with is unsuitable for registering surfaces.
	 */
	virtual Result registerSurface(const SurfaceBackendInterface* surface) = 0;

	/*!
	 * Unregister surface as source for pictures (video frames) to encode.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::EncoderBackend_NotInitialized if encoder backend has not yet been initialized.
	 *  - Result::EncoderBackend_SurfaceNotRegistered if no surface backend has been registered with this encoder backend.
	 *  - Result::EncoderBackend_InvalidSurface if passed surface backend is invalid.
	 */
	virtual Result unregisterSurface() = 0;

	/*!
	 * Encode single picture (video frame).
	 * \param timestamp Abstract timestamp value to associate with encoded picture.
	 * \param forceIDR Tells the encoder encode thje next frame as an IDR frame.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::EncoderBackend_NotInitialized if encoder backend has not yet been initialized.
	 *  - Result::EncoderBackend_SurfaceNotRegistered if no surface backend has been registered with this encoder backend.
	 *  - Result::EncoderBackend_MapFailed if failed to map input surface prior to encoding.
	 *  - Result::EncoderBackend_UnmapFailed if failed to unmap input surface after encoding.
	 *  - Result::EncoderBackend_EncodeFailed on general encode failure.
	 */
	virtual Result encodeFrame(uint64_t timestamp, bool forceIDR = false) = 0;

	/*!
	 * Map output buffer to the host address space (for access by CPU).
	 * \param[out] bufferPtr Pointer to the compressed video bitstream of previously encoded frame.
	 * \param[out] bufferSizeInBytes Number of bytes in the output buffer.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::EncoderBackend_NotInitialized if encoder backend has not yet been initialized.
	 *  - Result::EncoderBackend_MapFailed if failed to map output buffer.
	 */
	virtual Result mapOutputBuffer(void*& bufferPtr, size_t& bufferSizeInBytes) = 0;

	/*!
	 * Unmap previously mapped output buffer.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::EncoderBackend_NotInitialized if encoder backend has not yet been initialized.
	 *  - Result::EncoderBackend_UnmapFailed if failed to unmap output buffer.
	 */
	virtual Result unmapOutputBuffer() = 0;

	/*!
	 * Get registered input surface format.
	 */
	virtual SurfaceFormat getInputFormat() const = 0;
};

} // avs