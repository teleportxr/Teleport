#include <iostream>
#include <vector>
#include <filesystem>
#include <sys/prctl.h> // for prctl( PR_SET_NAME )
#include <libavstream/httputil.hpp>

enum class Platform { NONE, WIN64, ANDROID };

#define TELEPORT_ANDROID
#include "redist/android/native_app_glue/android_native_app_glue.h"
#include "android/log.h"
#define VK_USE_PLATFORM_ANDROID_KHR
#include "vulkan/vulkan.h"
Platform gPlatform = Platform::ANDROID;

#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/DisplaySurface.h"
#include "Platform/CrossPlatform/DisplaySurfaceManager.h"
#include "Platform/CrossPlatform/HdrRenderer.h"
#include "Platform/CrossPlatform/Framebuffer.h"
#include "FileLoader.h"

#include "Platform/Vulkan/RenderPlatform.h"
#include "Platform/Vulkan/DeviceManager.h"
#include "TeleportClient/Log.h"
#include <unistd.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_oculus.h>
#include <openxr/openxr_oculus_helpers.h>
#include "AndroidOpenXR.h"
#include "TeleportClient/ClientDeviceState.h"
#include "TeleportClient/Config.h"
#include "ClientApp/ClientApp.h"
#include "AndroidRenderer.h"
#include "TeleportClient/DiscoveryService.h"

using namespace platform;
using namespace crossplatform;
using namespace platform;
using namespace platform::core;
using namespace teleport;
using namespace teleport::core;
using namespace android;

int kOverrideWidth = 1440;
int kOverrideHeight = 900;

extern "C" { void android_main(struct android_app* app); }
DisplaySurfaceManager* displaySurfaceManager = nullptr;
teleport::client::ClientApp clientApp;
// Need ONE global instance of this:
avs::Context context;
bool g_WindowQuit=false;
struct AppState
{
	bool resumed=false;
	ANativeWindow* nativeWindow = nullptr;
	clientrender::Gui *gui = nullptr;
};
AppState appState;

void handle_cmd(android_app* app, int32_t cmd)
{
	switch (cmd) {
		// There is no APP_CMD_CREATE. The ANativeActivity creates the
		// application thread from onCreate(). The application thread
		// then calls android_main().
		case APP_CMD_START: {
			std::cout<<"onStart()"<<std::endl;
			std::cout<<"	APP_CMD_START"<<std::endl;
			break;
		}
		case APP_CMD_RESUME: {
			std::cout<<"onResume()"<<std::endl;
			std::cout<<"	APP_CMD_RESUME"<<std::endl;
			appState.resumed = true;
			break;
		}
		case APP_CMD_PAUSE: {
			std::cout<<"onPause()"<<std::endl;
			std::cout<<"	APP_CMD_PAUSE"<<std::endl;
			appState.resumed = false;
			break;
		}
		case APP_CMD_STOP: {
			std::cout<<"onStop()"<<std::endl;
			std::cout<<"	APP_CMD_STOP"<<std::endl;
			break;
		}
		case APP_CMD_DESTROY: {
			std::cout<<"onDestroy()"<<std::endl;
			std::cout<<"	APP_CMD_DESTROY"<<std::endl;
			appState.nativeWindow = nullptr;
			break;
		}
		case APP_CMD_INIT_WINDOW: {
			std::cout<<"surfaceCreated()"<<std::endl;
			std::cout<<"	APP_CMD_INIT_WINDOW"<<std::endl;
			appState.nativeWindow = app->window;
			appState.gui->SetPlatformWindow(app->window);
			g_WindowQuit = false;
			break;
		}
		case APP_CMD_TERM_WINDOW: {
			std::cout<<"surfaceDestroyed()"<<std::endl;
			std::cout<<"	APP_CMD_TERM_WINDOW"<<std::endl;
			//appState.nativeWindow = nullptr;
			//g_WindowQuit = true;
			break;
		}
	}
}

static bool x = false;


