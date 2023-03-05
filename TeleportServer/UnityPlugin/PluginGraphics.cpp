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

using SGM = teleport::GraphicsManager;

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);
static void AssignGraphicsDevice();

// Unity plugin load event
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces * unityInterfaces)
{
    SGM::mUnityInterfaces = unityInterfaces;
    SGM::mGraphics = unityInterfaces->Get<IUnityGraphics>();

    SGM::mGraphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

    // Run OnGraphicsDeviceEvent(initialize) manually on plugin load
    // to not miss the event in case the graphics device is already initialized
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

// Unity plugin unload event
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
    SGM::mGraphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
    switch (eventType)
    {
        case kUnityGfxDeviceEventInitialize:
        {
            SGM::mRendererType = SGM::mGraphics->GetRenderer();
            AssignGraphicsDevice();
            break;
        }
        case kUnityGfxDeviceEventShutdown:
        {
            SGM::mRendererType = kUnityGfxRendererNull;
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
    switch (SGM::mRendererType)
    {
// TODO: Vulkan support for Windows
#ifdef PLATFORM_WINDOWS
        case kUnityGfxRendererD3D11:
        {
            SGM::mGraphicsDevice = SGM::mUnityInterfaces->Get<IUnityGraphicsD3D11>()->GetDevice();
            break;
        }
        case kUnityGfxRendererD3D12:
        {
            SGM::mGraphicsDevice = SGM::mUnityInterfaces->Get<IUnityGraphicsD3D12>()->GetDevice();
            break;
        }
#else
        case kUnityGfxRendererVulkan:
        {
           	SGM::mGraphicsDevice = SGM::mUnityInterfaces->Get<IUnityGraphicsVulkan>()->Instance().device;
            break;
        }
#endif
        default:
            SGM::mGraphicsDevice = nullptr;
            break;
    };
}





