#include <iostream>
#include <vector>
#include <filesystem>

enum class Platform { NONE, WIN64, ANDROID };

#if defined(_WIN64)
#define TELEPORT_WIN64
#define VK_USE_PLATFORM_WIN32_KHR
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "vulkan/vulkan.h"
Platform gPlatform = Platform::WIN64;


#elif defined(__ANDROID__)
#define TELEPORT_ANDROID
#include "../redist/andriod/native_app_glue/android_native_app_glue.h"
#include "android/log.h"
#define VK_USE_PLATFORM_ANDROID_KHR
#include "vulkan/vulkan.h"
Platform gPlatform = Platform::ANDROID;

#else
#error Unknown Platform.
#endif

#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/DisplaySurface.h"
#include "Platform/CrossPlatform/DisplaySurfaceManager.h"
#include "Platform/CrossPlatform/HdrRenderer.h"
#include "Platform/CrossPlatform/BaseFramebuffer.h"
#include "Platform/Core/DefaultFileLoader.h"

#include "Platform/Vulkan/RenderPlatform.h"
#include "Platform/Vulkan/DeviceManager.h"
#include <unistd.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_oculus.h>
#include <openxr/openxr_oculus_helpers.h>

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

#if defined(TELEPORT_WIN64)
HWND hWnd = 0;
DisplaySurfaceManager* displaySurfaceManager = nullptr;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_PAINT:
		if (displaySurfaceManager)
		{
			displaySurfaceManager->Render(hWnd);
			displaySurfaceManager->EndFrame();
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

int main(int argc, char** argv)
{
	// Initialize the Window class:
	wchar_t wszWindowClass[] = L"TeleportVRQuestClientAGDE - x64";
	HINSTANCE hInstance = 0;

	WNDCLASSEXW wcex;
	wcex.cbSize = sizeof(WNDCLASSEXW);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = 0;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = NULL;
	wcex.lpszMenuName = 0;
	wcex.lpszClassName = wszWindowClass;
	wcex.hIconSm = 0;
	RegisterClassExW(&wcex);
	
	// Create the window:
	hWnd = CreateWindowW(wszWindowClass, wszWindowClass, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, kOverrideWidth, kOverrideHeight, NULL, NULL, hInstance, NULL);
	if (!hWnd)
		return 0;
	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	VulkanTester vt(hWnd);
	displaySurfaceManager = vt.GetDisplaySurfaceManager();

	//Main WIN32 MSG loop
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		InvalidateRect(hWnd, NULL, TRUE);
	}
}
#else
extern "C" { void android_main(struct android_app* app); }
ANativeWindow* window = nullptr;
DisplaySurfaceManager* displaySurfaceManager = nullptr;
bool g_WindowQuit;

void handle_cmd(android_app* app, int32_t cmd)
{
	switch (cmd) 
	{
	case APP_CMD_INIT_WINDOW:
		window = app->window;
		g_WindowQuit = false;
		break;
	case APP_CMD_TERM_WINDOW:
		g_WindowQuit = true;
		break;
	default:
		__android_log_write(ANDROID_LOG_INFO, "TeleportVRQuestClientAGDE", "Event not handled");
	}
}
static bool x = true;
void android_main(struct android_app* app)
{
	while (x)
	{
		__android_log_write(ANDROID_LOG_INFO, "TeleportVRQuestClientAGDE", "Waiting");
		//sleep(100);
	}
	app->onAppCmd = handle_cmd;
	int events;
	android_poll_source* source;
	while (window == nullptr) {
		if (ALooper_pollAll(1, nullptr, &events, (void**)&source) >= 0)
		{
			if (source != NULL)
				source->process(app, source);
		}
	}
	platform::core::DefaultFileLoader::SetAndroid_AAssetManager(app->activity->assetManager);

	VulkanTester vt(window);
	displaySurfaceManager = vt.GetDisplaySurfaceManager();

	while (!g_WindowQuit)
	{
		if (ALooper_pollAll(1, nullptr, &events, (void**)&source) >= 0)
		{
			if (source != NULL)
				source->process(app, source);

			displaySurfaceManager->Render(window);
			displaySurfaceManager->EndFrame();
		}
	}
}
#endif