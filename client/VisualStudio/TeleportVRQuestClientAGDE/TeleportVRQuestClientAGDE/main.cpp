#include <iostream>
#include <vector>
#include <filesystem>

enum class Platform { NONE, WIN64, ANDROID };

#define TELEPORT_ANDROID
#include "redist/andriod/native_app_glue/android_native_app_glue.h"
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

using namespace platform;
using namespace crossplatform;
using namespace platform;
using namespace core;

int kOverrideWidth = 1440;
int kOverrideHeight = 900;

class VulkanTester : public crossplatform::PlatformRendererInterface
{
public:
	VulkanTester(cp_hwnd window)
	{
		m_Window = window;
		Init();
	}
	~VulkanTester()
	{
		DeInit();
	}

	void Init()
	{
		//Set up GDI, RP and DSM
		m_GraphicsDeviceInterface = &m_Vulkan_DeviceManager;
		m_GraphicsDeviceInterface->Initialize(true, false, false);

		m_RenderPlatform = new vulkan::RenderPlatform();
		m_RenderPlatform->SetShaderBuildMode(platform::crossplatform::ShaderBuildMode::NEVER_BUILD);
		if (gPlatform == Platform::WIN64)
		{
			std::string cmake_source_dir = "../../../../firstparty/Platform";
			std::string cmake_binary_dir = "../../../../firstparty/Platform/build";
			m_RenderPlatform->PushTexturePath((cmake_source_dir + "/Resources/Textures").c_str());
			m_RenderPlatform->PushShaderBinaryPath(((cmake_binary_dir + "/") + m_RenderPlatform->GetPathName() + "/shaderbin").c_str());
		}
		else
		{
			m_RenderPlatform->PushTexturePath("Resources/Textures");
			m_RenderPlatform->PushShaderBinaryPath("shaderbin");
		}
		m_RenderPlatform->RestoreDeviceObjects(m_GraphicsDeviceInterface->GetDevice());

		m_DisplaySurfaceManager.Initialize(m_RenderPlatform);
		m_DisplaySurfaceManager.SetRenderer(m_Window, this, -1);

		//Set up HdrRenderer and Depth texture
		m_DepthTexture = m_RenderPlatform->CreateTexture("Depth-Stencil"); //Calls new
		m_HdrRenderer = new crossplatform::HdrRenderer();
		m_HdrRenderer->RestoreDeviceObjects(m_RenderPlatform);

		//Set up BaseFramebuffer
		m_HdrFramebuffer = m_RenderPlatform->CreateFramebuffer(); //Calls new
		m_HdrFramebuffer->SetFormat(crossplatform::RGBA_16_FLOAT);
		m_HdrFramebuffer->SetDepthFormat(crossplatform::D_32_FLOAT);
		m_HdrFramebuffer->SetAntialiasing(1);
		m_HdrFramebuffer->DefaultClearColour = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		m_HdrFramebuffer->DefaultClearDepth = m_ReverseDepth ? 0.0f : 1.0f;
		m_HdrFramebuffer->DefaultClearStencil = 0;
		m_HdrFramebuffer->RestoreDeviceObjects(m_RenderPlatform);

		vec3 look = { 0.0f, 1.0f, 0.0f }, up = { 0.0f, 0.0f, 1.0f };
		m_Camera.LookInDirection(look, up);
		m_Camera.SetHorizontalFieldOfViewDegrees(90.0f);
		m_Camera.SetVerticalFieldOfViewDegrees(0.0f);// Automatic vertical fov - depends on window shape

		crossplatform::CameraViewStruct vs;
		vs.exposure = 1.0f;
		vs.gamma = 0.44f;
		vs.projection = m_ReverseDepth ? crossplatform::DEPTH_REVERSE : crossplatform::FORWARD;
		vs.nearZ = 0.1f;
		vs.farZ = 300000.f;
		vs.InfiniteFarPlane = true;
		m_Camera.SetCameraViewStruct(vs);
	}

	void DeInit()
	{
		delete m_HdrFramebuffer;
		delete m_HdrRenderer;
		delete m_DepthTexture;

		m_DisplaySurfaceManager.RemoveWindow(m_Window);
		m_DisplaySurfaceManager.Shutdown();
		delete m_RenderPlatform;
		m_GraphicsDeviceInterface->Shutdown();
	}

