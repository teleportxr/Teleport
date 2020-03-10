#pragma once

#include "IUnityInterface.h"
#include "IUnityGraphics.h"

namespace SCServer
{
	class GraphicsManager
	{
	public:
		static void* CreateTextureCopy(void* sourceTexture);
		static void CopyResource(void* target, void* source);
		static void ReleaseResource(void* resource);
		static void AddResourceRef(void* texture);

		static IUnityInterfaces* mUnityInterfaces;
		static IUnityGraphics* mGraphics;
		static UnityGfxRenderer mRendererType;
		static void* mGraphicsDevice;
	};
}

