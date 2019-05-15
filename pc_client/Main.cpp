// Worldspace.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "resource.h"
#include "Simul/Base/EnvironmentVariables.h"
#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DisplaySurfaceManager.h"
#include "Simul/Platform/CrossPlatform/DisplaySurface.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/DirectX11/Direct3D11Manager.h"
#include "ClientRenderer.h"
#ifdef _MSC_VER
#include "Simul/Platform/Windows/VisualStudioDebugOutput.h"
VisualStudioDebugOutput debug_buffer(true, nullptr, 128);
#endif

simul::crossplatform::GraphicsDeviceInterface *gdi = nullptr;
simul::crossplatform::DisplaySurfaceManagerInterface *dsmi = nullptr;
simul::crossplatform::RenderPlatform *renderPlatform = nullptr;
simul::dx11::RenderPlatform renderPlatformDx11;
simul::dx11::Direct3D11Manager direct3D11Manager;
simul::crossplatform::DisplaySurfaceManager displaySurfaceManager;
ClientRenderer clientRenderer;
#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szWindowClass[]=L"MainWindow";            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void InitRenderer(HWND);
void ShutdownRenderer();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WORLDSPACE));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
	ShutdownRenderer();
    return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = L"MainWindow";
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass,L"Client", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);
   InitRenderer(hWnd);
   return TRUE;
}

void ShutdownRenderer()
{
	clientRenderer.InvalidateDeviceObjects();
}

void InitRenderer(HWND hWnd)
{
	gdi = &direct3D11Manager;
	dsmi = &displaySurfaceManager;
	renderPlatform =&renderPlatformDx11;
	displaySurfaceManager.Initialize(renderPlatform);
	// Pass "true" to direct3D11Manager to use d3d debugging:
	gdi->Initialize(true, false,false);
	std::string simul_env = "C:/Simul/4.2/Simul";// simul::base::EnvironmentVariables::GetSimulEnvironmentVariable("SIMUL");
	// Create an instance of our simple clientRenderer class defined above:
	{
		// Whether run from the project directory or from the executable location, we want to be
		// able to find the shaders and textures:
		
		renderPlatform->PushTexturePath("");
		renderPlatform->PushTexturePath("../../../../Media/Textures");
		renderPlatform->PushTexturePath("../../Media/Textures");
		renderPlatform->PushTexturePath((simul_env+"/Media/Textures").c_str());
		// Or from the Simul directory -e.g. by automatic builds:

		renderPlatform->PushTexturePath("Media/Textures");
	
		renderPlatform->PushShaderPath("Shaders");		// working directory
		renderPlatform->PushShaderPath("Platform/CrossPlatform/SFX/");
		renderPlatform->PushShaderPath("../../../../Platform/CrossPlatform/SFX");
		renderPlatform->PushShaderPath("../../Platform/CrossPlatform/SFX");
		renderPlatform->PushShaderPath((simul_env+"/Platform/CrossPlatform/SFX").c_str());
		renderPlatform->PushShaderPath((simul_env+"/Platform/CrossPlatform/SL").c_str());
		renderPlatform->SetShaderBinaryPath("shaderbin");
		if (strcmp(renderPlatform->GetName(), "DirectX 11") == 0)
		{
			renderPlatform->PushShaderPath("../../../../Platform/DirectX11/HLSL");
			renderPlatform->PushShaderPath("../../Platform/DirectX11/HLSL");
			renderPlatform->PushShaderPath("Platform/DirectX11/HLSL/");
			renderPlatform->PushShaderPath((simul_env+"/Platform/DirectX11/HLSL").c_str());
			if(simul_env.length())
				renderPlatform->SetShaderBinaryPath((simul_env+"/Media/shaderbin").c_str());
		}
		if (strcmp(renderPlatform->GetName(), "DirectX 12") == 0)
		{
			renderPlatform->PushShaderPath("../../../../Platform/DirectX12/HLSL");
			renderPlatform->PushShaderPath("../../Platform/DirectX12/HLSL");
			renderPlatform->PushShaderPath("Platform/DirectX12/HLSL/");
			// Must do this before RestoreDeviceObjects so the rootsig can be found
			if(simul_env.length())
				renderPlatform->SetShaderBinaryPath("../../Platform/DirectX12/shaderbin");
		}
	}
	//renderPlatformDx12.SetCommandList((ID3D12GraphicsCommandList*)direct3D12Manager.GetImmediateCommandList());
	renderPlatform->RestoreDeviceObjects(gdi->GetDevice());
	clientRenderer.Init(renderPlatform);
	dsmi->AddWindow(hWnd);
	dsmi->SetRenderer(hWnd,&clientRenderer,-1);
}

#define GET_X_LPARAM(lp)                        ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)                        ((int)(short)HIWORD(lp))
#define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
	case WM_MOUSEWHEEL:
		{
			int xPos = GET_X_LPARAM(lParam); 
			int yPos = GET_Y_LPARAM(lParam); 
			short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			clientRenderer.OnMouse((wParam&MK_LBUTTON)!=0
				,(wParam&MK_RBUTTON)!=0
				,(wParam&MK_MBUTTON)!=0
				,0,xPos,yPos);
		}
		break;
	case WM_MOUSEMOVE:
		{
			int xPos = GET_X_LPARAM(lParam); 
			int yPos = GET_Y_LPARAM(lParam); 
			clientRenderer.OnMouse((wParam&MK_LBUTTON)!=0
				,(wParam&MK_RBUTTON)!=0
				,(wParam&MK_MBUTTON)!=0
				,0,xPos,yPos);
		}
		break;
	case WM_KEYDOWN:
			clientRenderer.OnKeyboard((unsigned)wParam,true);
		break;
	case WM_KEYUP:
			clientRenderer.OnKeyboard((unsigned)wParam,false);
		break;
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
//            PAINTSTRUCT ps;
			if(gdi)
			{
				double fTime=0.0;
				float time_step=0.01f;
				simul::crossplatform::DisplaySurface *w = displaySurfaceManager.GetWindow(hWnd);
				clientRenderer.ResizeView(0, w->viewport.w, w->viewport.h);
				clientRenderer.OnFrameMove(fTime,time_step);
				dsmi->Render(hWnd);
				displaySurfaceManager.EndFrame();
			}
           // HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
           // EndPaint(hWnd, &ps);
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
