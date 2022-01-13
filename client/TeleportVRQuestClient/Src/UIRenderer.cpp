#include "ClientDeviceState.h"
#include "UIRenderer.h"
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <VrApi_Types.h>
#include <GUI/GuiSys.h>
#include <Locale/OVR_Locale.h>
#include <GUI/DefaultComponent.h>
#include <GUI/ActionComponents.h>
#include <GUI/VRMenu.h>
#include <GUI/VRMenuObject.h>
#include <GUI/VRMenuMgr.h>
#include <GUI/Reflection.h>
#include <Render/DebugLines.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <android/asset_manager.h>

using namespace teleport;
#ifdef TELEPORT_CLIENT_USE_IMGUI
#include "imgui.h"
#include "imgui_impl_teleport_android.h"
#include <imgui_impl_opengl3.h>


UIRenderer::UIRenderer(ClientAppInterface *c)
:clientAppInterface (c)
{
}

UIRenderer::~UIRenderer()
{
	if (!initialized)
		return;

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplAndroid_Shutdown();
	ImGui::DestroyContext();

	initialized = false;
}

void UIRenderer::Init()
{
	if(initialized)
		return;
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	// Disable loading/saving of .ini file from disk.
	// FIXME: Consider using LoadIniSettingsFromMemory() / SaveIniSettingsToMemory() to save in appropriate location for Android.
	io.IniFilename = NULL;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// Setup Platform/Renderer backends
	ImGui_ImplAndroid_Init();
	ImGui_ImplOpenGL3_Init("#version 300 es");

	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
	// - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
	// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
	// - Read 'docs/FONTS.md' for more instructions and details.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	// - Android: The TTF files have to be placed into the assets/ directory (android/app/src/main/assets), we use our GetAssetData() helper to retrieve them.

	// We load the default font with increased size to improve readability on many devices with "high" DPI.
	// FIXME: Put some effort into DPI awareness.
	// Important: when calling AddFontFromMemoryTTF(), ownership of font_data is transfered by Dear ImGui by default (deleted is handled by Dear ImGui), unless we set FontDataOwnedByAtlas=false in ImFontConfig
	ImFontConfig font_cfg;
	font_cfg.SizePixels = 22.0f;
	io.Fonts->AddFontDefault(&font_cfg);
	//void* font_data;
	//int font_data_size;
	//ImFont* font;
	//font_data_size = GetAssetData("Roboto-Medium.ttf", &font_data);
	//font = io.Fonts->AddFontFromMemoryTTF(font_data, font_data_size, 16.0f);
	//IM_ASSERT(font != NULL);

	// Arbitrary scale-up
	// FIXME: Put some effort into DPI awareness
	ImGui::GetStyle().ScaleAllSizes(3.0f);

	initialized = true;
}

