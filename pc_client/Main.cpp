#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "targetver.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#endif
// Windows Header Files:
#include <windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <filesystem>
//#include <Shlobj.h>
#include <Shlobj_core.h>
#include "Resource.h"
#include "Platform/Core/EnvironmentVariables.h"
#include "Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/DisplaySurfaceManager.h"
#include "Platform/CrossPlatform/DisplaySurface.h"
#include "Platform/Core/Timer.h"

#include "ClientRender/Renderer.h"
#include "TeleportCore/ErrorHandling.h"
#include "Config.h"
#ifdef _MSC_VER
#include "Platform/Windows/VisualStudioDebugOutput.h"
#include "TeleportClient/ClientDeviceState.h"
#include "TeleportClient/DiscoveryService.h"
#include "ClientApp/ClientApp.h"
VisualStudioDebugOutput debug_buffer(true,nullptr, 128);
#endif

#if TELEPORT_CLIENT_USE_D3D12
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/DirectX12/DeviceManager.h"
platform::dx12::DeviceManager deviceManager;
#else
#include "Platform/DirectX11/RenderPlatform.h"
#include "Platform/DirectX11/DeviceManager.h"
platform::dx11::DeviceManager deviceManager;
#endif
#include "UseOpenXR.h"
#include "Platform/CrossPlatform/GpuProfiler.cpp"

using namespace teleport;

clientrender::Renderer *clientRenderer=nullptr;
teleport::client::SessionClient *sessionClient=nullptr;
platform::crossplatform::RenderDelegate renderDelegate;
platform::crossplatform::RenderDelegate overlayDelegate;
UseOpenXR useOpenXR("Teleport PC Client");
Gui gui(useOpenXR);
platform::crossplatform::GraphicsDeviceInterface *gdi = nullptr;
platform::crossplatform::DisplaySurfaceManagerInterface *dsmi = nullptr;
platform::crossplatform::RenderPlatform *renderPlatform = nullptr;

platform::crossplatform::DisplaySurfaceManager displaySurfaceManager;
client::ClientApp clientApp;
std::string teleport_path;
std::string storage_folder;
// Need ONE global instance of this:
avs::Context context;

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szWindowClass[]=L"MainWindow";            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
HWND               InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void InitRenderer(HWND,bool,bool);
void ShutdownRenderer(HWND);
#include "Platform/Core/FileLoader.h"
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
	
	auto *fileLoader=platform::core::FileLoader::GetFileLoader();
	fileLoader->SetRecordFilesLoaded(true);
	// Needed for asynchronous device creation in XAudio2
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED); 
	if (FAILED(hr))
	{
		TELEPORT_CERR << "CoInitialize failed. Exiting." << std::endl;
		return 0;
	}
	// run from pc_client directory.
	if(!std::filesystem::exists("assets/client.ini"))
	{
		std::filesystem::path current_path=std::filesystem::current_path();
		wchar_t filename[700];
		DWORD res=GetModuleFileNameW(nullptr,filename,700);
		if(res)
		{
			current_path=filename;
			current_path=current_path.remove_filename();
		}
		while(!current_path.empty()&&!std::filesystem::exists("assets/client.ini"))
		{
			auto prev_path=current_path;
			std::string rel_pc_client="../../pc_client";
			current_path=current_path.append(rel_pc_client).lexically_normal();
			if(prev_path==current_path)
				break;
			if(std::filesystem::exists(current_path))
				std::filesystem::current_path(current_path);
		}
	}
	auto &config=client::Config::GetInstance();
	// Get a folder we can write to:
	
	char szPath[MAX_PATH];

	HRESULT hResult = SHGetFolderPathA(NULL,
		CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE,
		NULL,
		0,
		szPath);
	if(hResult==S_OK)
	{
		storage_folder = std::string(szPath)+"/TeleportVR";
	}
	
	config.SetStorageFolder(storage_folder.c_str());
	clientApp.Initialize();
	gui.SetServerIPs(config.recent_server_urls);
	if (config.log_filename.size() > 0)
	{
		size_t pos = config.log_filename.find(':');
		if ( pos== config.log_filename.length())
		{
			config.log_filename = storage_folder + "/"s + config.log_filename;
		}
		debug_buffer.setLogFile(config.log_filename.c_str());
	}
	errno=0;
    // Initialize global strings
    MyRegisterClass(hInstance);
    // Perform application initialization:
	HWND hWnd = InitInstance(hInstance, nCmdShow);
	InitRenderer(hWnd, config.enable_vr, config.dev_mode);
	if(!hWnd)
    {
        return FALSE;
    }
    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WORLDSPACE));
	MSG msg;
    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
       // if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
	
	if(fileLoader->GetRecordFilesLoaded())
	{
		auto l=fileLoader->GetFilesLoaded();
		std::cout<<"Files loaded:"<<std::endl;
		for(const auto &s:l)
			std::cout<<s<<std::endl;
	}
	ShutdownRenderer(msg.hwnd);
	teleport::client::DiscoveryService::ShutdownInstance();
	// Needed for asynchronous device creation in XAudio2
	CoUninitialize();

    return (int) msg.wParam;
}

