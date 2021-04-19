#pragma once

#include "libavstream/common_networking.h"

#include "CasterTypes.h"

namespace SCServer
{
	struct CasterSettings
	{
		int32_t requiredLatencyMs = 0;

		const wchar_t* sessionName = L"";
		const wchar_t* clientIP = L"";
		int32_t detectionSphereRadius = 0;
		int32_t detectionSphereBufferDistance = 0;
		int32_t expectedLag = 0;
		int64_t throttleKpS = 0;

		bool enableGeometryStreaming = false;
		uint8_t geometryTicksPerSecond = 0;
		int32_t geometryBufferCutoffSize = 0; // Size we stop encoding nodes at.
		float confirmationWaitTime = 0.0f; //Seconds to wait before resending a resource.

		bool enableVideoStreaming = false;
		bool enableWebcamStreaming = false;
		float captureCubeSize = 0.0f;
		int32_t pipelin = 0;
		bool enableDeferOutput = false;
		bool enableCubemapCulling = false;
		int32_t blocksPerCubeFaceAcross = 0; // The number of blocks per cube face will be this value squared
		int32_t cullQuadIndex = 0; // This culls a quad at the index. For debugging only
		int32_t targetFPS = 0;
		int32_t idrInterval = 0;
		avs::VideoCodec videoCodec = avs::VideoCodec::Invalid;
		VideoEncoderRateControlMode rateControlMode = SCServer::VideoEncoderRateControlMode::RC_CONSTQP;
		int32_t averageBitrate = 0;
		int32_t maxBitrate = 0;
		bool enableAutoBitRate = false;
		int32_t vbvBufferSizeInFrames = 0;
		bool useAsyncEncoding = false;
		bool use10BitEncoding = false;
		bool useYUV444Decoding = false;
		bool usePerspectiveRendering = false;
		int32_t perspectiveWidth = 0;
		int32_t perspectiveHeight = 0;
		float perspectiveFOV = 0.0f;
		bool useDynamicQuality = false;
		int32_t bandwidthCalculationInterval = 0;

		// Audio
		bool isStreamingAudio = false;
		bool isReceivingAudio = false;

		int32_t debugStream = 0;
		bool enableDebugNetworkPackets = false;
		bool enableDebugControlPackets = false;
		bool enableChecksums = false;
		bool willCacheReset = false;
		bool pipeDllOutputToUnity = false;
		uint8_t estimatedDecodingFrequency = 0; //An estimate of how frequently the client will decode the packets sent to it; used by throttling.

		bool useCompressedTextures = false;
		uint8_t qualityLevel = 0;
		uint8_t compressionLevel = 0;

		bool willDisableMainCamera = false;

		avs::AxesStandard axesStandard = avs::AxesStandard::NotInitialized;

		int32_t specularCubemapSize = 0;
		int32_t specularMips = 0;
		int32_t diffuseCubemapSize = 0;
		int32_t lightCubemapSize = 0;

		bool lockPlayerHeight = false;
		avs::ControlModel controlModel=avs::ControlModel::CLIENT_ORIGIN_SERVER_GRAVITY;
	};

	struct CasterNetworkSettings
	{
		int32_t localPort;
		const wchar_t* remoteIP;
		int32_t remotePort;
		int32_t clientBandwidthLimit;
		int32_t clientBufferSize;
		int32_t requiredLatencyMs;
		int32_t connectionTimeout;
	};

	struct CasterEncoderSettings
	{
		int32_t frameWidth;
		int32_t frameHeight;
		int32_t depthWidth;
		int32_t depthHeight;
		bool wllWriteDepthTexture;
		bool enableStackDepth;
		bool enableDecomposeCube;
		float maxDepth;
		int32_t specularCubemapSize;
		int32_t roughCubemapSize;
		int32_t diffuseCubemapSize;
		int32_t lightCubemapSize;
	};
}