void UIRenderer::Render(OVRFW::ovrRendererOutput& res,int w,int h)
{
	ImGuiIO& io = ImGui::GetIO();

	// Our state
	static bool show_demo_window = true;
	static bool show_another_window = false;
	static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	// Poll Unicode characters via JNI
	// FIXME: do not call this every frame because of JNI overhead
	//PollUnicodeChars();

	// Open on-screen (soft) input if requested by Dear ImGui
	static bool WantTextInputLast = false;
	//if (io.WantTextInput && !WantTextInputLast)
	//	ShowSoftKeyboardInput();
	WantTextInputLast = io.WantTextInput;

	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplAndroid_NewFrame(w,h);
	ImGui::NewFrame();

	// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
	//if (show_demo_window)
	//	ImGui::ShowDemoWindow(&show_demo_window);

	// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
	{
		static float f = 0.0f;
		static int counter = 0;

		ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

		ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
		ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
		ImGui::Checkbox("Another Window", &show_another_window);

		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
		ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

		if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
			counter++;
		ImGui::SameLine();
		ImGui::Text("counter = %d", counter);

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}

	// 3. Show another simple window.
	if (show_another_window)
	{
		ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
		ImGui::Text("Hello from another window!");
		if (ImGui::Button("Close Me"))
			show_another_window = false;
		ImGui::End();
	}

	// Rendering
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


void UIRenderer::DispatchDrawCalls()
{
}
#endif
using namespace OVR;
using namespace OVRFW;
static const OVR::Vector4f MENU_DEFAULT_COLOR(0.0f,0.0f,0.1f,1.0f);
static const OVR::Vector4f BUTTON_DEFAULT_COLOR(0.25f, 0.25f, 0.25f, 1.0f);
static const OVR::Vector4f BUTTON_HIGHLIGHT_COLOR(1.0f,0.8f,0.1f,1.0f);

class SimpleTargetMenu : public OVRFW::VRMenu
{
public:
	static SimpleTargetMenu*Create(
			OVRFW::ovrFileSys*FileSys,OVRFW::OvrGuiSys &guiSys,OVRFW::ovrLocale &locale
			,const std::string &menuName,const std::string &text)
	{
		return new SimpleTargetMenu(FileSys,guiSys,locale,menuName,text);
	}

private:
	SimpleTargetMenu(
			OVRFW::ovrFileSys*FileSys,OVRFW::OvrGuiSys &guiSys,OVRFW::ovrLocale &locale
			,const std::string &menuName,const std::string &text)
			: OVRFW::VRMenu(menuName.c_str())
	{
		std::vector<uint8_t>                         buffer;
		std::vector<OVRFW::VRMenuObjectParms const*> itemParms;
		char const                                   menuFiles[] = "apk:///assets/ui.txt";
		std::vector<uint8_t>                         parmBuffer;
		if (!FileSys->ReadFile(menuFiles,parmBuffer))
		{
			return;
		}
		// Add a null terminator
		parmBuffer.push_back('\0');

		OVRFW::ovrParseResult parseResult = OVRFW::VRMenuObject::ParseItemParms(
				guiSys.GetReflection(),locale,menuName.c_str(),parmBuffer,itemParms);
		if (!parseResult)
		{
			DeletePointerArray(itemParms);
			ALOG("SimpleTargetMenu FAILED -> %s",parseResult.GetErrorText());
			return;
		}

		/// Hijack params
		for (auto*ip : itemParms)
		{
			// Find the one panel
			if ((int)ip->Id.Get() == 0)
			{
				const_cast<OVRFW::VRMenuObjectParms*>(ip)->Text = text;
			}
		}

		InitWithItems(guiSys,2.0f,OVRFW::VRMenuFlags_t(
				OVRFW::VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP),itemParms);
	}

	virtual ~SimpleTargetMenu()
	{
	};
};

bool TinyUI::Init(const xrJava*context,OVRFW::ovrFileSys*fileSys,OVRFW::OvrGuiSys*guiSys
				  ,OVRFW::ovrLocale*l)
{
	FileSys = fileSys;
	GuiSys  = guiSys;
	/// Needed for FONTS
	Locale  = l;
	//const xrJava*java = context;

	/// Leftovers that aren't used
	auto SoundEffectPlayer = new OVRFW::OvrGuiSys::ovrDummySoundEffectPlayer();
	if (nullptr == SoundEffectPlayer)
	{
		ALOGE("Couldn't create SoundEffectPlayer");
		return false;
	}
	auto DebugLines = OVRFW::OvrDebugLines::Create();
	if (nullptr == DebugLines)
	{
		ALOGE("Couldn't create DebugLines");
		return false;
	}
	DebugLines->Init();

	std::string fontName;
	GetLocale().GetLocalizedString("@string/font_name","efigs.fnt",fontName);

	GuiSys = OvrGuiSys::Create(context);
	if (nullptr == GuiSys)
	{
		ALOGE("Couldn't create GUI");
		return false;
	}
	GuiSys->Init(FileSys,*SoundEffectPlayer,fontName.c_str(),DebugLines);

	//AddButton("Connect",Vector3f(1.0f,1.5f,-1.0f),Vector2f(200.0f,100.0f));
	CreateMenu("Menu","");

	//AddLabel("Label",Vector3f(2.0f,1.5f,-2.0f),Vector2f(200.0f,100.0f));
	// Initialize pointer pose beam and particle
	beamRenderer_.Init(FileSys,nullptr,OVR::Vector4f(0.0f),1.0f);
	return true;
}

void TinyUI::Shutdown()
{
	GuiSys = nullptr;
	Locale = nullptr;
	beamRenderer_.Shutdown();
}

extern ovrQuatf QuaternionMultiply(const ovrQuatf &p, const ovrQuatf &q);
void TinyUI::ToggleMenu(ovrVector3f pos,ovrVector4f orient)
{
	const float pi=3.14159f;
	static float offset = 0*pi;
	if (!visible)
	{
		float yaw_angle = 2.0f * atan2(orient.y,orient.w) + offset;
		pos.x -= 0.7f * sin(yaw_angle);
		pos.z -= 0.7f * cos(yaw_angle);
		pos.y -= 0.4f;
		static float pitch_angle=pi/4.0f;
		ovrQuatf     q_pitch    = {sin(-pitch_angle / 2.0f),0,0,cos(-pitch_angle / 2.0f)};
		ovrQuatf     q_azimuth  = {0,sin(yaw_angle / 2.0f),0,cos(yaw_angle / 2.0f)};
		//OvrVRMenuMgr &menuMgr = GuiSys->GetVRMenuMgr();
		Posef        pose;
		pose.Translation = pos;
		pose.Rotation    = QuaternionMultiply(q_azimuth,q_pitch);
		for (auto &m:menus)
		{
			m->Open(*GuiSys);
			m->SetMenuPose(pose);
		}
		visible=true;
	}
	else
	{
		HideMenu();
	}
}

void TinyUI::HideMenu()
{
	for (auto &m:menus)
	{
		m->Close(*GuiSys);
	}
	visible=false;
}

void TinyUI::AddHitTestRay(const OVR::Posef &ray,bool isClicking)
{
	HitTest device;
	device.pointerStart = ray.Transform({0.0f,0.0f,0.0f});
	device.pointerEnd   = ray.Transform({0.0f,0.0f,-1.0f});
	device.clicked      = isClicking;
	hitTests.push_back(device);
}

void TinyUI::DoHitTests(const OVRFW::ovrApplFrameIn &in,std::vector<ControllerState> states)
{
	HitTests().clear();
	for (auto &state:states)
	{
		AddHitTestRay(state.pose,state.clicking);
	}
}

void TinyUI::SetURL(const std::string &u)
{
	if(u=="")
		return;
	if(std::find(urls.begin(),urls.end(),u)==urls.end())
		urls.push_back(u);
	current_url=u;
	if(url)
		url->SetText(current_url.c_str());
}

void TinyUI::AddURL(const std::string &u)
{
	if(std::find(urls.begin(),urls.end(),u)==urls.end())
		urls.push_back(u);
	if(current_url=="")
		SetURL(u);
}

std::string TinyUI::GetURL() const
{
	return current_url;
}

void TinyUI::SetConnectHandler(std::function<void(const std::string &)> fn)
{
	connectHandler=fn;
}

void TinyUI::PressConnect()
{
	connectHandler(url->GetText());
}

void TinyUI::ChangeURL()
{
	static size_t i=-1;
	i++;
	if(i==urls.size())
		i=0;
	current_url=urls[i];
	url->SetText(current_url.c_str());
}

void TinyUI::Update(const OVRFW::ovrApplFrameIn &in)
{
	if(!visible)
		return;
	/// clear previous frame
	for (auto &device : previousHitTests)
	{
		if (device.hitObject != nullptr&&device.hitObject->GetType()==OVRFW::eVRMenuObjectType::VRMENU_BUTTON)
		{
			device.hitObject->SetSurfaceColor(0,BUTTON_DEFAULT_COLOR);
			device.hitObject->SetHilighted(false);
		}
	}

	/// hit test
	for (auto &device : hitTests)
	{
		Vector3f pointerStart = device.pointerStart;
		Vector3f pointerEnd   = device.pointerEnd;
		Vector3f pointerDir   = (pointerEnd - pointerStart).Normalized();
		Vector3f targetEnd    = pointerStart + pointerDir * 10.0f;

		HitTestResult hit = GuiSys->TestRayIntersection(pointerStart,pointerDir);
		if (hit.HitHandle.IsValid())
		{
			device.pointerEnd = pointerStart + hit.RayDir * hit.t - pointerDir * 0.025f;
			device.hitObject  = GuiSys->GetVRMenuMgr().ToObject(hit.HitHandle);
			if (device.hitObject != nullptr&&device.hitObject->GetType()==OVRFW::eVRMenuObjectType::VRMENU_BUTTON)
			{
				device.hitObject->SetSurfaceColor(0,BUTTON_HIGHLIGHT_COLOR);
				device.hitObject->SetHilighted(true);
				pointerEnd = targetEnd;
				// check hit-testing
				if (device.clicked)
				{
					auto it = buttonHandlers.find(device.hitObject);
					if (it != buttonHandlers.end())
					{
						it->second();
					}
				}
			}
		}
	}

	/// Save these for later
	previousHitTests = hitTests;

	beamRenderer_.Update(in,HitTests());
}

void TinyUI::Render(const OVRFW::ovrApplFrameIn &in,OVRFW::ovrRendererOutput &out)
{
	if(!visible)
		return;
	const Matrix4f &traceMat = out.FrameMatrices.CenterView.Inverted();
	GuiSys->Frame(in,out.FrameMatrices.CenterView,traceMat);

	GuiSys->AppendSurfaceList(out.FrameMatrices.CenterView,&out.Surfaces);
	beamRenderer_.Render(in,out);
}

OVRFW::VRMenuObject *TinyUI::CreateMenu(const std::string &menuName,const std::string &labelText)
{
	VRMenu *m = SimpleTargetMenu::Create(FileSys,*GuiSys,*Locale,menuName,labelText);

	if (m != nullptr)
	{
		GuiSys->AddMenu(m);
		//GuiSys->OpenMenu(m->GetName());
		Posef pose = m->GetMenuPose();
		m->SetMenuPose(pose);
		OvrVRMenuMgr &menuMgr = GuiSys->GetVRMenuMgr();
		VRMenuObject *mo      = menuMgr.ToObject(m->GetRootHandle());
		if (mo != nullptr)
		{
			mo = menuMgr.ToObject(mo->GetChildHandleForIndex(0));
			//mo->SetSurfaceDims(0,size);
			mo->RegenerateSurfaceGeometry(0,false);
		}
		url			= menuMgr.ToObject(mo->ChildHandleForName(menuMgr,"url"));
		SetURL(current_url);
		connect      = menuMgr.ToObject(mo->ChildHandleForName(menuMgr,"connect"));
		buttonHandlers[connect]	=std::bind(&TinyUI::PressConnect,this);
		buttonHandlers[url]		=std::bind(&TinyUI::ChangeURL,this);
		menuObjects.push_back(mo);
		menus.push_back(m);
		return mo;
	}
	return nullptr;
}

