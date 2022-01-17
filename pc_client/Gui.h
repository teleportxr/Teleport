#pragma once
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "ClientRender/NodeManager.h"
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
		void LinePrint(const char* txt,const float *clr=nullptr);
		void NodeTree(const clientrender::NodeManager::nodeList_t&);
		void BeginDebugGui(simul::crossplatform::GraphicsDeviceContext& deviceContext);
		void EndDebugGui(simul::crossplatform::GraphicsDeviceContext& deviceContext);
		//void RenderDebugGui(simul::crossplatform::GraphicsDeviceContext& deviceContext);
		void Update(const std::vector<vec4>& hand_pos_press,bool have_vr);
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
		avs::uid GetSelectedUid() const
		{
			return selected_uid;
		}
	protected:
		void TreeNode(const std::shared_ptr<clientrender::Node>& node);
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
		bool have_vr_device = false;
		avs::uid selected_uid=0;
		std::shared_ptr<clientrender::Node> selected_node;
		std::shared_ptr<clientrender::Material> selected_material;
		bool show_inspector=false;
	};
}