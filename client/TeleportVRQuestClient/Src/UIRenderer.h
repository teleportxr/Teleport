#pragma once
#include <GUI/GuiSys.h>
#include <GUI/VRMenu.h>
#include "ClientAppInterface.h"
namespace teleport
{
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

	// As a TEMPORARY measure we here define a GUI class based on the OVR menu code:
	class ovrControllerGUI : public OVRFW::VRMenu
	{
	public:
		static char const* MENU_NAME;

		virtual ~ovrControllerGUI() {}

		static ovrControllerGUI* Create(ClientAppInterface *c,OVRFW::OvrGuiSys *,OVRFW::ovrLocale *);

	private:
		ClientAppInterface *clientAppInterface=nullptr;
		OVRFW::OvrGuiSys *guiSys=nullptr;

		ovrControllerGUI(ClientAppInterface *c,OVRFW::OvrGuiSys *g)
				: VRMenu(MENU_NAME), clientAppInterface(c),guiSys(g) {}

		ovrControllerGUI operator=(ovrControllerGUI&) = delete;

		virtual void OnItemEvent_Impl(
				OVRFW::OvrGuiSys& guiSys,
				OVRFW::ovrApplFrameIn const& vrFrame,
				OVRFW::VRMenuId_t const itemId,
				OVRFW::VRMenuEvent const& event) override;

		virtual bool OnKeyEvent_Impl(OVRFW::OvrGuiSys& guiSys, int const keyCode, const int repeatCount)
		override;

		virtual void PostInit_Impl(OVRFW::OvrGuiSys& guiSys, OVRFW::ovrApplFrameIn const& vrFrame) override;

		virtual void Open_Impl(OVRFW::OvrGuiSys& guiSys) override;

		virtual void Frame_Impl(OVRFW::OvrGuiSys& guiSys, OVRFW::ovrApplFrameIn const& vrFrame) override;
	};
}