#include <filesystem>
#include <fstream>
#include "TeleportClient/DiscoveryService.h"
namespace fs = std::filesystem;
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
	while(fs::current_path().has_parent_path()&&fs::current_path().filename()!="pc_client")
	{
		fs::current_path(fs::current_path().parent_path());
		for (auto const& dir_entry : std::filesystem::directory_iterator{fs::current_path()}) 
		{
			auto str=dir_entry.path().filename();
			if(str=="pc_client")
			{
				fs::current_path(dir_entry.path());
				break;
			}
		}
	}
	teleport_path = fs::current_path().parent_path().string();

	// replacing Windows' broken resource system, just load our icon from a png:
	const char filename[]="textures\\teleportvr.png";
	size_t bufferSize=fs::file_size(filename);
	std::vector<unsigned char> buffer(bufferSize);
	std::ifstream ifs(filename,std::ifstream::in|std::ofstream::binary);
	ifs.read((char*)buffer.data(),bufferSize);
	

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
	auto hResource		= FindResource(hInstance, MAKEINTRESOURCE(IDI_WORLDSPACE), RT_ICON);
	wcex.hIcon			= CreateIconFromResourceEx(buffer.data(), (DWORD)bufferSize, 1, 0x30000, 256, 256, LR_DEFAULTCOLOR); 
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = L"MainWindow";

	wcex.hIconSm = CreateIconFromResourceEx(buffer.data(), (DWORD)bufferSize, 1, 0x30000, 32, 32, LR_DEFAULTCOLOR);
 
    return RegisterClassExW(&wcex);
}

HWND InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass,L"Teleport VR Client", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, 800, 500, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }
   SetWindowPos(hWnd
   , HWND_TOP// or HWND_TOPMOST
   , 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);	
   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);
   return hWnd;
}

void ShutdownRenderer(HWND hWnd)
{
	useOpenXR.Shutdown();
	displaySurfaceManager.Shutdown();
	delete clientRenderer;
	clientRenderer=nullptr;
	delete sessionClient;
	sessionClient=nullptr;
	delete renderPlatform;
	renderPlatform = nullptr;
}
#define STRINGIFY(a) STRINGIFY2(a)
#define STRINGIFY2(a) #a


