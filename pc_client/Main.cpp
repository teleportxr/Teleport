#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "targetver.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#endif
// Windows Header Files:
#ifdef _WIN32
#include <windows.h>
#include <Shlobj_core.h>
#include <shellapi.h>
#endif
// C RunTime Header Files
#include "ClientRender/Renderer.h"
#include "Config.h"
#include "Platform/Core/EnvironmentVariables.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/Timer.h"
#include "Platform/CrossPlatform/DisplaySurface.h"
#include "Platform/CrossPlatform/DisplaySurfaceManager.h"
#include "Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "ProcessHandler.h"
#include "TeleportClient/TabContext.h"
#include "TeleportClient/URLHandlers.h"
#include "TeleportCore/ErrorHandling.h"
#include <filesystem>
#include <locale>
#include <regex>
#include <stdlib.h>
#ifdef _WIN32
#include "Resource.h"
#endif
#include "ClientApp/ClientApp.h"
#include "TeleportClient/ClientDeviceState.h"
#include "TeleportClient/DiscoveryService.h"
#ifdef _MSC_VER
#include "Platform/Windows/VisualStudioDebugOutput.h"
VisualStudioDebugOutput debug_buffer(true, nullptr, 128);
#endif

#if TELEPORT_CLIENT_USE_D3D12
#include "Platform/DirectX12/DeviceManager.h"
#include "Platform/DirectX12/RenderPlatform.h"
platform::dx12::DeviceManager deviceManager;
#elif TELEPORT_CLIENT_USE_VULKAN
#include "Platform/Vulkan/DeviceManager.h"
#include "Platform/Vulkan/RenderPlatform.h"
platform::vulkan::DeviceManager deviceManager;
#elif defined(_WIN32)
#include "Platform/DirectX11/DeviceManager.h"
#include "Platform/DirectX11/RenderPlatform.h"
platform::dx11::DeviceManager deviceManager;
#endif
#include "Platform/CrossPlatform/GpuProfiler.cpp"
#include "UseOpenXR.h"
#if TELEPORT_CLIENT_SUPPORT_IPSME
#include "TeleportClient/IPSME_MsgEnv.h"
#endif
using namespace teleport;

clientrender::Renderer *clientRenderer = nullptr;
teleport::client::SessionClient *sessionClient = nullptr;
UseOpenXR useOpenXR("Teleport PC Client");
clientrender::Gui gui(useOpenXR);
platform::crossplatform::GraphicsDeviceInterface *gdi = nullptr;
platform::crossplatform::DisplaySurfaceManagerInterface *dsmi = nullptr;
platform::crossplatform::RenderPlatform *renderPlatform = nullptr;

platform::crossplatform::DisplaySurfaceManager displaySurfaceManager;
client::ClientApp clientApp;
std::string teleport_path;
std::string storage_folder;
bool receive_link = false;
std::string cmdLine;
// Need ONE global instance of this:
// Must be declared last so it's destroyed last during program shutdown
avs::Context context;

#include "Platform/Core/FileLoader.h"

// Cross-platform keyboard handler. Mirrors the keyboard logic that previously
// lived inline in the Win32 WndProc, so the same keys behave identically on
// Windows and Linux. 'vk' is a Win32-style virtual key code; 'pressed' is
// true on key-down or key-repeat, false on key-up.
static void HandleKeyboard(unsigned vk, bool pressed)
{
	if (!clientRenderer) return;
	bool gui_shown = (gui.GetGuiType() == teleport::clientrender::GuiType::Connection);
	clientRenderer->OnKeyboard(vk, pressed, gui_shown);
	if (gui.GetGuiType() == teleport::clientrender::GuiType::None) useOpenXR.OnKeyboard(vk, pressed);
	if (!pressed && vk == 0x1B /*VK_ESCAPE*/) clientRenderer->ShowHideGui();
}

#ifdef _WIN32
#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;										// current instance
WCHAR szWindowClass[] = L"TeleportPCClientWindowClass"; // the main window class name

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
HWND InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
void InitRenderer(HWND, bool, bool);
void ShutdownRenderer(HWND);
#endif

void ReceiveCmdLine()
{
	using std::string, std::regex;
	try
	{
		string re_str = R"(\"([^\"]*)\")";
		regex re(re_str, std::regex_constants::icase);
		std::smatch match;
		if (std::regex_search(cmdLine, match, re))
		{
			cmdLine = (match.size() > 1 && match[1].matched) ? match[1].str() : "";
		}
	}
	catch (...)
	{
	}

	if (cmdLine.length() > 0) receive_link = true;
}