	int AddView() override
	{
		static int last_view_id = 0;
		return last_view_id++;
	};
	void RemoveView(int id) override {};
	void ResizeView(int view_id, int W, int H) override {};
	void Render(int view_id, void* pContext, void* renderTexture, int w, int h, long long frame, void* context_allocator = nullptr) override
	{
		// Device context structure
		platform::crossplatform::GraphicsDeviceContext	deviceContext;

		// Store back buffer, depth buffer and viewport information
		deviceContext.defaultTargetsAndViewport.num = 1;
		deviceContext.defaultTargetsAndViewport.m_rt[0] = renderTexture;
		deviceContext.defaultTargetsAndViewport.rtFormats[0] = crossplatform::UNKNOWN; //To be later defined in the pipeline
		deviceContext.defaultTargetsAndViewport.m_dt = nullptr;
		deviceContext.defaultTargetsAndViewport.depthFormat = crossplatform::UNKNOWN;
		deviceContext.defaultTargetsAndViewport.viewport = int4(0, 0, w, h);
		deviceContext.platform_context = pContext;
		deviceContext.renderPlatform = m_RenderPlatform;
		deviceContext.viewStruct.view_id = view_id;
		deviceContext.viewStruct.depthTextureStyle = crossplatform::PROJECTION;
		{
			deviceContext.viewStruct.view = m_Camera.MakeViewMatrix();
			float aspect = (float)kOverrideWidth / (float)kOverrideHeight;
			if (m_ReverseDepth)
			{
				deviceContext.viewStruct.proj = m_Camera.MakeDepthReversedProjectionMatrix(aspect);
			}
			else
			{
				deviceContext.viewStruct.proj = m_Camera.MakeProjectionMatrix(aspect);
			}
			deviceContext.viewStruct.Init();
		}

		//Begin frame
		m_RenderPlatform->BeginFrame(m_Framenumber);
		m_HdrFramebuffer->SetWidthAndHeight(w, h);
		m_HdrFramebuffer->Activate(deviceContext);

		m_HdrFramebuffer->Clear(deviceContext, 0.00f, 0.31f, 0.57f, 1.00f, m_ReverseDepth ? 0.0f : 1.0f);
		
		m_HdrFramebuffer->Deactivate(deviceContext);
		m_HdrRenderer->Render(deviceContext, m_HdrFramebuffer->GetTexture(), 1.0f, 0.44f);
		m_RenderPlatform->EndFrame();
		m_Framenumber++;
	}

private:
	cp_hwnd m_Window = nullptr;
	bool m_ReverseDepth = true;
	int m_Framenumber = 0;

	vulkan::DeviceManager m_Vulkan_DeviceManager;
	GraphicsDeviceInterface* m_GraphicsDeviceInterface = nullptr;
	RenderPlatform* m_RenderPlatform = nullptr;
	DisplaySurfaceManager m_DisplaySurfaceManager;

	crossplatform::Texture* m_DepthTexture = nullptr;
	crossplatform::HdrRenderer* m_HdrRenderer = nullptr;
	crossplatform::BaseFramebuffer* m_HdrFramebuffer = nullptr;

	crossplatform::Camera m_Camera;

public:
	DisplaySurfaceManager* GetDisplaySurfaceManager() { return &m_DisplaySurfaceManager; }
};

extern "C" { void android_main(struct android_app* app); }
DisplaySurfaceManager* displaySurfaceManager = nullptr;
teleport::client::ClientDeviceState clientDeviceState;
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

void RenderView(platform::crossplatform::GraphicsDeviceContext &deviceContext)
{

}

#include <sys/prctl.h> // for prctl( PR_SET_NAME )
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
	/*while (window == nullptr) {
		if (ALooper_pollAll(1, nullptr, &events, (void**)&source) >= 0)
		{
			if (source != NULL)
				source->process(app, source);
		}
	}*/
	teleport::android::FileLoader androidFileLoader;
	androidFileLoader.SetAndroid_AAssetManager(app->activity->assetManager);
	platform::core::FileLoader::SetFileLoader(&androidFileLoader);
	
	teleport::android::OpenXR openXR(app->activity->vm,app->activity->clazz);
	openXR.InitInstance("Teleport VR Client");
	openXR.InitSystem();
	vulkan::DeviceManager vulkanDeviceManager;
	RedirectStdCoutCerr();
	vulkanDeviceManager.Initialize(true,false,false,openXR.GetRequiredVulkanDeviceExtensions(),openXR.GetRequiredVulkanInstanceExtensions());
	RenderPlatform *renderPlatform = new vulkan::RenderPlatform();
	renderPlatform->RestoreDeviceObjects(vulkanDeviceManager.GetDevice());
	renderPlatform->SetShaderBuildMode(platform::crossplatform::ShaderBuildMode::NEVER_BUILD);
    int MainThreadTid = gettid();
	openXR.SetVulkanDeviceAndInstance(vulkanDeviceManager.GetVulkanDevice(),vulkanDeviceManager.GetVulkanInstance(),MainThreadTid,0);
	openXR.Init(renderPlatform);
	InitXR(openXR);
	//VulkanTester vt(window);
	//displaySurfaceManager = vt.GetDisplaySurfaceManager();
	
	platform::crossplatform::RenderDelegate renderDelegate;
	renderDelegate = std::bind(&RenderView,std::placeholders::_1);

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
	}
	openXR.Shutdown();
}