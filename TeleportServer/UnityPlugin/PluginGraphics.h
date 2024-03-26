#pragma once
#if TELEPORT_UNITY_SERVER
#include "IUnityInterface.h"
#include "IUnityGraphics.h"
#endif
namespace teleport
{
	class GraphicsManager
	{
	public:
		static void* CreateTextureCopy(void* sourceTexture);
		static void CopyResource(void* target, void* source);
		static void ReleaseResource(void* resource);
		static void AddResourceRef(void* texture);

#if TELEPORT_UNITY_SERVER
		static IUnityInterfaces* mUnityInterfaces;
		static IUnityGraphics* mGraphics;
		static UnityGfxRenderer mRendererType;
#endif
		static void* mGraphicsDevice;
	};
}