#ifdef _WIN32
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	std::wstring wcmdline = lpCmdLine;
	cmdLine = platform::core::WStringToUtf8(wcmdline);
	if (EnsureSingleProcess(cmdLine)) return 0;

	auto *fileLoader = platform::core::FileLoader::GetFileLoader();
	fileLoader->SetRecordFilesLoaded(true);
	// Needed for asynchronous device creation in XAudio2
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		TELEPORT_INTERNAL_CERR("CoInitialize failed. Exiting.");
		return 0;
	}
	// run from pc_client directory.
	std::filesystem::path current_path = std::filesystem::current_path();
	if (!std::filesystem::exists("client/client_default.ini"))
	{
		wchar_t filename[700];
		DWORD res = GetModuleFileNameW(nullptr, filename, 700);
		if (res)
		{
			current_path = filename;
			current_path = current_path.remove_filename();
		}
		// Get into the pc_client directory.
		while (!current_path.empty() && !std::filesystem::exists("client/client_default.ini"))
		{
			std::filesystem::path prev_path = current_path;
			// std::string rel_pc_client="../../pc_client";
			current_path = current_path.append("../").lexically_normal();
			if (prev_path == current_path) break;
			if (std::filesystem::exists(current_path))
				std::filesystem::current_path(current_path);
			else
				break;
		}
	}
	current_path = current_path.append("client").lexically_normal();
	if (!std::filesystem::exists(current_path))
		return -1;
	std::filesystem::current_path(current_path);
	auto &config = client::Config::GetInstance();
	// Get a folder we can write to:
	char szPath[MAX_PATH];

	HRESULT hResult = SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, szPath);
	if (hResult == S_OK)
	{
		storage_folder = std::string(szPath) + "/TeleportXR";
	}
	if (storage_folder.length() > 200)
	{
		// Too long, use the current path.
		storage_folder = std::filesystem::current_path().string();
		if (storage_folder.length() > 200)
		{
			// Too long, use the current path.
			std::cerr << "Storage path " << storage_folder << " is too long." << std::endl;
			exit(135);
		}
	}
	config.SetStorageFolder(storage_folder.c_str());
	clientApp.Initialize();
	gui.SetServerIPs(config.recent_server_urls);
	if (config.log_filename.size() > 0)
	{
		size_t pos = config.log_filename.find(':');
		if (pos == config.log_filename.length())
		{
			config.log_filename = storage_folder + "/"s + config.log_filename;
		}
		debug_buffer.setLogFile(config.log_filename.c_str());
	}
	errno = 0;
	// Initialize global strings
	MyRegisterClass(hInstance);
	// Perform application initialization:
	HWND hWnd = InitInstance(hInstance, nCmdShow);
	if (!hWnd)
	{
		return FALSE;
	}
	InitRenderer(hWnd, config.enable_vr, config.dev_mode);
	receive_link = false;
	// remove quotes etc.
	ReceiveCmdLine();

#if TELEPORT_CLIENT_SUPPORT_IPSME
	mosquitto_lib_init();
#endif
	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WORLDSPACE));
	MSG msg;
	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			// Have we entered a URL or followed a link that is not a teleport protocol link?
			if (client::TabContext::ShouldFollowExternalURL())
			{
				std::string url = client::TabContext::PopExternalURL();
				teleport::client::LaunchProtocolHandler(url);
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

#if TELEPORT_CLIENT_SUPPORT_IPSME
	mosquitto_lib_cleanup();
#endif
	if (fileLoader->GetRecordFilesLoaded())
	{
		auto l = fileLoader->GetFilesLoaded();
		std::cout << "Files loaded:" << std::endl;
		for (const auto &s : l)
			std::cout << s << std::endl;
	}
	ShutdownRenderer(msg.hwnd);
	teleport::client::DiscoveryService::ShutdownInstance();
	// Needed for asynchronous device creation in XAudio2
	CoUninitialize();

	return (int)msg.wParam;
}

#include "TeleportClient/DiscoveryService.h"
#include <filesystem>
#include <fstream>
namespace fs = std::filesystem;
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	teleport_path = fs::current_path().parent_path().string();

	// replacing Windows' broken resource system, just load our icon from a png:
	const char filename[] = "textures\\teleportvr.png";
	size_t bufferSize = fs::file_size(filename);
	std::vector<unsigned char> buffer(bufferSize);
	std::ifstream ifs(filename, std::ifstream::in | std::ofstream::binary);
	ifs.read((char *)buffer.data(), bufferSize);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	auto hResource = FindResource(hInstance, MAKEINTRESOURCE(IDI_WORLDSPACE), RT_ICON);
	wcex.hIcon = CreateIconFromResourceEx(buffer.data(), (DWORD)bufferSize, 1, 0x30000, 256, 256, LR_DEFAULTCOLOR);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = 0;
	wcex.lpszClassName = szWindowClass;

	wcex.hIconSm = CreateIconFromResourceEx(buffer.data(), (DWORD)bufferSize, 1, 0x30000, 32, 32, LR_DEFAULTCOLOR);

	return RegisterClassExW(&wcex);
}

int TeleportClientReportHook(int reportType, char *message, int *returnValue)
{
	//_CRT_WARN, _CRT_ERROR, or _CRT_ASSERT
	TELEPORT_INTERNAL_CERR("{}", message);
	if (reportType == _CRT_ASSERT) SIMUL_BREAK("Assertion Failed!");
	if (reportType == _CRT_ERROR) SIMUL_BREAK("Error!");
	return 0;
}

int TeleportClientReportHookW(int reportType, wchar_t *message, int *returnValue)
{
	//_CRT_WARN, _CRT_ERROR, or _CRT_ASSERT
	TELEPORT_WARN("{}\n", platform::core::WStringToUtf8(message));
	if (reportType == _CRT_ASSERT) SIMUL_BREAK("Assertion Failed!");
	if (reportType == _CRT_ERROR) SIMUL_BREAK("Error!");
	return 0;
}

HWND InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowW(szWindowClass, L"Teleport Spatial Client", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 800, 500, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}
	SetWindowPos(hWnd,
				 HWND_TOP // or HWND_TOPMOST
				 ,
				 0,
				 0,
				 0,
				 0,
				 SWP_NOMOVE | SWP_NOSIZE);
	ShowWindow(hWnd, nCmdShow);
	_CrtSetReportHook(&TeleportClientReportHook);
	_CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, &TeleportClientReportHook);
	_CrtSetReportHookW2(_CRT_RPTHOOK_INSTALL, &TeleportClientReportHookW);
	_set_error_mode(_OUT_TO_STDERR);
	// std::cout<<"asserting\n";
	// assert(false);
	// std::cout << "asserting done\n";
	UpdateWindow(hWnd);
	return hWnd;
}

void ShutdownRenderer(HWND hWnd)
{
	useOpenXR.Shutdown();
	displaySurfaceManager.Shutdown();
	delete clientRenderer;
	clientRenderer = nullptr;
	delete sessionClient;
	sessionClient = nullptr;
	delete renderPlatform;
	renderPlatform = nullptr;
}
#define STRINGIFY(a) STRINGIFY2(a)
#define STRINGIFY2(a) #a

