#pragma once

#include <stdint.h>

namespace SCServer
{
	enum class VideoEncoderRateControlMode : uint8_t
	{
		RC_CONSTQP = 0, /**< Constant QP mode */
		RC_VBR = 1, /**< Variable bitrate mode */
		RC_CBR = 2, /**< Constant bitrate mode */
		RC_CBR_LOWDELAY_HQ = 3, /**< low-delay CBR, high quality */
		RC_CBR_HQ = 4, /**< CBR, high quality (slower) */
		RC_VBR_HQ = 5 /**< VBR, high quality (slower) */
	};

	/*! Graphics API device handle type. */
	enum class GraphicsDeviceType
	{
		Invalid = 0,
		Direct3D11 = 1,
		Direct3D12 = 2,
		OpenGL = 3,
		Vulkan = 4
	};

	/*!
	 * Result type.
	 */
	struct Result
	{
		/*! Result code enumeration. */
		enum Code
		{
			OK = 0,
			UnknownError,
			InvalidGraphicsDevice,
			InvalidGraphicsResource,
			InputSurfaceNodeConfigurationError,
			EncoderNodeConfigurationError,
			PipelineConfigurationError,
			PipelineNotInitialized,
			PipelineProcessingError,
		};

		Result(Code code) : m_code(code)
		{}
		//! if(Result) returns true only if m_code == OK i.e. is ZERO.
		operator bool() const { return m_code == OK; }
		operator Code() const { return m_code; }
	private:
		Code m_code;
	};
}