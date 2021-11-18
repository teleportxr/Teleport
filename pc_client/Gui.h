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
		void PrintHelpText(simul::crossplatform::GraphicsDeviceContext& deviceContext);
		void ShowHide();
		void Show();
		void Hide();
		void SetScaleMetres();
		void SetConnectHandler(std::function<void(const std::string&)> fn);
		bool HasFocus() const
		{
			return visible;
		}
		void SetServerIPs(const std::vector<std::string> &server_ips);
	protected:
		simul::crossplatform::RenderPlatform* renderPlatform=nullptr;
		vec3 view_pos;
		vec3 view_dir;
		vec3 menu_pos;
		std::string current_url;
		std::vector<std::string> server_ips;
		std::function<void(const std::string&)> connectHandler;
		bool visible = false;
		float width_m=0.6f;
		std::vector<unsigned int> keys_pressed;
		void ShowFont();
        char buf[500];
	};
}