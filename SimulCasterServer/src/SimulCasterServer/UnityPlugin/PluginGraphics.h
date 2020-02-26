#pragma once

#include "IUnityInterface.h"
#include "IUnityGraphics.h"

namespace SCServer
{
	class GraphicsManager
	{
	public:
		static IUnityInterfaces* mUnityInterfaces;
		static IUnityGraphics* mGraphics;
		static UnityGfxRenderer mRendererType;
		static void* mGraphicsDevice;
	};
}