void InitRenderer(HWND hWnd,bool try_init_vr,bool dev_mode)
{
	clientRenderer=new clientrender::Renderer(gui);
	gdi = &deviceManager;
	dsmi = &displaySurfaceManager;
#if TELEPORT_CLIENT_USE_D3D12
	renderPlatform = new dx12::RenderPlatform();
#else
	renderPlatform = new dx11::RenderPlatform();
#endif
	displaySurfaceManager.Initialize(renderPlatform);

	// Pass "true" for first argument to deviceManager to use API debugging:
#if TELEPORT_INTERNAL_CHECKS
	static bool use_debug=true;
#else
	static bool use_debug=false;
#endif
	gdi->Initialize(use_debug, false, false);

	std::string src_dir = STRINGIFY(CMAKE_SOURCE_DIR);
	std::string build_dir = STRINGIFY(CMAKE_BINARY_DIR);
	if (teleport_path != "")
	{
		src_dir = teleport_path;
		build_dir = teleport_path+"/build_pc_client";
	}
	// Create an instance of our simple clientRenderer class defined above:
	{
		// Whether run from the project directory or from the executable location, we want to be
		// able to find the shaders and textures:
		char cwd[90];
		_getcwd(cwd, 90);
		renderPlatform->PushTexturePath("");
		renderPlatform->PushTexturePath("Textures");
		renderPlatform->PushTexturePath("../../../../pc_client/Textures");
		renderPlatform->PushTexturePath("../../pc_client/Textures");
		// Or from the Simul directory -e.g. by automatic builds:

		renderPlatform->PushTexturePath("pc_client/Textures");
		renderPlatform->PushShaderPath("pc_client/Shaders");
		renderPlatform->PushShaderPath("../client/Shaders");
		renderPlatform->PushShaderPath("../../client/Shaders");
		renderPlatform->PushShaderPath("../../../../client/Shaders");
		renderPlatform->PushTexturePath("Textures");
		renderPlatform->PushShaderPath("Shaders");	// working directory C:\Teleport

		renderPlatform->PushShaderPath((src_dir+"/firstparty/Platform/Shaders/SFX").c_str());
		renderPlatform->PushShaderPath((src_dir+"/firstparty/Platform/CrossPlatform/Shaders").c_str());
		renderPlatform->PushShaderPath("../../../../firstparty/Platform/Shaders/SFX");
		renderPlatform->PushShaderPath("../../../../firstparty/Platform/CrossPlatform/Shaders");
		renderPlatform->PushShaderPath("../../../../firstparty/Platform/ImGui/Shaders");
		renderPlatform->PushShaderPath("../../firstparty/Platform/Shaders/SFX");
		renderPlatform->PushShaderPath("../../firstparty/Platform/CrossPlatform/Shaders");
		renderPlatform->PushShaderPath("../../firstparty/Platform/ImGui/Shaders");
#if TELEPORT_CLIENT_USE_D3D12
		renderPlatform->PushShaderPath("../../../../Platform/DirectX12/Sfx");
		renderPlatform->PushShaderPath("../../Platform/DirectX12/Sfx");
		renderPlatform->PushShaderPath("Platform/DirectX12/Sfx/");
		// Must do this before RestoreDeviceObjects so the rootsig can be found
		renderPlatform->PushShaderBinaryPath((build_dir+"/shaderbin/DirectX12").c_str());
		renderPlatform->PushShaderBinaryPath("assets/shaders");
#endif
#if TELEPORT_CLIENT_USE_D3D11
		renderPlatform->PushShaderPath((src_dir + "/firstparty/Platform/DirectX11/Sfx").c_str());
		renderPlatform->PushShaderBinaryPath((build_dir+"/shaderbin").c_str());
		renderPlatform->PushShaderBinaryPath("assets/shaders");
#endif
#if TELEPORT_CLIENT_USE_VULKAN
		renderPlatform->PushShaderPath((src_dir + "/firstparty/Platform/Vulkan/Sfx").c_str());
		renderPlatform->PushShaderBinaryPath((build_dir+"/shaderbin").c_str());
		renderPlatform->PushShaderBinaryPath("assets/shaders");
#endif

		renderPlatform->SetShaderBuildMode(platform::crossplatform::ShaderBuildMode::BUILD_IF_CHANGED);
	}
	renderPlatform->RestoreDeviceObjects(gdi->GetDevice());
	// Now renderPlatform is initialized, can init OpenXR:

	useOpenXR.SetRenderPlatform(renderPlatform);
	renderDelegate = std::bind(&clientrender::Renderer::RenderVRView, clientRenderer, std::placeholders::_1);
	overlayDelegate = std::bind(&clientrender::Renderer::DrawOSD, clientRenderer, std::placeholders::_1);
	auto &config=client::Config::GetInstance();
	clientRenderer->Init(renderPlatform, &useOpenXR, (teleport::PlatformWindow*)GetActiveWindow());
	if(config.recent_server_urls.size())
		client::SessionClient::GetSessionClient(1)->SetServerIP(config.recent_server_urls[0]);

	dsmi->AddWindow(hWnd);
	dsmi->SetRenderer(clientRenderer);
}
static platform::core::DefaultProfiler cpuProfiler;
#define GET_X_LPARAM(lp)                        ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)                        ((int)(short)HIWORD(lp))
#define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))