void InitRenderer(HWND hWnd, bool try_init_vr, bool dev_mode)
{
	clientRenderer = new clientrender::Renderer(gui);
	gdi = &deviceManager;
	dsmi = &displaySurfaceManager;
#if TELEPORT_CLIENT_USE_D3D12
	renderPlatform = new dx12::RenderPlatform();
#elif TELEPORT_CLIENT_USE_VULKAN
	renderPlatform = new vulkan::RenderPlatform();
#else
	renderPlatform = new dx11::RenderPlatform();
#endif
	displaySurfaceManager.Initialize(renderPlatform);

	// Pass "true" for first argument to deviceManager to use API debugging:
#if TELEPORT_INTERNAL_CHECKS
	static bool use_debug = true;
#else
	static bool use_debug = false;
#endif
	gdi->Initialize(use_debug, false, false);

	std::string src_dir = STRINGIFY(CMAKE_SOURCE_DIR);
	std::string build_dir = STRINGIFY(CMAKE_BINARY_DIR);
	if (teleport_path != "")
	{
		src_dir = teleport_path;
		build_dir = teleport_path + "/build_pc_client";
	}
	// Create an instance of our simple clientRenderer class defined above:
	{
		// Whether run from the project directory or from the executable location, we want to be
		// able to find the shaders and textures:
		renderPlatform->PushTexturePath("");
		renderPlatform->PushTexturePath("Textures");
		renderPlatform->PushTexturePath("../../../../pc_client/Textures");
		renderPlatform->PushTexturePath("../../pc_client/Textures");
		renderPlatform->PushTexturePath("assets/Textures");
		// Or from the Simul directory -e.g. by automatic builds:

		renderPlatform->PushTexturePath("pc_client/Textures");
		renderPlatform->PushShaderPath("pc_client/Shaders");
		renderPlatform->PushShaderPath("../client/Shaders");
		renderPlatform->PushShaderPath("../../client/Shaders");
		renderPlatform->PushShaderPath("../../../../client/Shaders");
		renderPlatform->PushTexturePath("Textures");
		renderPlatform->PushShaderPath("Shaders"); // working directory C:\Teleport

		renderPlatform->PushShaderPath((src_dir + "/firstparty/Platform/Shaders/SFX").c_str());
		renderPlatform->PushShaderPath((src_dir + "/firstparty/Platform/CrossPlatform/Shaders").c_str());
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
		renderPlatform->PushShaderBinaryPath((build_dir + "/shaderbin/DirectX12").c_str());
		renderPlatform->PushShaderBinaryPath("assets/shaders/directx12");
#endif
#if TELEPORT_CLIENT_USE_D3D11
		renderPlatform->PushShaderPath((src_dir + "/firstparty/Platform/DirectX11/Sfx").c_str());
		renderPlatform->PushShaderBinaryPath((build_dir + "/shaderbin").c_str());
		renderPlatform->PushShaderBinaryPath("assets/shaders/directx11");
#endif
#if TELEPORT_CLIENT_USE_VULKAN
		renderPlatform->PushShaderPath("../../../../Platform/Vulkan/Sfx");
		renderPlatform->PushShaderPath("../../Platform/Vulkan/Sfx");
		renderPlatform->PushShaderPath("Platform/Vulkan/Sfx/");
		// Must do this before RestoreDeviceObjects so the rootsig can be found
		renderPlatform->PushShaderBinaryPath((build_dir + "/shaderbin/Vulkan").c_str());
		renderPlatform->PushShaderBinaryPath("assets/shaders/vulkan");
#endif

		renderPlatform->SetShaderBuildMode(platform::crossplatform::ShaderBuildMode::BUILD_IF_CHANGED);
	}
	platform::crossplatform::ResourceGroupLayout perFrameLayout;
	perFrameLayout.UseConstantBufferSlot(0);  // Camera
	perFrameLayout.UseConstantBufferSlot(1);  // Stereo camera
	perFrameLayout.UseConstantBufferSlot(10); // Scene constants
	perFrameLayout.UseSamplerSlot(4);
	perFrameLayout.UseSamplerSlot(6);
	perFrameLayout.UseSamplerSlot(9);
	perFrameLayout.UseSamplerSlot(11);
	renderPlatform->SetResourceGroupLayout(0, perFrameLayout);
	platform::crossplatform::ResourceGroupLayout fewPerFrameLayout;
	fewPerFrameLayout.UseReadOnlyResourceSlot(19);
	fewPerFrameLayout.UseReadOnlyResourceSlot(20);
	fewPerFrameLayout.UseReadOnlyResourceSlot(21);
	fewPerFrameLayout.UseReadOnlyResourceSlot(22);
	renderPlatform->SetResourceGroupLayout(1, fewPerFrameLayout);
	platform::crossplatform::ResourceGroupLayout perMaterialLayout;
	perMaterialLayout.UseConstantBufferSlot(5);
	perMaterialLayout.UseReadOnlyResourceSlot(15);
	perMaterialLayout.UseReadOnlyResourceSlot(16);
	perMaterialLayout.UseReadOnlyResourceSlot(17);
	perMaterialLayout.UseReadOnlyResourceSlot(18);
	renderPlatform->SetResourceGroupLayout(2, perMaterialLayout);
	renderPlatform->RestoreDeviceObjects(gdi->GetDevice());
	// Now renderPlatform is initialized, can init OpenXR:

	useOpenXR.SetRenderPlatform(renderPlatform);
	auto &config = client::Config::GetInstance();
	clientRenderer->Init(renderPlatform, &useOpenXR, (teleport::clientrender::PlatformWindow *)GetActiveWindow());
	// if(config.recent_server_urls.size())
	//	client::SessionClient::GetSessionClient(1)->SetServerIP(config.recent_server_urls[0]);

	dsmi->AddWindow(hWnd);
	dsmi->SetRenderer(clientRenderer);
}
static platform::core::DefaultProfiler cpuProfiler;
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)HIWORD(wParam))

