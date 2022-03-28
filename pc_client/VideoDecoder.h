// libavstream
// (c) Copyright 2018-2021 Simul Software Ltd

#pragma once

#include <libavstream/decoders/dec_interface.hpp>
#include <memory>
#include <vector>
#include <unordered_map>
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
		bool usedForShortTermRef;
		bool usedForLongTermRef;

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
			markUnusedForReference();
		}

		void markUnusedForReference()
		{
			usedForShortTermRef = false;
			usedForLongTermRef = false;
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
	
	void resetFrames();
	void markFramesUnusedForReference();

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
	std::unordered_map<uint32_t, uint32_t> mPocFrameIndexMap;

	uint32_t mCurrentFrame;
};

