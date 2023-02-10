#pragma once
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "ClientRender/NodeManager.h"
#include "ClientRender/GeometryCache.h"
#include "libavstream/decoders/dec_interface.hpp"
#include "TeleportClient/Config.h"
#include <functional>
#ifdef __ANDROID__
struct ANativeWindow;
#endif
#include <client/Shaders/video_types.sl>
namespace clientrender
{
	struct DebugOptions;
}
namespace teleport
{
	namespace client
	{
		class SessionClient;
	}
	#ifdef _MSC_VER
	typedef void* PlatformWindow;
	#endif
	#ifdef __ANDROID__
	typedef ANativeWindow PlatformWindow;
	#endif
	class Gui
	{
	public:
		Gui()
		{
		}
		~Gui()
		{
		}
		void SetPlatformWindow(PlatformWindow *w);
		void RestoreDeviceObjects(platform::crossplatform::RenderPlatform *r,PlatformWindow *w);
		void InvalidateDeviceObjects();
		void RecompileShaders();
		void Render(platform::crossplatform::GraphicsDeviceContext &deviceContext);
		void DrawTexture(const platform::crossplatform::Texture* texture,float mip=-1.0f,int slice=0);
		void LinePrint(const char* txt,const float *clr=nullptr);
		void Textures(const ResourceManager<avs::uid,clientrender::Texture>& textureManager);
		void Anims(const ResourceManager<avs::uid,clientrender::Animation>& animManager);
		void NodeTree(const clientrender::NodeManager::nodeList_t&);
		void CubemapOSD(platform::crossplatform::Texture *videoTexture);
		void TagOSD(std::vector<clientrender::SceneCaptureCubeTagData> &videoTagDataCubeArray,VideoTagDataCube videoTagDataCube[]);
		void DebugPanel(clientrender::DebugOptions &debugOptions);
		void GeometryOSD();
		void Scene();
		bool Tab(const char *txt);
		void EndTab();
		// Unitless,relative to debug gui size, [-1,+1]
		void SetDebugGuiMouse(vec2 m,bool leftButton);
		void OnKeyboard(unsigned wParam, bool bKeyDown);
		void BeginDebugGui(platform::crossplatform::GraphicsDeviceContext& deviceContext);
		void EndDebugGui(platform::crossplatform::GraphicsDeviceContext& deviceContext);
		void setGeometryCache(const clientrender::GeometryCache *g)
		{
			geometryCache=g;
		}
		void setSessionClient(const teleport::client::SessionClient *g)
		{
			sessionClient=g;
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
		void SetVideoDecoderStatus(const avs::DecoderStatus& status) { videoStatus = status; }
		const avs::DecoderStatus& GetVideoDecoderStatus() { return videoStatus; }
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
		// Replaces Windows GetCursorPos if necessary.
		static int GetCursorPos(long p[2]) ;
	protected:
		void DelegatedDrawTexture(platform::crossplatform::GraphicsDeviceContext &deviceContext, platform::crossplatform::Texture* texture,float mip,int slice);

		void BoneTreeNode(const std::shared_ptr<clientrender::Bone>& n, const char* search_text); 
		void TreeNode(const std::shared_ptr<clientrender::Node>& node,const char *search_text);
		const clientrender::GeometryCache *geometryCache=nullptr;
		const teleport::client::SessionClient *sessionClient=nullptr;
		platform::crossplatform::RenderPlatform* renderPlatform=nullptr;
		vec3 view_pos;
		vec3 view_dir;
		vec3 menu_pos;
		bool in_tabs=false;
		float azimuth=0.0f, tilt = 0.0f;
		std::string current_url;
		std::vector<std::string> server_ips;
		std::function<void(const std::string&)> connectHandler;
		std::function<void()> cancelConnectHandler;
		bool visible = false;
		avs::DecoderStatus videoStatus;
		float width_m=0.6f;
		std::vector<unsigned int> keys_pressed;
		void ShowFont();
        char url_buffer[500];
		bool have_vr_device = false;
		std::vector<avs::uid> selection_history;
		size_t selection_cursor;
		avs::uid selected_server=0;

		bool show_inspector=false;
		std::vector<vec4> hand_pos_press;
		std::string selected_url;
	};
}