// Forward declare message handler from imgui_impl_win32.cpp
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern void ImGui_ImplPlatform_SetMousePos(int x, int y, int w, int h);
#include <imgui.h>
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	bool ui_handled = false;
	// if(message!=15)
	//	std::cout<<"\tWndProc "<<std::hex<<message<<std::endl;
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		ui_handled = true;
	}
	if (!ui_handled) // && gui.HasFocus())
	{
		switch (message)
		{
		case WM_IME_NOTIFY:
		case WM_ENABLE:
			// case WM_NCACTIVATE:
			//	case WM_ACTIVATE:
			// SIMUL_BREAK("WMW");
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
		if (!useOpenXR.IsSessionActive()) ImGui_ImplPlatform_SetMousePos(pos.x, pos.y, rect.right - rect.left, rect.bottom - rect.top);
	}
	{
		switch (message)
		{
		case WM_COPYDATA:
			// An external application has sent us a message.
			{
				cmdLine = GetExternalCommandLine(lParam);
				ReceiveCmdLine();
			}
			break;
		case WM_RBUTTONDOWN:
			clientRenderer->OnMouseButtonPressed(false, true, false, 0);
			if (gui.GetGuiType() == teleport::clientrender::GuiType::None) useOpenXR.OnMouseButtonPressed(false, true, false, 0);
			break;
		case WM_RBUTTONUP:
			clientRenderer->OnMouseButtonReleased(false, true, false, 0);
			if (gui.GetGuiType() == teleport::clientrender::GuiType::None) useOpenXR.OnMouseButtonReleased(false, true, false, 0);
			break;
		case WM_MOUSEMOVE:
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			clientRenderer->OnMouseMove(xPos, yPos, (wParam & MK_LBUTTON) != 0, (wParam & MK_RBUTTON) != 0, (wParam & MK_MBUTTON) != 0, zDelta);
		}
		break;
		default:
			break;
		}
	}
	if (!ui_handled)
	{
		switch (message)
		{
		case WM_KEYDOWN:
			HandleKeyboard((unsigned)wParam, true);
			break;
		case WM_KEYUP:
			HandleKeyboard((unsigned)wParam, false);
			break;
		default:
			break;
		}
	}

	if (!ui_handled && gui.GetGuiType() == teleport::clientrender::GuiType::None)
	{
		switch (message)
		{
		case WM_LBUTTONDOWN:
			clientRenderer->OnMouseButtonPressed(true, false, false, 0);
			if (gui.GetGuiType() == teleport::clientrender::GuiType::None) useOpenXR.OnMouseButtonPressed(true, false, false, 0);
			break;
		case WM_LBUTTONUP:
			clientRenderer->OnMouseButtonReleased(true, false, false, 0);
			if (gui.GetGuiType() == teleport::clientrender::GuiType::None) useOpenXR.OnMouseButtonReleased(true, false, false, 0);
			break;
		case WM_MBUTTONDOWN:
			clientRenderer->OnMouseButtonPressed(false, false, true, 0);
			if (gui.GetGuiType() == teleport::clientrender::GuiType::None) useOpenXR.OnMouseButtonPressed(false, false, true, 0);
			break;
		case WM_MBUTTONUP:
			clientRenderer->OnMouseButtonReleased(false, false, true, 0);
			if (gui.GetGuiType() == teleport::clientrender::GuiType::None) useOpenXR.OnMouseButtonReleased(false, false, true, 0);
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
		if (gdi)
		{
			auto microsecondsUTC = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());

			clientRenderer->Update(microsecondsUTC);
			useOpenXR.Tick();
#ifndef FIX_BROKEN
			static double fTime = 0.0;
			static platform::core::Timer t;
			float time_step = t.UpdateTime() / 1000.0f;
			static long long frame = 1;
			renderPlatform->BeginFrame(frame++);
			platform::crossplatform::DisplaySurface *w = displaySurfaceManager.GetWindow(hWnd);
			if(w)
			{
			clientRenderer->ResizeView(0, w->viewport.w, w->viewport.h);
			}
			// Call StartFrame here so the command list will be in a recording state for D3D12
			// because vertex and index buffers can be created in OnFrameMove.
			// StartFrame does nothing for D3D11.
			clientRenderer->OnFrameMove(fTime, time_step);
			fTime += time_step;
			errno = 0;
			{
				platform::core::SetProfilingInterface(GET_THREAD_ID(), &cpuProfiler);
				renderPlatform->GetGpuProfiler()->SetMaxLevel(5);
				cpuProfiler.SetMaxLevel(5);
				cpuProfiler.StartFrame();

				dsmi->Render(hWnd);
			}
#endif
			if (receive_link)
			{
				gui.SetGuiType(teleport::clientrender::GuiType::Connection);
				gui.Navigate(cmdLine);
				receive_link = false;
			}
			displaySurfaceManager.EndFrame();
			// renderPlatform->EndFrame();
			cpuProfiler.EndFrame();
		}
	}
	break;
	case WM_DESTROY:
		client::SessionClient::DestroySessionClients();
		// ShutdownRenderer(hWnd);
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

#else // Linux implementation

#include <unistd.h>
#include <pwd.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "Platform/ImGui/imgui_impl_platform.h"

namespace fs = std::filesystem;

static platform::core::DefaultProfiler cpuProfiler;

// The GLFW window and the Vulkan surface created from it. A pointer to
// g_surface is what we pass to the DisplaySurfaceManager as cp_hwnd, because
// the Vulkan DisplaySurface dereferences the handle as VkSurfaceKHR* on Linux.
static GLFWwindow *g_window = nullptr;
static VkSurfaceKHR g_surface = VK_NULL_HANDLE;

