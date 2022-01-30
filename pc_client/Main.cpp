#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "stdafx.h"
#include "Resource.h"
#include "Platform/Core/EnvironmentVariables.h"
#include "Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/DisplaySurfaceManager.h"
#include "Platform/CrossPlatform/DisplaySurface.h"
#include "Platform/Core/Timer.h"
#include "Platform/Core/SimpleIni.h"

#include "ClientRenderer.h"
#include "TeleportCore/ErrorHandling.h"
#include "Config.h"
#ifdef _MSC_VER
#include "Platform/Windows/VisualStudioDebugOutput.h"
#include "TeleportClient/ClientDeviceState.h"
VisualStudioDebugOutput debug_buffer(true, nullptr, 128);
#endif

#if TELEPORT_CLIENT_USE_D3D12
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/DirectX12/DeviceManager.h"
simul::dx12::RenderPlatform renderPlatformImpl;
simul::dx12::DeviceManager deviceManager;
#else
#include "Platform/DirectX11/RenderPlatform.h"
#include "Platform/DirectX11/DeviceManager.h"
simul::dx11::RenderPlatform renderPlatformImpl;
simul::dx11::DeviceManager deviceManager;
#endif
#include "UseOpenXR.h"
#include "Platform/CrossPlatform/GpuProfiler.cpp"

using namespace teleport;

teleport::ClientRenderer *clientRenderer=nullptr;
simul::crossplatform::RenderDelegate renderDelegate;
teleport::UseOpenXR useOpenXR;
simul::crossplatform::GraphicsDeviceInterface *gdi = nullptr;
simul::crossplatform::DisplaySurfaceManagerInterface *dsmi = nullptr;
simul::crossplatform::RenderPlatform *renderPlatform = nullptr;

simul::crossplatform::DisplaySurfaceManager displaySurfaceManager;
teleport::client::ClientDeviceState clientDeviceState;
std::vector<std::string> server_ips;
teleport::Gui gui;

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

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	// Needed for asynchronous device creation in XAudio2
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED); 
	if (FAILED(hr))
	{
		TELEPORT_CERR << "Coinitialize failed. Exiting." << std::endl;
		return 0;
	}

	CSimpleIniA ini;
	SI_Error rc = ini.LoadFile("client.ini");
	bool enable_vr = true;
	bool dev_mode = false;
	bool render_local_offline = false;
	if(rc == SI_OK)
	{

		std::string server_ip = ini.GetValue("", "SERVER_IP", TELEPORT_SERVER_IP);
		std::string ip_list;
		ip_list = ini.GetValue("", "SERVER_IP", "");

		size_t pos = 0;
		std::string token;
		do
		{
			pos = ip_list.find(",");
			std::string ip = ip_list.substr(0, pos);
			server_ips.push_back(ip);
			ip_list.erase(0, pos + 1);
		} while (pos != std::string::npos);

		enable_vr = ini.GetLongValue("", "ENABLE_VR", true);
		dev_mode = ini.GetLongValue("", "DEV_MODE", false);
		render_local_offline = ini.GetLongValue("", "RENDER_LOCAL_OFFLINE", false);
		gui.SetServerIPs(server_ips);
	}
	else
	{
		TELEPORT_CERR<<"Create client.ini in pc_client directory to specify settings."<<std::endl;
	}
	errno=0;
    // Initialize global strings
    MyRegisterClass(hInstance);
    // Perform application initialization:
	HWND hWnd = InitInstance(hInstance, nCmdShow);
	InitRenderer(hWnd, enable_vr, dev_mode);
	if(!hWnd)
    {
        return FALSE;
    }
	clientRenderer->render_local_offline = render_local_offline;
    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WORLDSPACE));
    MSG msg;
    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
       // if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
          //  TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
	ShutdownRenderer(msg.hwnd);

	// Needed for asynchronous device creation in XAudio2
	CoUninitialize();

    return (int) msg.wParam;
}

#include <filesystem>
#include <fstream>
namespace fs = std::filesystem;
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
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
	if(clientRenderer)
		clientRenderer->InvalidateDeviceObjects();
	delete clientRenderer;
	clientRenderer=nullptr;
}
#define STRINGIFY(a) STRINGIFY2(a)
#define STRINGIFY2(a) #a
	
