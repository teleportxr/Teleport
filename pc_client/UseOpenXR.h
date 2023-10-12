#pragma once
#include "Platform/CrossPlatform/RenderPlatform.h"
#include <vector>
#include "Platform/CrossPlatform/RenderDelegate.h"
#include "Platform/CrossPlatform/Texture.h"
#include "TeleportClient/OpenXR.h"
#include "common_maths.h"			// for avs::Pose
#include "TeleportCore/CommonNetworking.h"		// for avs::InputState
#include "TeleportCore/Input.h"

namespace teleport
{
	class UseOpenXR: public client::OpenXR
	{
	public:
		UseOpenXR(const char* txt):OpenXR(txt)
		{}

		bool StartSession() override;
		void SetCurrentFrameDeviceContext(platform::crossplatform::GraphicsDeviceContext* d)
		{
			deviceContext = d;
		}

		std::set<std::string> GetRequiredExtensions() const override;
	protected:
		const char *GetOpenXRGraphicsAPIExtensionName() const override;
		platform::crossplatform::MultiviewGraphicsDeviceContext& GetDeviceContext(size_t, size_t) override
		{
			return *((platform::crossplatform::MultiviewGraphicsDeviceContext*)deviceContext);
		}
		platform::crossplatform::GraphicsDeviceContext* deviceContext = nullptr;
	};
}