// Map a GLFW key code to a Win32-style virtual key code that the Renderer/Gui
// understand. Only the keys actually used by the renderer are mapped.
static unsigned GlfwKeyToVk(int key)
{
	if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) return (unsigned)('A' + (key - GLFW_KEY_A));
	if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) return (unsigned)('0' + (key - GLFW_KEY_0));
	switch (key)
	{
	case GLFW_KEY_SPACE: return 0x20;
	case GLFW_KEY_ESCAPE: return 0x1B;
	case GLFW_KEY_LEFT_SHIFT:
	case GLFW_KEY_RIGHT_SHIFT: return 0x10;
	case GLFW_KEY_LEFT_CONTROL:
	case GLFW_KEY_RIGHT_CONTROL: return 0x11;
	case GLFW_KEY_LEFT_ALT:
	case GLFW_KEY_RIGHT_ALT: return 0x12;
	case GLFW_KEY_LEFT: return 0x25;
	case GLFW_KEY_UP: return 0x26;
	case GLFW_KEY_RIGHT: return 0x27;
	case GLFW_KEY_DOWN: return 0x28;
	case GLFW_KEY_ENTER: return 0x0D;
	case GLFW_KEY_BACKSPACE: return 0x08;
	case GLFW_KEY_TAB: return 0x09;
	default: return 0;
	}
}

// Map a GLFW key code to the corresponding ImGuiKey.
static ImGuiKey GlfwKeyToImGuiKey(int key)
{
	if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) return (ImGuiKey)(ImGuiKey_A + (key - GLFW_KEY_A));
	if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) return (ImGuiKey)(ImGuiKey_0 + (key - GLFW_KEY_0));
	if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) return (ImGuiKey)(ImGuiKey_F1 + (key - GLFW_KEY_F1));
	if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) return (ImGuiKey)(ImGuiKey_Keypad0 + (key - GLFW_KEY_KP_0));
	switch (key)
	{
	case GLFW_KEY_TAB: return ImGuiKey_Tab;
	case GLFW_KEY_LEFT: return ImGuiKey_LeftArrow;
	case GLFW_KEY_RIGHT: return ImGuiKey_RightArrow;
	case GLFW_KEY_UP: return ImGuiKey_UpArrow;
	case GLFW_KEY_DOWN: return ImGuiKey_DownArrow;
	case GLFW_KEY_PAGE_UP: return ImGuiKey_PageUp;
	case GLFW_KEY_PAGE_DOWN: return ImGuiKey_PageDown;
	case GLFW_KEY_HOME: return ImGuiKey_Home;
	case GLFW_KEY_END: return ImGuiKey_End;
	case GLFW_KEY_INSERT: return ImGuiKey_Insert;
	case GLFW_KEY_DELETE: return ImGuiKey_Delete;
	case GLFW_KEY_BACKSPACE: return ImGuiKey_Backspace;
	case GLFW_KEY_SPACE: return ImGuiKey_Space;
	case GLFW_KEY_ENTER: return ImGuiKey_Enter;
	case GLFW_KEY_ESCAPE: return ImGuiKey_Escape;
	case GLFW_KEY_KP_ENTER: return ImGuiKey_KeypadEnter;
	case GLFW_KEY_KP_DECIMAL: return ImGuiKey_KeypadDecimal;
	case GLFW_KEY_KP_DIVIDE: return ImGuiKey_KeypadDivide;
	case GLFW_KEY_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
	case GLFW_KEY_KP_SUBTRACT: return ImGuiKey_KeypadSubtract;
	case GLFW_KEY_KP_ADD: return ImGuiKey_KeypadAdd;
	case GLFW_KEY_APOSTROPHE: return ImGuiKey_Apostrophe;
	case GLFW_KEY_COMMA: return ImGuiKey_Comma;
	case GLFW_KEY_MINUS: return ImGuiKey_Minus;
	case GLFW_KEY_PERIOD: return ImGuiKey_Period;
	case GLFW_KEY_SLASH: return ImGuiKey_Slash;
	case GLFW_KEY_SEMICOLON: return ImGuiKey_Semicolon;
	case GLFW_KEY_EQUAL: return ImGuiKey_Equal;
	case GLFW_KEY_LEFT_BRACKET: return ImGuiKey_LeftBracket;
	case GLFW_KEY_BACKSLASH: return ImGuiKey_Backslash;
	case GLFW_KEY_RIGHT_BRACKET: return ImGuiKey_RightBracket;
	case GLFW_KEY_GRAVE_ACCENT: return ImGuiKey_GraveAccent;
	case GLFW_KEY_CAPS_LOCK: return ImGuiKey_CapsLock;
	case GLFW_KEY_SCROLL_LOCK: return ImGuiKey_ScrollLock;
	case GLFW_KEY_NUM_LOCK: return ImGuiKey_NumLock;
	case GLFW_KEY_PRINT_SCREEN: return ImGuiKey_PrintScreen;
	case GLFW_KEY_PAUSE: return ImGuiKey_Pause;
	case GLFW_KEY_LEFT_SHIFT: return ImGuiKey_LeftShift;
	case GLFW_KEY_RIGHT_SHIFT: return ImGuiKey_RightShift;
	case GLFW_KEY_LEFT_CONTROL: return ImGuiKey_LeftCtrl;
	case GLFW_KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
	case GLFW_KEY_LEFT_ALT: return ImGuiKey_LeftAlt;
	case GLFW_KEY_RIGHT_ALT: return ImGuiKey_RightAlt;
	case GLFW_KEY_LEFT_SUPER: return ImGuiKey_LeftSuper;
	case GLFW_KEY_RIGHT_SUPER: return ImGuiKey_RightSuper;
	case GLFW_KEY_MENU: return ImGuiKey_Menu;
	default: return ImGuiKey_None;
	}
}