void InitRenderer(HWND hWnd,bool try_init_vr,bool dev_mode)
{
	clientRenderer=new teleport::ClientRenderer (&clientDeviceState,gui, dev_mode);
	gdi = &deviceManager;
	dsmi = &displaySurfaceManager;
	renderPlatform = &renderPlatformImpl;
	displaySurfaceManager.Initialize(renderPlatform);
	// Pass "true" to direct3D11Manager to use d3d debugging:
	gdi->Initialize(true, false,false);
	std::string src_dir = STRINGIFY(CMAKE_SOURCE_DIR);
	std::string build_dir = STRINGIFY(CMAKE_BINARY_DIR);
	// Create an instance of our simple clientRenderer class defined above:
	{
		// Whether run from the project directory or from the executable location, we want to be
		// able to find the shaders and textures:
		
		renderPlatform->PushTexturePath("");
		renderPlatform->PushTexturePath("Textures");
		renderPlatform->PushTexturePath("../../../../pc_client/Textures");
		renderPlatform->PushTexturePath("../../pc_client/Textures");
		// Or from the Simul directory -e.g. by automatic builds:

		renderPlatform->PushTexturePath("pc_client/Textures");
		renderPlatform->PushShaderPath("pc_client/Shaders");		// working directory C:\Simul\RemotePlay

		renderPlatform->PushShaderPath((src_dir+"/firstparty/Platform/Shaders/SFX").c_str());
		renderPlatform->PushShaderPath((src_dir+"/firstparty/Platform/Shaders/SL").c_str());
		renderPlatform->PushShaderPath("../../../../firstparty/Platform/Shaders/SFX");
		renderPlatform->PushShaderPath("../../../../firstparty/Platform/Shaders/SL");
		renderPlatform->PushShaderPath("../../firstparty/Platform/Shaders/SFX");
		renderPlatform->PushShaderPath("../../firstparty/Platform/Shaders/SL");
#if TELEPORT_CLIENT_USE_D3D12
		renderPlatform->PushShaderPath("../../../../Platform/DirectX12/HLSL");
		renderPlatform->PushShaderPath("../../Platform/DirectX12/HLSL");
		renderPlatform->PushShaderPath("Platform/DirectX12/HLSL/");
		// Must do this before RestoreDeviceObjects so the rootsig can be found
		renderPlatform->PushShaderBinaryPath((build_dir+"/firstparty/Platform/DirectX12/shaderbin").c_str());
		renderPlatform->PushShaderBinaryPath((build_dir+"/Platform/DirectX12/shaderbin").c_str());

		simul::dx12::DeviceManager* deviceManager = (simul::dx12::DeviceManager*)gdi;
		// We will provide a command list so initialization of following resource can take place
		((simul::dx12::RenderPlatform*)renderPlatform)->SetImmediateContext((simul::dx12::ImmediateContext*)deviceManager->GetImmediateContext());
#else
		renderPlatform->PushShaderPath((src_dir + "/firstparty/Platform/DirectX11/HLSL").c_str());
		renderPlatform->PushShaderBinaryPath((build_dir + "/firstparty/Platform/DirectX11/shaderbin").c_str());
		renderPlatform->PushShaderBinaryPath((build_dir + "/Platform/DirectX11/shaderbin").c_str());
#endif
	}
	//renderPlatformDx12.SetCommandList((ID3D12GraphicsCommandList*)direct3D12Manager.GetImmediateCommandList());
	renderPlatform->RestoreDeviceObjects(gdi->GetDevice());
	// Now renderPlatform is initialized, can init OpenXR:
	if (try_init_vr)
	{
		useOpenXR.Init(renderPlatform, "Teleport VR Client");
		useOpenXR.MakeActions();
	}
	auto showHideDelegate = std::bind(&teleport::Gui::ShowHide, &gui);
	if (useOpenXR.HaveXRDevice())
		useOpenXR.SetMenuButtonHandler(showHideDelegate);
	renderDelegate = std::bind(&ClientRenderer::RenderView, clientRenderer, std::placeholders::_1);
	clientRenderer->Init(renderPlatform);
	if(server_ips.size())
		clientRenderer->SetServer(server_ips[0].c_str());

#if TELEPORT_CLIENT_USE_D3D12
	//((simul::dx12::DeviceManager*)gdi)->FlushImmediateCommandList();
#endif

	dsmi->AddWindow(hWnd);
	dsmi->SetRenderer(hWnd,clientRenderer,-1);
}

static platform::core::DefaultProfiler cpuProfiler;
#define GET_X_LPARAM(lp)                        ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)                        ((int)(short)HIWORD(lp))
#define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))


// Forward declare message handler from imgui_impl_win32.cpp
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern  void		ImGui_ImplPlatform_SetMousePos(int x, int y, int W, int H);
#include <imgui.h>
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
#ifdef DEBUG_KEYS
	switch (message)
	{
	case WM_KEYDOWN:
		cout << "Key down" << std::endl;
		break;
	case WM_KEYUP:
		cout << "Key up" << std::endl;
		break;
	case WM_LBUTTONDOWN:
		cout << "Left button down" << std::endl;
		break;
	case WM_LBUTTONUP:
		cout << "Left button up" << std::endl;
		break;
	};
