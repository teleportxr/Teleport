#pragma once
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "ClientRender/NodeManager.h"
#include "ClientRender/GeometryCache.h"
#include "TeleportClient/Config.h"
#include <functional>
#ifdef __ANDROID__
struct ANativeWindow;
#endif
namespace teleport
{
	#ifdef _MSC_VER
	typedef void* PlatformWindow;
	#endif
	#ifdef __ANDROID__
	typedef ANativeWindow PlatformWindow;
	#endif
	class Gui
	{
		client::Config *config=nullptr;
	public:
		Gui()
		{
		}
		~Gui()
		{
		}
		void SetConfig(client::Config *c)
		{
			config=c;
		}
		void SetPlatformWindow(PlatformWindow *w);
		void RestoreDeviceObjects(platform::crossplatform::RenderPlatform *r,PlatformWindow *w);
		void InvalidateDeviceObjects();
		void RecompileShaders();
		void Render(platform::crossplatform::GraphicsDeviceContext &deviceContext);
		void DrawTexture(platform::crossplatform::Texture* texture);
		void LinePrint(const char* txt,const float *clr=nullptr);
		void Anims(const ResourceManager<avs::uid,clientrender::Animation>& animManager);
		void NodeTree(const clientrender::NodeManager::nodeList_t&);
		void Scene();
		void BeginDebugGui(platform::crossplatform::GraphicsDeviceContext& deviceContext);
		void EndDebugGui(platform::crossplatform::GraphicsDeviceContext& deviceContext);
		void setGeometryCache(const clientrender::GeometryCache *g)
		{
			geometryCache=g;
		}
		void SetConnectHandler(std::function<void(const std::string&)> fn);
		void SetCancelConnectHandler(std::function<void()> fn);
		void Update(const std::vector<vec4>& hand_pos_press,bool have_vr);
		void ShowHide();
		void Show();
		void Hide();
		void SetScaleMetres();
		bool HasFocus() const
		{
			return visible;
		}
		bool SetConnecting(bool c)
		{
			return connecting = c;
		}
		bool SetConnected(bool c)
		{
			return connected = c;
		}
		void SetServerIPs(const std::vector<std::string> &server_ips);
		avs::uid GetSelectedServer() const;
		avs::uid GetSelectedUid() const;
		vec3 Get3DPos()
		{
			return menu_pos;
		}
		void Select(avs::uid u);
		void SelectPrevious();
		void SelectNext();
	protected:
		void BoneTreeNode(const std::shared_ptr<clientrender::Bone>& n, const char* search_text); 
		void TreeNode(const std::shared_ptr<clientrender::Node>& node,const char *search_text);
		const clientrender::GeometryCache *geometryCache=nullptr;
		platform::crossplatform::RenderPlatform* renderPlatform=nullptr;
		vec3 view_pos;
		vec3 view_dir;
		vec3 menu_pos;
		float azimuth=0.0f, tilt = 0.0f;
		std::string current_url;
		std::vector<std::string> server_ips;
		std::function<void(const std::string&)> connectHandler;
		std::function<void()> cancelConnectHandler;
		bool visible = false;
		bool connecting = false;
		bool connected = false;
		float width_m=0.6f;
		std::vector<unsigned int> keys_pressed;
		void ShowFont();
        char buf[500];
		bool have_vr_device = false;
		std::vector<avs::uid> selection_history;
		size_t selection_cursor;
		avs::uid selected_server=0;

		bool show_inspector=false;
		std::vector<vec4> hand_pos_press;
		std::string selected_url;
	};
}