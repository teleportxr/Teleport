#if 1
#pragma once
#include "Platform/CrossPlatform/RenderPlatform.h"
#include <vector>
#include "Platform/CrossPlatform/RenderDelegate.h"
#include "Platform/CrossPlatform/Texture.h"
#include "TeleportClient/OpenXR.h"
#include "TeleportCore/Input.h"
#include "redist/andriod/native_app_glue/android_native_app_glue.h"
#include <vulkan/vulkan.hpp>
#include "CmdBuffer.h"

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
		extern std::string vkResultString(VkResult res);
		class OpenXR : public client::OpenXR
		{
		public:
			OpenXR(JavaVM *vm,jobject clazz);
			bool InitSystem();
			void SetVulkanDeviceAndInstance(vk::Device *vkDevice,vk::Instance *vkInstance,int MainThreadTid,int RenderThreadTid);
			bool TryInitDevice() override;
			std::vector<std::string> GetRequiredVulkanDeviceExtensions() const;
			std::vector<std::string> GetRequiredVulkanInstanceExtensions() const;
		protected:
			const char* GetOpenXRGraphicsAPIExtensionName() const override;
			std::vector<std::string> GetRequiredExtensions() const override;
			void HandleSessionStateChanges( XrSessionState state) override;
			platform::crossplatform::GraphicsDeviceContext& GetDeviceContext(int i) override;
			void FinishDeviceContext(int i) override;
			void EndFrame() override;
			vk::Device *vulkanDevice=nullptr;
			//vk::PhysicalDevice *vulkanPhysicalDevice=nullptr;
			vk::Instance *vulkanInstance=nullptr;
			CmdBuffer cmdBuffer{};
			vk::Queue vulkanQueue;
#define VULKAN_FRAME_LATENCY 2
			vk::Fence fences[VULKAN_FRAME_LATENCY+1];
			// These threads will be marked as performance threads.
			int MainThreadTid=0;
			int RenderThreadTid=0;
			XrSystemSpaceWarpPropertiesFB spaceWarpProperties = {};
		};
	}
}
#endif