double GetTimeInSeconds()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec * 1e9 + now.tv_nsec) * 0.000000001;
}

void android_main(struct android_app* app)
{
	while (x)
	{
		__android_log_write(ANDROID_LOG_INFO, "TeleportVRQuestClientAGDE", "Waiting");
		//sleep(100);
	}
	RedirectStdCoutCerr();
	JNIEnv* Env;
	(*app->activity->vm).AttachCurrentThread(&Env, NULL);
	// Note that AttachCurrentThread will reset the thread name.
	prctl(PR_SET_NAME, (long)"Teleport Main", 0, 0, 0);
	
	teleport::android::OpenXR openXR(app->activity->vm,app->activity->clazz);
	teleport::clientrender::Gui gui(openXR);
	appState.gui=&gui;
	app->onAppCmd = handle_cmd;
	int events;
	android_poll_source* source;
	// wait for the main window:
	while (app->window == nullptr)
	{
		if (ALooper_pollAll(1, nullptr, &events, (void**)&source) >= 0)
		{
			if (source != NULL)
				source->process(app, source);
		}
	} 
	teleport::android::FileLoader androidFileLoader;
	androidFileLoader.SetAndroid_AAssetManager(app->activity->assetManager);
	platform::core::FileLoader::SetFileLoader(&androidFileLoader);
	auto &config=client::Config::GetInstance();
	config.SetStorageFolder(app->activity->internalDataPath);
	{
		void *pem_ptr=nullptr;
		unsigned int pem_bytes=0;
		androidFileLoader.AcquireFileContents(pem_ptr,pem_bytes,"assets/cert/cacert.pem",false);
		std::string cert_path=config.GetStorageFolder()+"/cacert.pem";
		androidFileLoader.Save(pem_ptr,pem_bytes,cert_path.c_str(),false);

		avs::HTTPUtil::SetCertificatePath(cert_path.c_str());
	}
	clientApp.Initialize();
	gui.SetPlatformWindow(app->window);
	
	gui.SetServerIPs(config.recent_server_urls);

	teleport::android::AndroidRenderer *androidRenderer=new teleport::android::AndroidRenderer(gui);
	//if(config.recent_server_urls.size())
	//	client::SessionClient::GetSessionClient(1)->SetServerIP(config.recent_server_urls[0]);
	platform::crossplatform::RenderDelegate renderDelegate = std::bind(&clientrender::Renderer::RenderView, androidRenderer, std::placeholders::_1);
	platform::crossplatform::RenderDelegate overlayDelegate = std::bind(&clientrender::Renderer::DrawOSD, androidRenderer, std::placeholders::_1);

	openXR.InitInstance();
	openXR.InitSystem();
	vulkan::DeviceManager vulkanDeviceManager;
	RedirectStdCoutCerr();
	
	auto device_exts=openXR.GetRequiredVulkanDeviceExtensions();
	RedirectStdCoutCerr();
	device_exts.push_back(VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME);
	device_exts.push_back(VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME);
	device_exts.push_back(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
	device_exts.push_back(VK_KHR_MULTIVIEW_EXTENSION_NAME); 
	
	RedirectStdCoutCerr();
	bool enable_validation=false;
	#ifdef _DEBUG
	enable_validation=true;
	#endif
	vulkanDeviceManager.Initialize(enable_validation,false,false,device_exts,openXR.GetRequiredVulkanInstanceExtensions());
	RenderPlatform *renderPlatform = new vulkan::RenderPlatform();

	renderPlatform->PushTexturePath("");
	renderPlatform->PushTexturePath("textures");
	renderPlatform->PushTexturePath("assets/textures");
	renderPlatform->PushTexturePath("fonts");
	renderPlatform->PushTexturePath("assets/fonts");
	renderPlatform->PushShaderBinaryPath("shaders");
	renderPlatform->PushShaderBinaryPath("assets/shaders");

	// Mark CB's 0 and 1 as being in resource group 0 (per-frame)
	platform::crossplatform::ResourceGroupLayout perFrameLayout;
	perFrameLayout.UseConstantBufferSlot(0);
	perFrameLayout.UseConstantBufferSlot(1);
	renderPlatform->SetResourceGroupLayout(0, perFrameLayout);
	// Mark texture slots 19 to 22 as being in resource group 1 (few per-frame)
	platform::crossplatform::ResourceGroupLayout fewPerFrameLayout;
	fewPerFrameLayout.UseReadOnlyResourceSlot(19);
	fewPerFrameLayout.UseReadOnlyResourceSlot(20);
	fewPerFrameLayout.UseReadOnlyResourceSlot(21);
	fewPerFrameLayout.UseReadOnlyResourceSlot(22);
	renderPlatform->SetResourceGroupLayout(1, fewPerFrameLayout);
	// Mark CB 5 and texture slots 15 to 18 as being in resource group 2 (per material)
	platform::crossplatform::ResourceGroupLayout perMaterialLayout;
	perMaterialLayout.UseConstantBufferSlot(5);
	perMaterialLayout.UseReadOnlyResourceSlot(15);
	perMaterialLayout.UseReadOnlyResourceSlot(16);
	perMaterialLayout.UseReadOnlyResourceSlot(17);
	perMaterialLayout.UseReadOnlyResourceSlot(18);
	renderPlatform->SetResourceGroupLayout(2, perMaterialLayout);

	renderPlatform->RestoreDeviceObjects(vulkanDeviceManager.GetDevice());
	renderPlatform->SetShaderBuildMode(platform::crossplatform::ShaderBuildMode::NEVER_BUILD);
	androidRenderer->Init(renderPlatform,&openXR,app->window);
	int MainThreadTid = gettid();
	openXR.SetVulkanDeviceAndInstance(vulkanDeviceManager.GetVulkanDevice(),vulkanDeviceManager.GetVulkanInstance(),MainThreadTid,0);
	openXR.SetRenderPlatform(renderPlatform);
	
	while (!g_WindowQuit)
	{
	  // Read all pending events.
		for (;;)
		{
			int events;
			struct android_poll_source* source;
			// If the timeout is zero, returns immediately without blocking.
			// If the timeout is negative, waits indefinitely until an event appears.
			const int timeoutMilliseconds =
				(appState.resumed == false && openXR.IsSessionActive()== false &&
				 app->destroyRequested == 0)
				? -1
				: 0;
			if (ALooper_pollAll(timeoutMilliseconds, NULL, &events, (void**)&source) < 0) {
				break;
			}

			// Process this event.
			if (source != NULL)
			{
				source->process(app, source);
			}
		}
		openXR.Tick();
		if(openXR.CanStartSession())
		{
			if(openXR.StartSession())
			{
				openXR.MakeActions();
			}
		}
		static int64_t frame = 0;
		frame++;
		static double time_seconds=GetTimeInSeconds();
		double new_time_seconds=GetTimeInSeconds();
		float time_step_seconds=new_time_seconds-time_seconds;
		time_seconds = new_time_seconds;
		auto microsecondsUTC = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());

		androidRenderer->Update(microsecondsUTC);
		androidRenderer->OnFrameMove(float(time_seconds),float(time_step_seconds));
		renderPlatform->BeginFrame(frame);
		if (openXR.HaveXRDevice())
		{
			platform::crossplatform::GraphicsDeviceContext	deviceContext;
			deviceContext.renderPlatform = renderPlatform;
			// This context is active. So we will use it.
			deviceContext.platform_context = vulkanDeviceManager.GetDeviceContext();
			// Note we do this even when the device is inactive.
			//  if we don't, we will never receive the transition from XR_SESSION_STATE_READY to XR_SESSION_STATE_FOCUSED
			openXR.RenderFrame(renderDelegate, overlayDelegate);
		}
		renderPlatform->EndFrame();
	}
	openXR.Shutdown();
	delete androidRenderer;
	teleport::client::DiscoveryService::ShutdownInstance();
}