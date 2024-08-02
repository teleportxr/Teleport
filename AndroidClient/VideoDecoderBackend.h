// (C) Copyright 2018 Simul.co

#pragma once

#include <libavstream/decoders/dec_interface.h>
#include "Platform/CrossPlatform/Texture.h"

class DecodeEventInterface
{
public:
	virtual void OnFrameAvailable() = 0;
};

namespace teleport
{
	namespace android
	{
		class NdkVideoDecoder;

		class VideoDecoderBackend final : public avs::DecoderBackendInterface
		{
		public:
			VideoDecoderBackend(platform::crossplatform::RenderPlatform* renderPlatform, platform::crossplatform::Texture* surfaceTexture, DecodeEventInterface* eventInterface);
			virtual ~VideoDecoderBackend();

			/* Begin avs::DecoderBackendInterface */
			avs::Result initialize(const avs::DeviceHandle& device, int frameWidth, int frameHeight, const avs::DecoderParams& params) override;
			avs::Result reconfigure(int frameWidth, int frameHeight, const avs::DecoderParams& params) override;
			avs::Result shutdown() override;
			avs::Result registerSurface(const avs::SurfaceBackendInterface* colorSurface, const avs::SurfaceBackendInterface* alphaSurface = nullptr) override;
			avs::Result unregisterSurface() override;
			avs::Result decode(const void* buffer, size_t bufferSizeInBytes, const void* alphaBuffer, size_t alphaBufferSizeInBytes, avs::VideoPayloadType payaloadType, bool lastPayload) override;
			avs::Result display(bool showAlphaAsColor = false) override;
			/* End avs::DecoderBackendInterface */

			void NotifyFrameAvailable();

			//! Copy incoming video to the main video texture.
			void CopyVideoTexture(platform::crossplatform::GraphicsDeviceContext& deviceContext);
			std::atomic<avs::DecoderStatus>& GetDecoderStatus() { return mDecoderStatus; }

		private:
			void InitializeVideoDecoder(platform::crossplatform::Texture* colorSurfaceTexture, platform::crossplatform::Texture* alphaSurfaceTexture);
			void ShutdownVideoDecoder();

			int mFrameWidth = 0, mFrameHeight = 0;
			bool mUseAlphaLayerDecoding = false;
			bool mInitialized = false;

			platform::crossplatform::RenderPlatform* renderPlatform = nullptr;
			platform::crossplatform::Texture* mColorSurfaceTexture = nullptr;
			//platform::crossplatform::Texture* mAlphaSurfaceTexture=nullptr;
			DecodeEventInterface* mEventInterface = nullptr;

			NdkVideoDecoder* mColorDecoder = nullptr;
			//NdkVideoDecoder *mAlphaDecoder=nullptr;			
			
			std::atomic<avs::DecoderStatus> mDecoderStatus = avs::DecoderStatus::DecoderUnavailable;
		};
	}
}


