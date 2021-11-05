#pragma once
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include <functional>

namespace teleport
{
	class Gui
	{
	public:
		void RestoreDeviceObjects(simul::crossplatform::RenderPlatform *r);
		void InvalidateDeviceObjects();
		void RecompileShaders();
		void Render(simul::crossplatform::GraphicsDeviceContext &deviceContext);
		void ShowHide();
		void Show();
		void Hide();
		void SetConnectHandler(std::function<void(const std::string&)> fn);
		bool HasFocus() const
		{
			return hasFocus;
		}
	protected:
		simul::crossplatform::RenderPlatform* renderPlatform=nullptr;
		vec3 view_pos;
		vec3 menu_pos;
		std::string current_url;
		std::function<void(const std::string&)> connectHandler;
		bool visible = false;
		bool hasFocus=false;
	};
}