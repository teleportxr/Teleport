#include <vector>
#include <dlfcn.h>
#include <jni.h>
#include <sys/resource.h>
#include <android/log.h>
#include <android/sensor.h>
#include <android/window.h>
#include <android/native_app_glue/android_native_app_glue.h>
#include "vulkan_wrapper.h"

#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END }

EXTERN_C_BEGIN;

//Forward Declarations
ANativeWindow* gp_AndroidWindow = nullptr;
bool g_CloseWindow = false;
static void handle_cmd(android_app* app, int32_t cmd)
{
	switch (cmd) {
	case APP_CMD_INIT_WINDOW:
		gp_AndroidWindow = app->window;
		g_CloseWindow = false;
		break;
	case APP_CMD_TERM_WINDOW:
		g_CloseWindow = true;
		break;
	default:
		__android_log_write(ANDROID_LOG_INFO, "TeleportVRQuestClient", "Event not handled");
	}
}

void android_main(struct android_app* app) 
{
	app->onAppCmd = handle_cmd;
	int events;
	android_poll_source* source;
	while (gp_AndroidWindow == nullptr) {
		if (ALooper_pollAll(1, nullptr, &events, (void**)&source) >= 0)
		{
			if (source != nullptr)
				source->process(app, source);
		}
	}
	if (!InitVulkan())
		return;

	VkApplicationInfo m_AI;
	m_AI.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	m_AI.pNext = nullptr;
	m_AI.pApplicationName = "TeleportVRQuestClient";
	m_AI.applicationVersion = 1;
	m_AI.pEngineName = "TeleportVRQuestClient";
	m_AI.engineVersion = 1;
	m_AI.apiVersion = VK_MAKE_VERSION(1, 0, 0);

	std::vector<const char*> m_ActiveInstanceLayers = {};
	std::vector<const char*> m_ActiveInstanceExtensions = {"VK_KHR_surface", "VK_KHR_android_surface"};

	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> instanceLayerProperties;
	instanceLayerProperties.resize(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, instanceLayerProperties.data());

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> instanceExtensionProperties;
	instanceExtensionProperties.resize(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, instanceExtensionProperties.data());

	for (auto& ext : instanceExtensionProperties)
	{
		const char* str = ext.extensionName;
		__android_log_write(ANDROID_LOG_INFO, "TeleportVRQuestClient", str);
	}

	VkInstanceCreateInfo m_InstanceCI;
	m_InstanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	m_InstanceCI.pNext = nullptr;
	m_InstanceCI.flags = 0;
	m_InstanceCI.pApplicationInfo = &m_AI;
	m_InstanceCI.enabledLayerCount = static_cast<uint32_t>(m_ActiveInstanceLayers.size());
	m_InstanceCI.ppEnabledLayerNames = m_ActiveInstanceLayers.data();
	m_InstanceCI.enabledExtensionCount = static_cast<uint32_t>(m_ActiveInstanceExtensions.size());
	m_InstanceCI.ppEnabledExtensionNames = m_ActiveInstanceExtensions.data();

	VkInstance m_Instance;
	VkResult res = vkCreateInstance(&m_InstanceCI, nullptr, &m_Instance);

	vkDestroyInstance(m_Instance, nullptr);

	while (!g_CloseWindow)
	{
		if (ALooper_pollAll(1, nullptr, &events, (void**)&source) >= 0)
		{
			if (source != nullptr)
				source->process(app, source);
		}
	}
}

EXTERN_C_END;