#endif
	bool ui_handled=false;
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		ui_handled=true;
	}
	if (!ui_handled && !gui.HasFocus())
	{
		switch (message)
		{
		case WM_KEYUP:
			switch (wParam)
			{
			case VK_ESCAPE:
				gui.Hide();
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
		ImGui_ImplPlatform_SetMousePos(pos.x, pos.y, rect.right - rect.left, rect.bottom - rect.top);
	}
	{
		switch (message)
		{
		case WM_RBUTTONDOWN:
			clientRenderer->OnMouseButtonPressed(false, true, false, 0);
			break;
		case WM_RBUTTONUP:
			clientRenderer->OnMouseButtonReleased(false, true, false, 0);
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
	if (!ui_handled && !gui.HasFocus())
	{
		switch (message)
		{
		case WM_KEYDOWN:
			clientRenderer->OnKeyboard((unsigned)wParam, true, gui.HasFocus());
			break;
		case WM_KEYUP:
			clientRenderer->OnKeyboard((unsigned)wParam, false, gui.HasFocus());
			break;
		case WM_LBUTTONDOWN:
			clientRenderer->OnMouseButtonPressed(true, false, false, 0);
			break;
		case WM_LBUTTONUP:
			clientRenderer->OnMouseButtonReleased(true, false, false, 0);
			break;
		case WM_MBUTTONDOWN:
			clientRenderer->OnMouseButtonPressed(false, false, true, 0);
			break;
		case WM_MBUTTONUP:
			clientRenderer->OnMouseButtonReleased(false, false, true, 0);
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
			//	PAINTSTRUCT ps;
				//BeginPaint(hWnd, &ps);
				clientRenderer->Update();
				bool quit = false;
				if (useOpenXR.HaveXRDevice())
				{
					useOpenXR.PollEvents(quit);
					useOpenXR.PollActions();
				}
				static double fTime=0.0;
				static platform::core::Timer t;
				float time_step=t.UpdateTime()/1000.0f;
				static long long frame = 1;
				renderPlatform->BeginFrame(frame++);
				simul::crossplatform::DisplaySurface *w = displaySurfaceManager.GetWindow(hWnd);
				clientRenderer->ResizeView(0, w->viewport.w, w->viewport.h);
				// Call StartFrame here so the command list will be in a recording state for D3D12 
				// because vertex and index buffers can be created in OnFrameMove. 
				// StartFrame does nothing for D3D11.
				w->StartFrame();
				if (useOpenXR.HaveXRDevice())
				{
					const avs::Pose &headPose=useOpenXR.GetHeadPose();
					clientDeviceState.SetHeadPose(headPose.position, headPose.orientation);
					for (int i = 0; i < useOpenXR.GetNumControllers(); i++)
					{
						const avs::Pose& controllerPose = useOpenXR.GetControllerPose(i);
						clientDeviceState.SetControllerPose(i, controllerPose.position, controllerPose.orientation);
						const teleport::client::ControllerState& controllerState = useOpenXR.GetControllerState(i);
						clientDeviceState.SetControllerState(i, controllerState);
					}
				}
				clientRenderer->OnFrameMove(fTime,time_step,useOpenXR.HaveXRDevice());
				fTime+=time_step;
				errno=0;
				simul::crossplatform::GraphicsDeviceContext	deviceContext;
				deviceContext.renderPlatform = renderPlatform;
				// This context is active. 
				deviceContext.platform_context = w->GetPlatformDeviceContext();

				simul::crossplatform::SetGpuProfilingInterface(deviceContext, renderPlatform->GetGpuProfiler());
				platform::core::SetProfilingInterface(GET_THREAD_ID(), &cpuProfiler);
				renderPlatform->GetGpuProfiler()->SetMaxLevel(5);
				cpuProfiler.SetMaxLevel(5);
				cpuProfiler.StartFrame();
				renderPlatform->GetGpuProfiler()->StartFrame(deviceContext);
				SIMUL_COMBINED_PROFILE_STARTFRAME(deviceContext)
				SIMUL_COMBINED_PROFILE_START(deviceContext, "all");

				dsmi->Render(hWnd);

				SIMUL_COMBINED_PROFILE_END(deviceContext);
				vec3 originPosition		= *((vec3*)&clientDeviceState.originPose.position);
				vec4 originOrientation	= *((vec4*)&clientDeviceState.originPose.orientation);
				if (useOpenXR.HaveXRDevice())
				{
					clientRenderer->SetExternalTexture(useOpenXR.GetRenderTexture());
					useOpenXR.RenderFrame(deviceContext, renderDelegate, originPosition, originOrientation);
				}
				errno=0;
				renderPlatform->GetGpuProfiler()->EndFrame(deviceContext);
				cpuProfiler.EndFrame();
				displaySurfaceManager.EndFrame();
				//renderPlatform->EndFrame();
				SIMUL_COMBINED_PROFILE_ENDFRAME(deviceContext)
				//EndPaint(hWnd, &ps);
			}
        }
        break;
    case WM_DESTROY:
		ShutdownRenderer(hWnd);
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
