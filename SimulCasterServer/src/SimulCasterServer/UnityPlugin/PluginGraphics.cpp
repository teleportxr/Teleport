#include "PluginGraphics.h"

#ifdef PLATFORM_WINDOWS
#include <d3d11.h>
#include <d3d12.h>
#include "IUnityGraphicsD3D11.h"
#include "IUnityGraphicsD3D12.h"
#endif

//#include "IUnityGraphicsVulkan.h"

namespace SCServer
{  
    IUnityInterfaces* GraphicsManager::mUnityInterfaces = nullptr;
    IUnityGraphics* GraphicsManager::mGraphics = nullptr;
    UnityGfxRenderer GraphicsManager::mRendererType = kUnityGfxRendererNull;
    void* GraphicsManager::mGraphicsDevice = nullptr;
}

using SGM = SCServer::GraphicsManager;

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
        case kUnityGfxRendererVulkan:
        {
            //Uncomment this when we support Vulkan. It would require addition of vulkan include paths.
            //SGM::mGraphicsDevice = SGM::mUnityInterfaces->Get<IUnityGraphicsVulkan>()->Instance().device;
            break;
        }
        default:
            SGM::mGraphicsDevice = nullptr;
            break;
    };
}