// Forward declare message handler from imgui_impl_win32.cpp
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern  void		ImGui_ImplPlatform_SetMousePos(int x, int y,int w,int h);
#include <imgui.h>
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	//return 0;
	bool ui_handled=false;
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		ui_handled=true;
	}
	if (!ui_handled)// && gui.HasFocus())
	{
		switch (message)
		{
		case WM_KEYUP:
			switch (wParam)
			{
			case VK_ESCAPE:
				clientRenderer->ShowHideGui();
			default:
				break;
			}
			break;
		default:
			break;
		}
	}
	POINT pos;
	if (::GetCursorPos(&pos) && ::ScreenToClient(hWnd, &pos))
	{
		RECT rect;
		GetClientRect(hWnd, &rect);
		ImGui_ImplPlatform_SetMousePos(pos.x, pos.y,rect.right-rect.left,rect.bottom-rect.top);
	}
	{
		switch (message)
		{
		case WM_RBUTTONDOWN:
			clientRenderer->OnMouseButtonPressed(false, true, false, 0);
			if(!gui.IsVisible()&&!clientRenderer->OSDVisible())
				useOpenXR.OnMouseButtonPressed(false, true, false, 0);
			break;
		case WM_RBUTTONUP:
			clientRenderer->OnMouseButtonReleased(false, true, false, 0);
			if(!gui.IsVisible()&&!clientRenderer->OSDVisible())
				useOpenXR.OnMouseButtonReleased(false, true, false, 0);
			break;
		case WM_MOUSEMOVE:
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			clientRenderer->OnMouseMove(xPos, yPos
				, (wParam & MK_LBUTTON) != 0
				, (wParam & MK_RBUTTON) != 0
				, (wParam & MK_MBUTTON) != 0
				, zDelta);
		}
		break;
		default:
			break;
		}
	}
	if (!ui_handled )
	{
		switch (message)
		{
		case WM_KEYDOWN:
			clientRenderer->OnKeyboard((unsigned)wParam, true, gui.IsVisible());
			if(!gui.IsVisible()&&!clientRenderer->OSDVisible())
				useOpenXR.OnKeyboard((unsigned)wParam, true);
			break;
		case WM_KEYUP:
			clientRenderer->OnKeyboard((unsigned)wParam, false, gui.IsVisible());
			if(!gui.IsVisible()&&!clientRenderer->OSDVisible())
				useOpenXR.OnKeyboard((unsigned)wParam, false);
			break;
		default:
			break;
		}
	}

	if (!ui_handled && !gui.IsVisible())
	{
		switch (message)
		{
		case WM_LBUTTONDOWN:
			clientRenderer->OnMouseButtonPressed(true, false, false, 0);
			if (!gui.IsVisible() && !clientRenderer->OSDVisible())
				useOpenXR.OnMouseButtonPressed(true, false, false, 0);
			break;
		case WM_LBUTTONUP:
			clientRenderer->OnMouseButtonReleased(true, false, false, 0);
			if (!gui.IsVisible() && !clientRenderer->OSDVisible())
				useOpenXR.OnMouseButtonReleased(true, false, false, 0);
			break;
		case WM_MBUTTONDOWN:
			clientRenderer->OnMouseButtonPressed(false, false, true, 0);
			if (!gui.IsVisible() && !clientRenderer->OSDVisible())
				useOpenXR.OnMouseButtonPressed(false, false, true, 0);
			break;
		case WM_MBUTTONUP:
			clientRenderer->OnMouseButtonReleased(false, false, true, 0);
			if (!gui.IsVisible() && !clientRenderer->OSDVisible())
				useOpenXR.OnMouseButtonReleased(false, false, true, 0);
			break;
		case WM_MOUSEWHEEL:
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		}
		break;

		default:
			break;
		}
	}
	switch (message)
	{
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
			if(gdi)
			{
				double timestamp_ms = avs::PlatformWindows::getTimeElapsedInMilliseconds(clientrender::platformStartTimestamp, avs::PlatformWindows::getTimestamp());

				clientRenderer->Update(timestamp_ms);
				useOpenXR.Tick();
#ifndef FIX_BROKEN
				static double fTime=0.0;
				static platform::core::Timer t;
				float time_step=t.UpdateTime()/1000.0f;
				static long long frame = 1;
				renderPlatform->BeginFrame(frame++);
				platform::crossplatform::DisplaySurface *w = displaySurfaceManager.GetWindow(hWnd);
				clientRenderer->ResizeView(0, w->viewport.w, w->viewport.h);
				// Call StartFrame here so the command list will be in a recording state for D3D12 
				// because vertex and index buffers can be created in OnFrameMove. 
				// StartFrame does nothing for D3D11.
				w->StartFrame();
				clientRenderer->OnFrameMove(fTime,time_step,useOpenXR.HaveXRDevice());
				fTime+=time_step;
				errno=0;
				platform::crossplatform::MultiviewGraphicsDeviceContext	deviceContext;
				deviceContext.renderPlatform = renderPlatform;
				// This context is active. So we will use it.
				deviceContext.platform_context = w->GetPlatformDeviceContext();
				if (deviceContext.platform_context)
				{
					platform::crossplatform::SetGpuProfilingInterface(deviceContext, renderPlatform->GetGpuProfiler());
					platform::core::SetProfilingInterface(GET_THREAD_ID(), &cpuProfiler);
					renderPlatform->GetGpuProfiler()->SetMaxLevel(5);
					cpuProfiler.SetMaxLevel(5);
					cpuProfiler.StartFrame();
					renderPlatform->GetGpuProfiler()->StartFrame(deviceContext);
					SIMUL_COMBINED_PROFILE_STARTFRAME(deviceContext)
					SIMUL_COMBINED_PROFILE_START(deviceContext, "all");

					dsmi->Render(hWnd);
				
					SIMUL_COMBINED_PROFILE_END(deviceContext);	
					if (useOpenXR.HaveXRDevice())
					{
						// Note we do this even when the device is inactive.
						//  if we don't, we will never receive the transition from XR_SESSION_STATE_READY to XR_SESSION_STATE_FOCUSED
						useOpenXR.SetCurrentFrameDeviceContext(deviceContext);
						useOpenXR.RenderFrame(renderDelegate, overlayDelegate);
						if(useOpenXR.IsXRDeviceRendering())
							clientRenderer->SetExternalTexture(useOpenXR.GetRenderTexture());
					}
					else
					{
						clientRenderer->SetExternalTexture(nullptr);
					}
					errno = 0;
					renderPlatform->GetGpuProfiler()->EndFrame(deviceContext);
					cpuProfiler.EndFrame();
					SIMUL_COMBINED_PROFILE_ENDFRAME(deviceContext)
				}
#endif
				displaySurfaceManager.EndFrame();
				renderPlatform->EndFrame();
			}
        }
        break;
    case WM_DESTROY:
		client::SessionClient::DestroySessionClients();
		//ShutdownRenderer(hWnd);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
