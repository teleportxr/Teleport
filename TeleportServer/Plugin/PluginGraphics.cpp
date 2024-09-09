#include "PluginGraphics.h"

#ifdef PLATFORM_WINDOWS
#include <d3d11.h>
#include <d3d12.h>
#include "IUnityGraphicsD3D11.h"
#include "IUnityGraphicsD3D12.h"
#else
#include "IUnityGraphicsVulkan.h"
#endif


#include <iostream>

namespace teleport
{  
    IUnityInterfaces* GraphicsManager::mUnityInterfaces = nullptr;
    IUnityGraphics* GraphicsManager::mGraphics = nullptr;
    UnityGfxRenderer GraphicsManager::mRendererType = kUnityGfxRendererNull;
    void* GraphicsManager::mGraphicsDevice = nullptr;

    void* GraphicsManager::CreateTextureCopy(void* sourceTexture)
    {
        if (kUnityGfxRendererD3D11 != mRendererType)
        {
            std::cout << "Texture conversion only supported for D3D11 currently" << "\n";
            return nullptr;
        }
#ifdef PLATFORM_WINDOWS
        ID3D11Device* device = (ID3D11Device*)mGraphicsDevice;
        ID3D11Texture2D* source = (ID3D11Texture2D*)sourceTexture;

        D3D11_TEXTURE2D_DESC desc;
        source->GetDesc(&desc);

        // Aidan: This is a bit hacky but these are the only possibilities so it'll be grand 
        if (desc.Format == DXGI_FORMAT_R16G16B16A16_TYPELESS)
        {
            desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
        }
        else if (desc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS)
        {
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        ID3D11Texture2D* copy;
        device->CreateTexture2D(&desc, NULL, &copy);
       
        return copy;
		#else
		return nullptr;
		#endif
    }

    void GraphicsManager::CopyResource(void* target, void* source)
    {
#ifdef PLATFORM_WINDOWS
        if (kUnityGfxRendererD3D11 != mRendererType)
        {
            std::cout << "Texture conversion only supported for D3D11 currently" << "\n";
            return;
        }

        ID3D11Device* device = (ID3D11Device*)mGraphicsDevice;
     
        ID3D11DeviceContext* context;
        device->GetImmediateContext(&context);

        context->CopyResource((ID3D11Resource*)target, (ID3D11Resource*)source);

        context->Release();
#endif
    }

    void GraphicsManager::ReleaseResource(void* resource)
    {
#ifdef PLATFORM_WINDOWS
        if (resource)
        {
            ((IUnknown*)resource)->Release();
        }
#endif
    }

    void GraphicsManager::AddResourceRef(void* resource)
    {    
#ifdef PLATFORM_WINDOWS
        ((IUnknown*)resource)->AddRef();
#endif
    }
}

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);
static void AssignGraphicsDevice();

// Unity plugin load event
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces * unityInterfaces)
{
    teleport::GraphicsManager::mUnityInterfaces = unityInterfaces;
    teleport::GraphicsManager::mGraphics = unityInterfaces->Get<IUnityGraphics>();

    teleport::GraphicsManager::mGraphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

    // Run OnGraphicsDeviceEvent(initialize) manually on plugin load
    // to not miss the event in case the graphics device is already initialized
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

// Unity plugin unload event
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
    teleport::GraphicsManager::mGraphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

// Unity tries to call this, even though it's an old API that shouldn't exist.
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnitySetGraphicsDevice(void* device, int deviceType, int eventType)
{
	std::cerr<<"Unity calls UnitySetGraphicsDevice\n";
}

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
    switch (eventType)
    {
        case kUnityGfxDeviceEventInitialize:
        {
            teleport::GraphicsManager::mRendererType = teleport::GraphicsManager::mGraphics->GetRenderer();
            AssignGraphicsDevice();
            break;
        }
        case kUnityGfxDeviceEventShutdown:
        {
            teleport::GraphicsManager::mRendererType = kUnityGfxRendererNull;
            AssignGraphicsDevice();
            break;
        }
        case kUnityGfxDeviceEventBeforeReset:
        {      
            break;
        }
        case kUnityGfxDeviceEventAfterReset:
        {     
            break;
        }
    };
}

static void AssignGraphicsDevice()
{
    switch (teleport::GraphicsManager::mRendererType)
    {
// TODO: Vulkan support for Windows
#ifdef PLATFORM_WINDOWS
        case kUnityGfxRendererD3D11:
        {
            teleport::GraphicsManager::mGraphicsDevice = teleport::GraphicsManager::mUnityInterfaces->Get<IUnityGraphicsD3D11>()->GetDevice();
            break;
        }
        case kUnityGfxRendererD3D12:
        {
            teleport::GraphicsManager::mGraphicsDevice = teleport::GraphicsManager::mUnityInterfaces->Get<IUnityGraphicsD3D12>()->GetDevice();
            break;
        }
#else
        case kUnityGfxRendererVulkan:
        {
			auto v=teleport::GraphicsManager::mUnityInterfaces->Get<IUnityGraphicsVulkan>();
           	teleport::GraphicsManager::mGraphicsDevice = v->Instance().device;
            break;
        }
#endif
        default:
            teleport::GraphicsManager::mGraphicsDevice = nullptr;
            break;
    };
}





