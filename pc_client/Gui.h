#pragma once
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "crossplatform/NodeManager.h"
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
		void DebugGui(simul::crossplatform::GraphicsDeviceContext& deviceContext,const scr::NodeManager::nodeList_t&);
		void Update(const std::vector<vec4>& hand_pos_press);
		void ShowHide();
		void Show();
		void Hide();
		void SetScaleMetres();
		void SetConnectHandler(std::function<void(const std::string&)> fn);
		bool HasFocus() const
		{
			return visible;
		}
		bool SetConnecting(bool c)
		{
			return connecting=c;
		}
		void SetServerIPs(const std::vector<std::string> &server_ips);
	protected:
		void NodeTree(const scr::Node& node);
		simul::crossplatform::RenderPlatform* renderPlatform=nullptr;
		vec3 view_pos;
		vec3 view_dir;
		vec3 menu_pos;
		float azimuth=0.0f, tilt = 0.0f;
		std::string current_url;
		std::vector<std::string> server_ips;
		std::function<void(const std::string&)> connectHandler;
		bool visible = false;
		bool connecting=false;
		float width_m=0.6f;
		std::vector<unsigned int> keys_pressed;
		void ShowFont();
        char buf[500];
	};
}