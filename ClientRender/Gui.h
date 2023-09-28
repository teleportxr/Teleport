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
	enum class ColourStyle
	{
		NONE,
		LIGHT_STYLE,
		DARK_STYLE,
		REBIND_STYLE
	};
	namespace client
	{
		class SessionClient;
		class ClientPipeline;
		class OpenXR;
	}
	#ifdef _MSC_VER
	typedef void* PlatformWindow;
	#endif
	#ifdef __ANDROID__
	typedef ANativeWindow PlatformWindow;
	#endif
	class Gui
	{
		bool BeginMainMenuBar();
		void EndMainMenuBar();
	public:
		Gui(client::OpenXR &o):openXR(o)
		{
		}
		~Gui()
		{
		}
		void SetPlatformWindow(PlatformWindow *w);
		void RestoreDeviceObjects(platform::crossplatform::RenderPlatform *r,PlatformWindow *w);
		void InvalidateDeviceObjects();
		void LoadShaders();
		void RecompileShaders();
		void Render3DGUI(platform::crossplatform::GraphicsDeviceContext &deviceContext);
		void Render2DGUI(platform::crossplatform::GraphicsDeviceContext& deviceContext);
		void DrawTexture(const platform::crossplatform::Texture* texture,float mip=-1.0f,int slice=0);
		void LinePrint(const std::string& str, const float* clr = nullptr);
		void LinePrint(const char* txt,const float *clr=nullptr);
		void Textures(const ResourceManager<avs::uid,clientrender::Texture>& textureManager);
		void Anims(const ResourceManager<avs::uid,clientrender::Animation>& animManager);
		void NodeTree(const std::vector<std::weak_ptr<clientrender::Node>>&);
		void CubemapOSD(platform::crossplatform::Texture *videoTexture);
		void TagOSD(std::vector<clientrender::SceneCaptureCubeTagData> &videoTagDataCubeArray,VideoTagDataCube videoTagDataCube[]);

		void InputsPanel(avs::uid server_uid,client::SessionClient* sessionClient, client::OpenXR* openXR);
		void NetworkPanel(const teleport::client::ClientPipeline& clientPipeline);
		void DebugPanel(clientrender::DebugOptions &debugOptions);
		void GeometryOSD();
		void Scene();
		bool Tab(const char *txt);
		void EndTab();
		// Unitless,relative to debug gui size, [-1,+1]
		void SetDebugGuiMouse(vec2 m,bool leftButton);
		void OnKeyboard(unsigned wParam, bool bKeyDown);
		void OverlayMenu(platform::crossplatform::GraphicsDeviceContext &deviceContext);
		void BeginFrame(platform::crossplatform::GraphicsDeviceContext &deviceContext);
		void BeginDebugGui(platform::crossplatform::GraphicsDeviceContext& deviceContext);
		void EndDebugGui(platform::crossplatform::GraphicsDeviceContext &deviceContext);
		void EndFrame(platform::crossplatform::GraphicsDeviceContext &deviceContext);

		void SetConsoleCommandHandler(std::function<void(const std::string&)> fn)
		{
			console = fn;
		}
		void SetConnectHandler(std::function<void(int32_t,const std::string&)> fn);
		void SetCancelConnectHandler(std::function<void(int32_t)> fn);
		void SetStartXRSessionHandler(std::function<void()> fn)
		{
			startXRSessionHandler = fn;
		}
		void SetEndXRSessionHandler(std::function<void()> fn)
		{
			endXRSessionHandler = fn;
		}
		void Update(const std::vector<vec4>& hand_pos_press,bool have_vr);
		void ShowHide();
		void Show();
		void Hide();
		void SetScaleMetres();
		bool IsVisible() const
		{
			return visible;
		}
		bool URLInputActive() const { return url_input; }
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
		std::function<void(const std::string&)> console;
		client::OpenXR &openXR;
		avs::uid cache_uid=0;
		void LightStyle();
		void DarkStyle();
		void RebindStyle();
		void ShowSettings2D();
		void MenuBar2D();
		void MainOptions();
		void ListBookmarks();
		void DevModeOptions();
		ColourStyle style= ColourStyle::NONE;
		void DelegatedDrawTexture(platform::crossplatform::GraphicsDeviceContext &deviceContext, platform::crossplatform::Texture* texture,float mip,int slice);

		void BoneTreeNode(const std::shared_ptr<clientrender::Bone> n, const char* search_text); 
		void TreeNode(const std::shared_ptr<clientrender::Node> node,const char *search_text);

		platform::crossplatform::RenderPlatform* renderPlatform=nullptr;
		vec3 view_pos;
		vec3 view_dir;
		vec3 menu_pos;
		vec2 bookmarks_pos;
		int32_t current_tab_context = 0;
		bool in_tabs=false;
		float azimuth=0.0f, tilt = 0.0f;
		std::string current_url;
		std::vector<std::string> server_ips;
		std::function<void(int32_t,const std::string &)> connectHandler;
		std::function<void(int32_t)> cancelConnectHandler;
		std::function<void()> startXRSessionHandler;
		std::function<void()> endXRSessionHandler;
		bool visible = false;
		static bool url_input;
		bool reset_menu_pos=false;
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
		bool show_bookmarks = false;
		bool show_options = false;
		platform::crossplatform::Texture *vrHeadsetIconTexture = nullptr;
		platform::crossplatform::Texture *viveControllerTexture = nullptr;
		
	};
}