static void GlfwKeyCallback(GLFWwindow *win, int key, int /*scancode*/, int action, int /*mods*/)
{
	if (!clientRenderer) return;
	{
		ImGuiIO &io = ImGui::GetIO();
		io.AddKeyEvent(ImGuiMod_Ctrl, (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) || (glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS));
		io.AddKeyEvent(ImGuiMod_Shift, (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) || (glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS));
		io.AddKeyEvent(ImGuiMod_Alt, (glfwGetKey(win, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) || (glfwGetKey(win, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS));
		io.AddKeyEvent(ImGuiMod_Super, (glfwGetKey(win, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS) || (glfwGetKey(win, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS));
		ImGuiKey imkey = GlfwKeyToImGuiKey(key);
		if (imkey != ImGuiKey_None)
		{
			// Defensive: some X11 setups deliver auto-repeat as RELEASE+PRESS pairs instead of GLFW_REPEAT.
			// Only forward real state transitions to ImGui by consulting glfwGetKey which reflects the
			// physical key state regardless of the artificial events.
			static bool s_keyDown[GLFW_KEY_LAST + 1] = {};
			bool		actually_down				 = (glfwGetKey(win, key) == GLFW_PRESS);
			if (action == GLFW_REPEAT) actually_down = true;
			if (actually_down != s_keyDown[key])
			{
				s_keyDown[key] = actually_down;
				io.AddKeyEvent(imkey, actually_down);
			}
		}
	}
	unsigned vk = GlfwKeyToVk(key);
	if (vk == 0) return;
	// Mirror the Win32 'ui_handled' gate: when ImGui is capturing the keyboard
	// (e.g. a text input field has focus), do not forward keys to the renderer.
	if (ImGui::GetIO().WantCaptureKeyboard) return;
	if (action == GLFW_PRESS || action == GLFW_REPEAT)
		HandleKeyboard(vk, true);
	else if (action == GLFW_RELEASE)
		HandleKeyboard(vk, false);
}

static void GlfwCharCallback(GLFWwindow *, unsigned int codepoint)
{
	ImGuiIO &io = ImGui::GetIO();
	io.AddInputCharacter(codepoint);
}

static const char *ImGuiGetClipboardTextGlfw(void *user_data)
{
	const char *t =glfwGetClipboardString((GLFWwindow *)user_data);
	fprintf(stderr, "[clip] GET len=%zu first=\"%.40s\"\n", t ? strlen(t) : 0, t ? t : "(null)");
	return t;
}

static void ImGuiSetClipboardTextGlfw(void *user_data, const char *text)
{
	fprintf(stderr, "[clip] SET len=%zu first=\"%.40s\"\n", text ? strlen(text) : 0, text ? text : "(null)");
	glfwSetClipboardString((GLFWwindow *)user_data, text);
}

static void GlfwMouseButtonCallback(GLFWwindow *win, int button, int action, int /*mods*/)
{
	if (!clientRenderer) return;
	bool left = (button == GLFW_MOUSE_BUTTON_LEFT);
	bool right = (button == GLFW_MOUSE_BUTTON_RIGHT);
	bool middle = (button == GLFW_MOUSE_BUTTON_MIDDLE);
	bool gui_none = (gui.GetGuiType() == teleport::clientrender::GuiType::None);
	if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT || button == GLFW_MOUSE_BUTTON_MIDDLE)
	{
		ImGui_ImplPlatform_SetMouseDown(button, action == GLFW_PRESS);
	}
	if (action == GLFW_PRESS)
	{
		if (right || gui_none)
		{
			clientRenderer->OnMouseButtonPressed(left && gui_none, right, middle && gui_none, 0);
			if (gui_none) useOpenXR.OnMouseButtonPressed(left, right, middle, 0);
		}
	}
	else if (action == GLFW_RELEASE)
	{
		if (right || gui_none)
		{
			clientRenderer->OnMouseButtonReleased(left && gui_none, right, middle && gui_none, 0);
			if (gui_none) useOpenXR.OnMouseButtonReleased(left, right, middle, 0);
		}
	}
}

static void GlfwCursorPosCallback(GLFWwindow *win, double xpos, double ypos)
{
	if (!clientRenderer) return;
	int leftDown = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
	int rightDown = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
	int middleDown = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
	clientRenderer->OnMouseMove((int)xpos, (int)ypos, leftDown != 0, rightDown != 0, middleDown != 0, 0);
	int w = 0, h = 0;
	glfwGetWindowSize(win, &w, &h);
	if (!useOpenXR.IsSessionActive()) ImGui_ImplPlatform_SetMousePos((int)xpos, (int)ypos, w, h);
}

void ShutdownRendererLinux()
{
	useOpenXR.Shutdown();
	displaySurfaceManager.Shutdown();
	delete clientRenderer;
	clientRenderer = nullptr;
	delete sessionClient;
	sessionClient = nullptr;
	if (g_surface != VK_NULL_HANDLE && renderPlatform)
	{
		auto *vulkanRP = (vulkan::RenderPlatform *)renderPlatform;
		vk::Instance *inst = vulkanRP->AsVulkanInstance();
		if (inst) inst->destroySurfaceKHR(g_surface, nullptr);
		g_surface = VK_NULL_HANDLE;
	}
	delete renderPlatform;
	renderPlatform = nullptr;
	if (g_window)
	{
		glfwDestroyWindow(g_window);
		g_window = nullptr;
	}
	glfwTerminate();
}

void InitRendererLinux(GLFWwindow *window, bool try_init_vr, bool dev_mode, const std::vector<std::string> &required_instance_extensions)
{
	clientRenderer = new clientrender::Renderer(gui);
	gdi = &deviceManager;
	dsmi = &displaySurfaceManager;
#if TELEPORT_CLIENT_USE_VULKAN
	renderPlatform = new vulkan::RenderPlatform();
#else
	TELEPORT_INTERNAL_CERR("Linux build requires Vulkan support");
	return;
#endif
	displaySurfaceManager.Initialize(renderPlatform);

#if TELEPORT_INTERNAL_CHECKS
	static bool use_debug = true;
#else
	static bool use_debug = false;
#endif
	deviceManager.Initialize(use_debug, false, false, std::vector<std::string>(), required_instance_extensions);

	// Create the Vulkan surface from the GLFW window now that the Vulkan instance exists.
	// The instance lives on deviceManager at this point; renderPlatform doesn't get it
	// until RestoreDeviceObjects is called below.
	{
		vk::Instance *inst = deviceManager.GetVulkanInstance();
		if (inst && window)
		{
			VkResult r = glfwCreateWindowSurface(static_cast<VkInstance>(*inst), window, nullptr, &g_surface);
			if (r != VK_SUCCESS)
			{
				TELEPORT_INTERNAL_CERR("glfwCreateWindowSurface failed: {}", (int)r);
			}
		}
		else
		{
			TELEPORT_INTERNAL_CERR("Cannot create Vulkan surface: instance={} window={}", reinterpret_cast<void*>(inst), reinterpret_cast<void*>(window));
		}
	}

	teleport_path = fs::current_path().parent_path().string();
	std::string src_dir = teleport_path;
	std::string build_dir = teleport_path + "/build_pc_client";

	{
		renderPlatform->PushTexturePath("");
		renderPlatform->PushTexturePath("Textures");
		renderPlatform->PushTexturePath("../../../../pc_client/Textures");
		renderPlatform->PushTexturePath("../../pc_client/Textures");
		renderPlatform->PushTexturePath("assets/Textures");
		renderPlatform->PushTexturePath("pc_client/Textures");
		renderPlatform->PushShaderPath("pc_client/Shaders");
		renderPlatform->PushShaderPath("../client/Shaders");
		renderPlatform->PushShaderPath("../../client/Shaders");
		renderPlatform->PushShaderPath("../../../../client/Shaders");
		renderPlatform->PushTexturePath("Textures");
		renderPlatform->PushShaderPath("Shaders");

		renderPlatform->PushShaderPath((src_dir + "/firstparty/Platform/Shaders/SFX").c_str());
		renderPlatform->PushShaderPath((src_dir + "/firstparty/Platform/CrossPlatform/Shaders").c_str());
		renderPlatform->PushShaderPath("../../../../firstparty/Platform/Shaders/SFX");
		renderPlatform->PushShaderPath("../../../../firstparty/Platform/CrossPlatform/Shaders");
		renderPlatform->PushShaderPath("../../../../firstparty/Platform/ImGui/Shaders");
		renderPlatform->PushShaderPath("../../firstparty/Platform/Shaders/SFX");
		renderPlatform->PushShaderPath("../../firstparty/Platform/CrossPlatform/Shaders");
		renderPlatform->PushShaderPath("../../firstparty/Platform/ImGui/Shaders");
#if TELEPORT_CLIENT_USE_VULKAN
		renderPlatform->PushShaderPath("../../../../Platform/Vulkan/Sfx");
		renderPlatform->PushShaderPath("../../Platform/Vulkan/Sfx");
		renderPlatform->PushShaderPath("Platform/Vulkan/Sfx/");
		renderPlatform->PushShaderBinaryPath((build_dir + "/shaderbin/Vulkan").c_str());
		renderPlatform->PushShaderBinaryPath("assets/shaders/vulkan");
#endif
		renderPlatform->SetShaderBuildMode(platform::crossplatform::ShaderBuildMode::BUILD_IF_CHANGED);
	}
	platform::crossplatform::ResourceGroupLayout perFrameLayout;
	perFrameLayout.UseConstantBufferSlot(0);
	perFrameLayout.UseConstantBufferSlot(1);
	perFrameLayout.UseConstantBufferSlot(10);
	perFrameLayout.UseSamplerSlot(4);
	perFrameLayout.UseSamplerSlot(6);
	perFrameLayout.UseSamplerSlot(9);
	perFrameLayout.UseSamplerSlot(11);
	renderPlatform->SetResourceGroupLayout(0, perFrameLayout);
	platform::crossplatform::ResourceGroupLayout fewPerFrameLayout;
	fewPerFrameLayout.UseReadOnlyResourceSlot(19);
	fewPerFrameLayout.UseReadOnlyResourceSlot(20);
	fewPerFrameLayout.UseReadOnlyResourceSlot(21);
	fewPerFrameLayout.UseReadOnlyResourceSlot(22);
	renderPlatform->SetResourceGroupLayout(1, fewPerFrameLayout);
	platform::crossplatform::ResourceGroupLayout perMaterialLayout;
	perMaterialLayout.UseConstantBufferSlot(5);
	perMaterialLayout.UseReadOnlyResourceSlot(15);
	perMaterialLayout.UseReadOnlyResourceSlot(16);
	perMaterialLayout.UseReadOnlyResourceSlot(17);
	perMaterialLayout.UseReadOnlyResourceSlot(18);
	renderPlatform->SetResourceGroupLayout(2, perMaterialLayout);
	renderPlatform->RestoreDeviceObjects(gdi->GetDevice());

	useOpenXR.SetRenderPlatform(renderPlatform);
	auto &config = client::Config::GetInstance();
	clientRenderer->Init(renderPlatform, &useOpenXR, (teleport::clientrender::PlatformWindow *)window);
	if (g_surface != VK_NULL_HANDLE)
	{
		dsmi->AddWindow(&g_surface);
	}
	dsmi->SetRenderer(clientRenderer);
}

int main(int argc, char *argv[])
{
	// Force the C++ global locale to "C" so that std::regex compilation does not
	// route through glibc UTF-8 collation (strxfrm), which is pathologically slow
	// inside libstdc++'s _BracketMatcher::_M_make_cache and effectively hangs
	// shader effect parsing.
	std::locale::global(std::locale::classic());

	// Parse command line
	for (int i = 1; i < argc; i++)
	{
		if (cmdLine.length() > 0)
			cmdLine += " ";
		cmdLine += argv[i];
	}

	if (EnsureSingleProcess(cmdLine))
		return 0;

	auto *fileLoader = platform::core::FileLoader::GetFileLoader();
	fileLoader->SetRecordFilesLoaded(true);

	// Find the pc_client directory
	std::filesystem::path current_path = std::filesystem::current_path();
	if (!std::filesystem::exists("client/client_default.ini"))
	{
		// Try to find it relative to executable
		char exe_path[1024];
		ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
		if (len != -1)
		{
			exe_path[len] = '\0';
			current_path = std::filesystem::path(exe_path).parent_path();
		}
		while (!current_path.empty() && !std::filesystem::exists("client/client_default.ini"))
		{
			std::filesystem::path prev_path = current_path;
			current_path = current_path.append("../").lexically_normal();
			if (prev_path == current_path)
				break;
			if (std::filesystem::exists(current_path))
				std::filesystem::current_path(current_path);
			else
				break;
		}
	}
	current_path = current_path.append("client").lexically_normal();
	if (!std::filesystem::exists(current_path))
	{
		TELEPORT_WARN("Cannot find pc_client directory");
		return -1;
	}
	std::filesystem::current_path(current_path);

	auto &config = client::Config::GetInstance();

	// Get storage folder (Linux equivalent of CSIDL_LOCAL_APPDATA)
	const char *home = getenv("HOME");
	if (!home)
	{
		struct passwd *pw = getpwuid(getuid());
		if (pw)
			home = pw->pw_dir;
	}
	if (home)
	{
		storage_folder = std::string(home) + "/.local/share/TeleportXR";
		std::filesystem::create_directories(storage_folder);
	}
	else
	{
		storage_folder = std::filesystem::current_path().string();
	}
	config.SetStorageFolder(storage_folder.c_str());
	clientApp.Initialize();
	gui.SetServerIPs(config.recent_server_urls);

	// Bring up GLFW and create a window before the Vulkan instance, so that we can
	// query the platform-specific surface extensions GLFW needs.
	if (!glfwInit())
	{
		TELEPORT_INTERNAL_CERR("glfwInit failed");
		return -1;
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	g_window = glfwCreateWindow(800, 500, "Teleport Spatial Client", nullptr, nullptr);
	if (!g_window)
	{
		TELEPORT_INTERNAL_CERR("glfwCreateWindow failed");
		glfwTerminate();
		return -1;
	}
	glfwSetKeyCallback(g_window, GlfwKeyCallback);
	glfwSetCharCallback(g_window, GlfwCharCallback);
	glfwSetMouseButtonCallback(g_window, GlfwMouseButtonCallback);
	glfwSetCursorPosCallback(g_window, GlfwCursorPosCallback);

	std::vector<std::string> required_instance_extensions;
	{
		uint32_t count = 0;
		const char **exts = glfwGetRequiredInstanceExtensions(&count);
		for (uint32_t i = 0; i < count; i++)
			required_instance_extensions.emplace_back(exts[i]);
	}

	InitRendererLinux(g_window, config.enable_vr, config.dev_mode, required_instance_extensions);
	{
		ImGuiIO &io = ImGui::GetIO();
		io.SetClipboardTextFn = ImGuiSetClipboardTextGlfw;
		io.GetClipboardTextFn = ImGuiGetClipboardTextGlfw;
		io.ClipboardUserData = g_window;
	}
	receive_link = false;
	ReceiveCmdLine();

#if TELEPORT_CLIENT_SUPPORT_IPSME
	mosquitto_lib_init();
#endif

	platform::core::Timer frameTimer;
	double fTime = 0.0;
	long long frame = 1;

	while (!glfwWindowShouldClose(g_window))
	{
		glfwPollEvents();

		if (client::TabContext::ShouldFollowExternalURL())
		{
			std::string url = client::TabContext::PopExternalURL();
			teleport::client::LaunchProtocolHandler(url);
		}

		if (gdi && clientRenderer && renderPlatform)
		{
			auto microsecondsUTC = std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::system_clock::now().time_since_epoch());
			clientRenderer->Update(microsecondsUTC);
			useOpenXR.Tick();

			float time_step = frameTimer.UpdateTime() / 1000.0f;
			renderPlatform->BeginFrame(frame++);
			platform::crossplatform::DisplaySurface *w = displaySurfaceManager.GetWindow(&g_surface);
			if (w)
			{
				int fbw = 0, fbh = 0;
				glfwGetFramebufferSize(g_window, &fbw, &fbh);
				if (fbw > 0 && fbh > 0) clientRenderer->ResizeView(0, w->viewport.w, w->viewport.h);
			}
			clientRenderer->OnFrameMove(fTime, time_step);
			fTime += time_step;
			errno = 0;
			{
				platform::core::SetProfilingInterface(GET_THREAD_ID(), &cpuProfiler);
				renderPlatform->GetGpuProfiler()->SetMaxLevel(5);
				cpuProfiler.SetMaxLevel(5);
				cpuProfiler.StartFrame();

				{
					ImGuiIO &io = ImGui::GetIO();
					io.DeltaTime = time_step > 0.0f ? time_step : (1.0f / 60.0f);
				}
				dsmi->Render(&g_surface);
			}
			if (receive_link)
			{
				gui.SetGuiType(teleport::clientrender::GuiType::Connection);
				gui.Navigate(cmdLine);
				receive_link = false;
			}
			displaySurfaceManager.EndFrame();
			cpuProfiler.EndFrame();
		}
	}

#if TELEPORT_CLIENT_SUPPORT_IPSME
	mosquitto_lib_cleanup();
#endif

	if (fileLoader->GetRecordFilesLoaded())
	{
		auto l = fileLoader->GetFilesLoaded();
		std::cout << "Files loaded:" << std::endl;
		for (const auto &s : l)
			std::cout << s << std::endl;
	}

	client::SessionClient::DestroySessionClients();
	ShutdownRendererLinux();
	teleport::client::DiscoveryService::ShutdownInstance();

	return 0;
}

#endif // _WIN32
