#if 1
#pragma once
#include "Platform/CrossPlatform/RenderPlatform.h"
#include <vector>
#include "Platform/CrossPlatform/RenderDelegate.h"
#include "Platform/CrossPlatform/Texture.h"
#include "TeleportClient/OpenXR.h"
#include "TeleportCore/Input.h"

#if defined(DEBUG)
extern void OXR_CheckErrors(XrInstance instance, XrResult result, const char* function, bool failOnError);
#endif

#if defined(DEBUG)
#define OXR(func) OXR_CheckErrors(ovrApp_GetInstance(), func, #func, true);
#else
#define OXR(func) OXR_CheckErrors(ovrApp_GetInstance(), func, #func, false);
#endif

namespace teleport
{
	namespace android
	{
		class OpenXR : public client::OpenXR
		{
		public:
			bool TryInitDevice() override;
		protected:
			const char* GetOpenXRGraphicsAPIExtensionName() const override;
			std::vector<std::string> GetRequiredExtensions() const override;
		};
	}
}
#endif