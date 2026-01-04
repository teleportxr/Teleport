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
// Need ONE global instance of this:
avs::Context context;
bool receive_link = false;
std::string cmdLine;

#include "Platform/Core/FileLoader.h"

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
		TELEPORT_CERR << "CoInitialize failed. Exiting." << std::endl;
		return 0;
	}
	// run from pc_client directory.
	std::filesystem::path current_path = std::filesystem::current_path();
	if (!std::filesystem::exists("pc_client/client_default.ini"))
	{
		wchar_t filename[700];
		DWORD res = GetModuleFileNameW(nullptr, filename, 700);
		if (res)
		{
			current_path = filename;
			current_path = current_path.remove_filename();
		}
		// Get into the pc_client directory.
		while (!current_path.empty() && !std::filesystem::exists("pc_client/client_default.ini"))
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
	current_path = current_path.append("pc_client").lexically_normal();
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
	TELEPORT_CERR << message << "\n";
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
		char cwd[90];
		_getcwd(cwd, 90);
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
		case WM_KEYUP:
			switch (wParam)
			{
			case VK_ESCAPE:
				clientRenderer->ShowHideGui();
			default:
				break;
			}
			break;
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
			clientRenderer->OnKeyboard((unsigned)wParam, true, gui.GetGuiType() == teleport::clientrender::GuiType::Connection);
			if (gui.GetGuiType() == teleport::clientrender::GuiType::None) useOpenXR.OnKeyboard((unsigned)wParam, true);
			break;
		case WM_KEYUP:
			clientRenderer->OnKeyboard((unsigned)wParam, false, gui.GetGuiType() == teleport::clientrender::GuiType::Connection);
			if (gui.GetGuiType() == teleport::clientrender::GuiType::None) useOpenXR.OnKeyboard((unsigned)wParam, false);
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

namespace fs = std::filesystem;

void ShutdownRendererLinux()
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

void InitRendererLinux(bool try_init_vr, bool dev_mode)
{
	clientRenderer = new clientrender::Renderer(gui);
	gdi = &deviceManager;
	dsmi = &displaySurfaceManager;
#if TELEPORT_CLIENT_USE_VULKAN
	renderPlatform = new vulkan::RenderPlatform();
#else
	TELEPORT_CERR << "Linux build requires Vulkan support" << std::endl;
	return;
#endif
	displaySurfaceManager.Initialize(renderPlatform);

#if TELEPORT_INTERNAL_CHECKS
	static bool use_debug = true;
#else
	static bool use_debug = false;
#endif
	gdi->Initialize(use_debug, false, false);

	teleport_path = fs::current_path().parent_path().string();
	std::string src_dir = teleport_path;
	std::string build_dir = teleport_path + "/build_pc_client";

	{
		char cwd[256];
		if (getcwd(cwd, sizeof(cwd)) != nullptr)
		{
			// Working directory obtained
		}
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
	clientRenderer->Init(renderPlatform, &useOpenXR, nullptr);
	dsmi->SetRenderer(clientRenderer);
}

int main(int argc, char *argv[])
{
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
	if (!std::filesystem::exists("pc_client/client_default.ini"))
	{
		// Try to find it relative to executable
		char exe_path[1024];
		ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
		if (len != -1)
		{
			exe_path[len] = '\0';
			current_path = std::filesystem::path(exe_path).parent_path();
		}
		while (!current_path.empty() && !std::filesystem::exists("pc_client/client_default.ini"))
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
	current_path = current_path.append("pc_client").lexically_normal();
	if (!std::filesystem::exists(current_path))
	{
		TELEPORT_CERR << "Cannot find pc_client directory" << std::endl;
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

	InitRendererLinux(config.enable_vr, config.dev_mode);
	ReceiveCmdLine();

#if TELEPORT_CLIENT_SUPPORT_IPSME
	mosquitto_lib_init();
#endif

	// Simple main loop - for now just initialize and wait
	// A full implementation would use X11/Wayland or GLFW for window management
	bool running = true;
	while (running)
	{
		if (client::TabContext::ShouldFollowExternalURL())
		{
			std::string url = client::TabContext::PopExternalURL();
			teleport::client::LaunchProtocolHandler(url);
		}

		auto microsecondsUTC = std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::system_clock::now().time_since_epoch());
		if (clientRenderer)
		{
			clientRenderer->Update(microsecondsUTC);
		}
		useOpenXR.Tick();

		// For headless/XR-only mode, we just run the XR loop
		// Break if XR session ends or user requests quit
		if (!useOpenXR.IsSessionActive() && !config.enable_vr)
		{
			// No VR and no window - exit
			running = false;
		}

		usleep(1000); // 1ms sleep to prevent busy-waiting
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

	ShutdownRendererLinux();
	teleport::client::DiscoveryService::ShutdownInstance();

	return 0;
}

#endif // _WIN32
