#pragma once
#include <GUI/GuiSys.h>
#include <GUI/VRMenu.h>
#include "UIBeamRenderer.h"
#include "ClientAppInterface.h"
namespace teleport
{
#ifdef TELEPORT_CLIENT_USE_IMGUI
	// This is the class we really want to use.
	class UIRenderer
	{
	public:
		UIRenderer(ClientAppInterface *c);
		~UIRenderer();
		void Init();
		void Render(OVRFW::ovrRendererOutput& res,int w,int h);
		void DispatchDrawCalls();
	protected:
		ClientAppInterface *clientAppInterface=nullptr;
		bool initialized=false;
	};
#endif
	// As a TEMPORARY measure we here define a GUI class based on the OVR menu code:
	class TinyUI
	{
	public:
		TinyUI() {}
		~TinyUI() {}

		bool Init(const xrJava* context, OVRFW::ovrFileSys* FileSys,OVRFW::OvrGuiSys *g,OVRFW::ovrLocale* l);
		void Shutdown();

		void ToggleMenu(ovrVector3f,ovrVector4f);
		void HideMenu();
		void SetURL(const std::string &u);
		void AddURL(const std::string &u);
		std::string GetURL() const;

		void Update(const OVRFW::ovrApplFrameIn& in);
		void Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out);

		void SetConnectHandler(std::function<void(const std::string &)> fn);

		OVRFW::OvrGuiSys& GetGuiSys() {
			return *GuiSys;
		}
		OVRFW::ovrLocale& GetLocale() {
			return *Locale;
		}
		std::vector<HitTest>& HitTests() {
			return hitTests;
		}

		struct ControllerState
		{
			OVR::Posef pose;
			bool clicking;
		};

		void DoHitTests(const OVRFW::ovrApplFrameIn& in,std::vector<ControllerState> states);
		void AddHitTestRay(const OVR::Posef& ray, bool isClicking);

	protected:
		OVRFW::VRMenuObject* CreateMenu(const std::string &menuName,const std::string &labelText);

		void PressConnect();
		void ChangeURL();
		OVRFW::ovrFileSys* FileSys=nullptr;
		OVRFW::OvrGuiSys* GuiSys=nullptr;
		OVRFW::ovrLocale* Locale=nullptr;
		std::unordered_map<OVRFW::VRMenuObject*, std::function<void(void)>> buttonHandlers;
		std::vector<HitTest> hitTests;
		std::vector<HitTest> previousHitTests;
		std::vector<OVRFW::VRMenuObject*> menuObjects;
		std::vector<OVRFW::VRMenu*> menus;
		std::vector<std::string> urls;
		UIBeamRenderer beamRenderer_;
		OVRFW::VRMenuObject *url= nullptr;
		OVRFW::VRMenuObject *connect= nullptr;
		std::string 		current_url;
		std::function<void(const std::string &)> connectHandler;
		bool  visible=false;
	};
}