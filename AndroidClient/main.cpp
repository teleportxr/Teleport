#include <iostream>
#include <vector>
#include <filesystem>
#include <sys/prctl.h> // for prctl( PR_SET_NAME )

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
#include "Platform/CrossPlatform/BaseFramebuffer.h"
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
#include "AndroidRenderer.h"
#include "AndroidDiscoveryService.h"

using namespace platform;
using namespace crossplatform;
using namespace platform;
using namespace core;
using namespace teleport;
using namespace android;

int kOverrideWidth = 1440;
int kOverrideHeight = 900;

extern "C" { void android_main(struct android_app* app); }
DisplaySurfaceManager* displaySurfaceManager = nullptr;
teleport::client::ClientDeviceState clientDeviceState;
// Need ONE global instance of this:
avs::Context context;
bool g_WindowQuit;
struct AppState
{
	bool resumed=false;
	ANativeWindow* nativeWindow = nullptr;
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
            std::cout<<"    APP_CMD_START"<<std::endl;
            break;
        }
        case APP_CMD_RESUME: {
            std::cout<<"onResume()"<<std::endl;
            std::cout<<"    APP_CMD_RESUME"<<std::endl;
            appState.resumed = true;
            break;
        }
        case APP_CMD_PAUSE: {
            std::cout<<"onPause()"<<std::endl;
            std::cout<<"    APP_CMD_PAUSE"<<std::endl;
            appState.resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            std::cout<<"onStop()"<<std::endl;
            std::cout<<"    APP_CMD_STOP"<<std::endl;
            break;
        }
        case APP_CMD_DESTROY: {
            std::cout<<"onDestroy()"<<std::endl;
            std::cout<<"    APP_CMD_DESTROY"<<std::endl;
            appState.nativeWindow = NULL;
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            std::cout<<"surfaceCreated()"<<std::endl;
            std::cout<<"    APP_CMD_INIT_WINDOW"<<std::endl;
            appState.nativeWindow = app->window;
		g_WindowQuit = false;
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            std::cout<<"surfaceDestroyed()"<<std::endl;
            std::cout<<"    APP_CMD_TERM_WINDOW"<<std::endl;
            appState.nativeWindow = NULL;
		g_WindowQuit = true;
            break;
        }
    }
}

static bool x = false;

void InitXR(teleport::android::OpenXR &openXR)
{
	if(openXR.TryInitDevice())
	{
		openXR.MakeActions();
		//std::function<void()> showHideDelegate = std::bind(&teleport::Gui::ShowHide, &gui);
		//if (openXR.HaveXRDevice())
		//	openXR.SetMenuButtonHandler(showHideDelegate);
	}
}


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
	
	teleport::android::OpenXR openXR(app->activity->vm,app->activity->clazz);
#if TELEPORT_INTERNAL_CHECKS
	static bool dev_mode=true;
#else
	static bool dev_mode=false;
#endif
	teleport::Gui gui;

	teleport::client::SessionClient *sessionClient=new teleport::client::SessionClient(std::make_unique<android::AndroidDiscoveryService>());
	teleport::android::AndroidRenderer *androidRenderer=new teleport::android::AndroidRenderer (&clientDeviceState, sessionClient,gui,dev_mode);
	
	platform::crossplatform::RenderDelegate renderDelegate = std::bind(&clientrender::Renderer::RenderView, androidRenderer, std::placeholders::_1);
	openXR.InitInstance("Teleport VR Client");
	openXR.InitSystem();
	vulkan::DeviceManager vulkanDeviceManager;
	RedirectStdCoutCerr();
	vulkanDeviceManager.Initialize(true,false,false,openXR.GetRequiredVulkanDeviceExtensions(),openXR.GetRequiredVulkanInstanceExtensions());
	RenderPlatform *renderPlatform = new vulkan::RenderPlatform();

	renderPlatform->PushTexturePath("");
	renderPlatform->PushTexturePath("textures");
	renderPlatform->PushTexturePath("fonts");
	renderPlatform->PushShaderBinaryPath("");
	renderPlatform->PushShaderBinaryPath("shaders");

	renderPlatform->RestoreDeviceObjects(vulkanDeviceManager.GetDevice());
	renderPlatform->SetShaderBuildMode(platform::crossplatform::ShaderBuildMode::NEVER_BUILD);
	androidRenderer->Init(renderPlatform,&openXR,app->window);
    int MainThreadTid = gettid();
	openXR.SetVulkanDeviceAndInstance(vulkanDeviceManager.GetVulkanDevice(),vulkanDeviceManager.GetVulkanInstance(),MainThreadTid,0);
	openXR.Init(renderPlatform);
	InitXR(openXR);
	
	//renderDelegate = std::bind(&RenderView,std::placeholders::_1);

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

		if (openXR.HaveXRDevice())
		{
			openXR.PollEvents(g_WindowQuit);
			openXR.PollActions();
		}
		else
		{
			static char c=0;
			c--;
			if(!c)
				InitXR(openXR);
		}
		static int64_t frame = 0;
		frame++;
		static double time_seconds=GetTimeInSeconds();
		double new_time_seconds=GetTimeInSeconds();
		float time_step_seconds=new_time_seconds-time_seconds;
		time_seconds=new_time_seconds;
		
		androidRenderer->OnFrameMove(float(time_seconds),float(time_step_seconds),openXR.HaveXRDevice());
		renderPlatform->BeginFrame(frame);
		if (openXR.HaveXRDevice())
		{
			platform::crossplatform::GraphicsDeviceContext	deviceContext;
			deviceContext.renderPlatform = renderPlatform;
			// This context is active. So we will use it.
			deviceContext.platform_context = vulkanDeviceManager.GetDeviceContext();
			vec3 originPosition = *((vec3*)&clientDeviceState.originPose.position);
			vec4 originOrientation = *((vec4*)&clientDeviceState.originPose.orientation);
			// Note we do this even when the device is inactive.
			//  if we don't, we will never receive the transition from XR_SESSION_STATE_READY to XR_SESSION_STATE_FOCUSED
			openXR.RenderFrame( renderDelegate, originPosition, originOrientation);
		}
		renderPlatform->EndFrame();
	}
	openXR.Shutdown();
	delete androidRenderer;
	delete sessionClient;
}