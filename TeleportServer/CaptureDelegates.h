#pragma once

#include <functional>

#include "libavstream/common_maths.h"

namespace teleport
{
	namespace server
	{
		struct ClientNetworkContext;

		struct CameraInfo
		{
			vec4 orientation = { 0, 0, 0, 1 };
			vec3 position = { 0, 0, 0 };
			float fov = 90;
			float width = 1920;
			float height = 1080;
			bool isVR = false;
		};

		struct CaptureDelegates
		{
			std::function<void(ClientNetworkContext* context)> startStreaming;

			std::function<void(void)> requestKeyframe;

			std::function<CameraInfo& (void)> getClientCameraInfo;
		};
	}
}