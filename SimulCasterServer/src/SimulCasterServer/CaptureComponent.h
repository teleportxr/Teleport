#pragma once

#include "libavstream/common.hpp"

namespace SCServer
{
	struct CasterContext;

	struct CameraInfo
	{
		avs::vec4 orientation = {0, 0, 0, 1};
		avs::vec3 position = {0, 0, 0};
		float fov = 90;
		float width = 1920;
		float height = 1080;
		bool isVR = false;
	};

	class CaptureComponent
	{
	public:
		virtual ~CaptureComponent() = default;

		virtual void startStreaming(SCServer::CasterContext* context) = 0;
		virtual void stopStreaming() = 0;

		virtual void requestKeyframe() = 0;

		virtual SCServer::CameraInfo& getClientCameraInfo() = 0;
	};
}