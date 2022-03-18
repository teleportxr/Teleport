// libavstream
// (c) Copyright 2018-2021 Simul Software Ltd

#pragma once

#include <libavstream/decoders/dec_interface.hpp>
#include <memory>
#include <vector>
#include "Platform/Crossplatform/BaseRenderer.h"
#include "Platform/Crossplatform/VideoDecoder.h"
#include "Platform/Crossplatform/RenderPlatform.h"
#include "AVParser/H264Types.h"
#include "AVParser/HevcTypes.h"

namespace simul
{
	namespace crossplatform
	{
		class Texture;
	}	
}
namespace avparser
{
	class Parser;
}

class VideoDecoder final : public avs::DecoderBackendInterface
{
	struct FrameCache
	{
		uint32_t poc;
		// Related to short term reference picture.
		uint32_t stRpsIdx;
		uint32_t refRpsIdx;
		avparser::hevc::SliceType sliceType;
		bool inUse;

		FrameCache()
		{
			reset();
		}

		void reset()
		{
			poc = 0;
			stRpsIdx = 0;
			refRpsIdx = 0;
			sliceType = avparser::hevc::SliceType::None;
			inUse = false;
		}
	};

public:
	VideoDecoder(simul::crossplatform::RenderPlatform* renderPlatform, simul::crossplatform::Texture* surfaceTexture);
	~VideoDecoder();

	/* Begin DecoderBackendInterface */
	avs::Result initialize(const avs::DeviceHandle& device, int frameWidth, int frameHeight, const avs::DecoderParams& params) override;
	avs::Result reconfigure(int frameWidth, int frameHeight, const avs::DecoderParams& params) override;
	avs::Result shutdown() override;
	avs::Result registerSurface(const avs::SurfaceBackendInterface* surface, const avs::SurfaceBackendInterface* alphaSurface = nullptr) override;
	avs::Result unregisterSurface() override;

	avs::Result decode(const void* buffer, size_t bufferSizeInBytes, const void* alphaBuffer, size_t alphaBufferSizeInBytes, avs::VideoPayloadType payloadType, bool lastPayload) override;
	avs::Result display(bool showAlphaAsColor = false) override;
	/* End DecoderBackendInterface */

	void recompileShaders();

private:
	void updatePicParams();
	void updatePicParamsH264();
	void updatePicParamsHEVC();
	static uint32_t computeHevcPoc(const avparser::hevc::SPS* sps, uint32_t prevPocTid0, uint32_t pocLsb, uint32_t nalUnitType);
	void resetFrames();

	avs::DeviceType mDeviceType = avs::DeviceType::Invalid;

	avs::DecoderParams mParams = {};

	unsigned int mFrameWidth = 0;
	unsigned int mFrameHeight = 0;
	int mDisplayPictureIndex = -1;

	std::unique_ptr<avparser::Parser> mParser;

	std::unique_ptr<simul::crossplatform::VideoDecoder> mDecoder;

	simul::crossplatform::VideoDecodeArgument mPicParams;
	simul::crossplatform::RenderPlatform* mRenderPlatform;
	simul::crossplatform::Texture* mSurfaceTexture;
	simul::crossplatform::Texture* mOutputTexture;
	simul::crossplatform::Effect* mTextureConversionEffect;

	std::vector<FrameCache> mDPB;

	uint32_t mCurrentFrame;
	uint32_t mPrevPocTid0;
};

