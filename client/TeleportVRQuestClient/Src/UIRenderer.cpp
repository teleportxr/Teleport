#include "ClientDeviceState.h"
#include "UIRenderer.h"
#include "imgui.h"
#include <VrApi_Types.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include "imgui_impl_teleport_android.h"
#include <imgui_impl_opengl3.h>

using namespace teleport;

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


OVRFW::ovrLocale* Locale=nullptr;
const char* ovrControllerGUI::MENU_NAME = "controllerGUI";

ovrControllerGUI* ovrControllerGUI::Create(ClientAppInterface *c,OVRFW::OvrGuiSys *g, OVRFW::ovrLocale *l)
{
	char const* menuFiles[] = {"apk:///assets/ui.txt", nullptr};

	ovrControllerGUI* menu = new ovrControllerGUI(c,g);

	if (!menu->InitFromReflectionData(
			*g,
			g->GetFileSys(),
			g->GetReflection(),
			*l,
			menuFiles,
			2.0f,
			OVRFW::VRMenuFlags_t(OVRFW::VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP)))
	{
		delete menu;
		return nullptr;
	}
	return menu;
}

void ovrControllerGUI::OnItemEvent_Impl(
		OVRFW::OvrGuiSys& guiSys,
		OVRFW::ovrApplFrameIn const& vrFrame,
		OVRFW::VRMenuId_t const itemId,
		OVRFW::VRMenuEvent const& event) {}

bool ovrControllerGUI::OnKeyEvent_Impl(
		OVRFW::OvrGuiSys& guiSys,
		int const keyCode,
		const int repeatCount) {
	return (keyCode == AKEYCODE_BACK);
}

void ovrControllerGUI::PostInit_Impl(OVRFW::OvrGuiSys& guiSys, OVRFW::ovrApplFrameIn const& vrFrame) {}

void ovrControllerGUI::Open_Impl(OVRFW::OvrGuiSys& guiSys)
{}

void ovrControllerGUI::Frame_Impl(OVRFW::OvrGuiSys& guiSys, OVRFW::ovrApplFrameIn const& vrFrame)
{

}