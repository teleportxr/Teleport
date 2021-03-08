#pragma once

#include "CasterTypes.h"

namespace SCServer
{
	struct CasterSettings
	{
		int32_t requiredLatencyMs;

		const wchar_t* sessionName;
		const wchar_t* clientIP;
		int32_t detectionSphereRadius;
		int32_t detectionSphereBufferDistance;
		int32_t expectedLag;
		int64_t throttleKpS;

		bool enableGeometryStreaming;
		uint8_t geometryTicksPerSecond;
		int32_t geometryBufferCutoffSize; // Size we stop encoding nodes at.
		float confirmationWaitTime; //Seconds to wait before resending a resource.

		bool enableVideoStreaming;
		bool enableWebcamStreaming;
		float captureCubeSize;
		int32_t pipelin;
		bool enableDeferOutput;
		bool enableCubemapCulling;
		int32_t blocksPerCubeFaceAcross; // The number of blocks per cube face will be this value squared
		int32_t cullQuadIndex; // This culls a quad at the index. For debugging only
		int32_t targetFPS;
		int32_t idrInterval;
		avs::VideoCodec videoCodec;
		VideoEncoderRateControlMode rateControlMode;
		int32_t averageBitrate;
		int32_t maxBitrate;
		bool enableAutoBitRate;
		int32_t vbvBufferSizeInFrames;
		bool useAsyncEncoding;
		bool use10BitEncoding;
		bool useYUV444Decoding;
		bool usePerspectiveRendering;
		int32_t sceneCaptureWidth;
		int32_t sceneCaptureHeight;
		float perspectiveFOV;
		bool useDynamicQuality;
		int32_t bandwidthCalculationInterval;

		// Audio
		bool isStreamingAudio;
		bool isReceivingAudio;

		int32_t debugStream;
		bool enableDebugNetworkPackets;
		bool enableDebugControlPackets;
		bool enableChecksums;
		bool willCacheReset;
		bool pipeDllOutputToUnity;
		uint8_t estimatedDecodingFrequency; //An estimate of how frequently the client will decode the packets sent to it; used by throttling.

		bool useCompressedTextures;
		uint8_t qualityLevel;
		uint8_t compressionLevel;

		bool willDisableMainCamera;

		avs::AxesStandard axesStandard = avs::AxesStandard::NotInitialized;

		int32_t specularCubemapSize;
		int32_t specularMips;
		int32_t diffuseCubemapSize;
		int32_t lightCubemapSize;

		bool lockPlayerHeight;
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