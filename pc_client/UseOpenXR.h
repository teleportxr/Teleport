#pragma once
#include "Platform/CrossPlatform/RenderPlatform.h"
#include <vector>
#include "Platform/CrossPlatform/RenderDelegate.h"
#include "Platform/CrossPlatform/Texture.h"
#include "TeleportClient/OpenXR.h"
#include "common_maths.h"			// for avs::Pose
#include "common_networking.h"		// for avs::InputState
#include "TeleportCore/Input.h"

namespace teleport
{
	class UseOpenXR: public client::OpenXR
	{
	public:
		bool TryInitDevice() override;
	protected:
		const char *GetOpenXRGraphicsAPIExtensionName() const override;
	};
}
