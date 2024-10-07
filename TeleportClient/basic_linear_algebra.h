// (C) Copyright 2018-2024 Simul Software Ltd
#pragma once
#include <cmath>

#include "libavstream/common_maths.h"

#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "Platform/CrossPlatform/Quaterniond.h"
#include "TeleportCore/CommonNetworking.h"

// TODO: Replace all this with Platform's maths.
namespace teleport
{
	namespace clientrender
	{
		// CONSTANTS
		constexpr float PI = 3.1415926535f;
		constexpr float DEG_TO_RAD = PI / 180.f;
		constexpr float RAD_TO_DEG = 180.f / PI;

		// DEFINITIONS
		using quat=platform::crossplatform::Quaternionf;

		/** Returns vertical FOV. FOV values in radians. */
		inline float GetVerticalFOVFromHorizontal(float horizontal, float aspect)
		{
			return 2 * std::atanf(tanf(horizontal * 0.5f) * aspect);
		}

		inline float GetVerticalFOVFromHorizontalInDegrees(float horizontal, float aspect)
		{
			return GetVerticalFOVFromHorizontal(horizontal * DEG_TO_RAD, aspect) * RAD_TO_DEG;
		}
		inline vec3 LocalToGlobal(const teleport::core::Pose &pose, const vec3 &local)
		{
			vec3 ret = pose.position;
			platform::crossplatform::Quaternionf *q = (platform::crossplatform::Quaternionf *)&pose.orientation;
			ret += q->RotateVector(local);
			return ret;
		}
	}
}