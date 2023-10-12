
	#define _CRT_SECURE_NO_WARNINGS 1
#include <imgui.h>
#ifdef _MSC_VER
#include "Platform/ImGui/imgui_impl_win32.h"
#endif
#ifdef __ANDROID__
#include "backends/imgui_impl_android.h"
#endif
#include "Platform/ImGui/imgui_impl_platform.h"
#include "Platform/Core/FileLoader.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Gui.h"
#include "Light.h"
#include "IconsForkAwesome.h"
#include "TeleportCore/ErrorHandling.h"
#include "InstanceRenderer.h"
#include "Platform/External/magic_enum/include/magic_enum.hpp"
#include <fmt/core.h>
#include <fmt/format.h>

#ifdef _MSC_VER
#include <direct.h>
#include <Windows.h>
#endif
using namespace std::string_literals;
#include "Platform/Core/StringFunctions.h"
#include "TeleportClient/SessionClient.h"
#include "TeleportClient/OpenXR.h"
#include "TeleportClient/TabContext.h"

#ifdef __ANDROID__
#define VK_BACK           0x01
#define VK_ESCAPE         0x02
#endif

#define VK_MAX         0x10

bool KeysDown[VK_MAX];

using namespace teleport;
using namespace platform;
using namespace crossplatform;

ImFont *defaultFont = nullptr;
std::map<int,ImFont *> fontInter;
#define STR_VECTOR3 "%3.3f %3.3f %3.3f"
#define STR_VECTOR4 "%3.3f %3.3f %3.3f %3.3f"
PlatformWindow* platformWindow = nullptr;

bool Gui::url_input = false;

#define TIMED_TOOLTIP(...)\
		{if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))\
			ImGui::SetTooltip(__VA_ARGS__);}\

#define TIMED_TOOLTIP2(...)\
	{\
		static int timer=1200;\
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))\
			timer--; \
		else\
			timer = 1200;\
		if(timer<=0)\
			ImGui::SetTooltip(__VA_ARGS__);\
	}
	
ImGui_ImplPlatform_TextureView imgui_vrHeadsetIconTexture;
ImGui_ImplPlatform_TextureView imgui_ViveControllerTexture;
void Gui::SetPlatformWindow(PlatformWindow *w)
{
#ifndef _MSC_VER
	if(renderPlatform!=nullptr&&w!=platformWindow)
		ImGui_ImplAndroid_Init(w);
#endif
	platformWindow=w;
}
bool debug_begin_end=false;
std::vector<std::string> begin_end_stack;
bool ImGuiBegin(const char *txt,bool *on_off,ImGuiWindowFlags flags)
{
	begin_end_stack.push_back(txt);
	if(debug_begin_end)
	{
		TELEPORT_CERR<<"ImGuiBegin "<<txt<<"\n";
	}
	return ImGui::Begin(txt,on_off,flags);
}

void ImGuiEnd()
{
	if(begin_end_stack.size()==0)
	{
		TELEPORT_CERR<<"Called ImGuiEnd too many times...\n";
		debug_begin_end=true;
		return;
	}
	if(debug_begin_end)
	{
		TELEPORT_CERR<<"ImGuiEnd "<<begin_end_stack.back().c_str()<<"\n";
		if(begin_end_stack.size()==1)
		{
			TELEPORT_CERR<<"Empty.\n";
		}
	}
	begin_end_stack.pop_back();
	ImGui::End();
}

static inline ImVec4 ImLerp(const ImVec4& a, const ImVec4& b, float t)
{
	return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

void Gui::LightStyle()
{
	if (style == ColourStyle::LIGHT_STYLE)
		return;
	style = ColourStyle::LIGHT_STYLE;
	ImGui::StyleColorsLight();
	auto& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;
	colors[ImGuiCol_Button]			= ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	colors[ImGuiCol_ButtonHovered]	= ImVec4(0.7f, 0.7f, 0.7f, 1.00f);
	colors[ImGuiCol_ButtonActive]	= ImVec4(0.8f, 0.8f, 0.8f, 1.00f);
	colors[ImGuiCol_Border]			= ImVec4(0.00f, 0.00f, 0.00f, 0.3f);
	colors[ImGuiCol_BorderShadow]	= ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg]		= ImVec4(0.8f, 0.8f, 0.8f, 1.00f);
	style.GrabRounding = 1.f;
	style.WindowRounding = 12.f;
	style.ScrollbarRounding = 3.f;
	style.FrameRounding = 12.f;
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
	style.FramePadding = ImVec2(8.f, 4.f);
	style.FrameBorderSize = 1.f;
	style.WindowPadding = ImVec2(20.f, 14.f);
}

void Gui::RebindStyle()
{
	if (style == ColourStyle::REBIND_STYLE)
		return;
	style = ColourStyle::REBIND_STYLE;
	ImGui::StyleColorsDark();

	auto &style = ImGui::GetStyle();
	style.GrabRounding = 12.f;
	style.WindowRounding = 12.f;
	style.ScrollbarRounding = 12.f;
	style.FrameRounding = 2.f;
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
	style.FramePadding = ImVec2(4.f, 2.f);
	style.FramePadding.x = 4.f;
	style.FrameBorderSize = 2.f;
	style.WindowPadding = ImVec2(20.f, 14.f);
}

void Gui::DarkStyle()
{
	if (style == ColourStyle::DARK_STYLE)
		return;
	style = ColourStyle::DARK_STYLE;
	ImGui::StyleColorsDark();

	auto& style = ImGui::GetStyle();

	style.GrabRounding = 1.f;
	style.WindowRounding = 12.f;
	style.ScrollbarRounding = 3.f;
	style.FrameRounding = 12.f;
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
	style.FramePadding = ImVec2(4.f, 2.f);
	style.FramePadding.x = 4.f;
	style.FrameBorderSize = 2.f;
	style.WindowPadding = ImVec2(20.f, 14.f);
	ImVec4* colors = style.Colors;
	ImVec4 imWhite(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_Text] = imWhite;
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.43f, 0.50f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.29f, 0.29f, 0.29f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.45f, 0.45f, 0.45f, 0.40f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.65f, 0.65f, 0.65f, 0.67f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.30f, 0.30f, 0.30f, 0.40f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	colors[ImGuiCol_Tab] = ImLerp(colors[ImGuiCol_Header], colors[ImGuiCol_TitleBgActive], 0.80f);
	colors[ImGuiCol_TabHovered] = colors[ImGuiCol_HeaderHovered];
	colors[ImGuiCol_TabActive] = ImLerp(colors[ImGuiCol_HeaderActive], colors[ImGuiCol_TitleBgActive], 0.60f);
	colors[ImGuiCol_TabUnfocused] = ImLerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.80f);
	colors[ImGuiCol_TabUnfocusedActive] = ImLerp(colors[ImGuiCol_TabActive], colors[ImGuiCol_TitleBg], 0.40f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);   // Prefer using Alpha=1.0 here
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);   // Prefer using Alpha=1.0 here
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.65f, 0.65f, 0.65f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}

void Gui::RestoreDeviceObjects(RenderPlatform* r,PlatformWindow *w)
{
	renderPlatform=r;
	if(!r)
		return;
	SAFE_DELETE(vrHeadsetIconTexture);
	vrHeadsetIconTexture=renderPlatform->CreateTexture("headsetIconLine.png");
	imgui_vrHeadsetIconTexture.height=20;
	imgui_vrHeadsetIconTexture.width=20;
	imgui_vrHeadsetIconTexture.mip=0;
	imgui_vrHeadsetIconTexture.slice=0;
	imgui_vrHeadsetIconTexture.texture=vrHeadsetIconTexture;

	{
		SAFE_DELETE(viveControllerTexture);
		viveControllerTexture = renderPlatform->CreateTexture("viveController.png");
		imgui_ViveControllerTexture.height = 347;
		imgui_ViveControllerTexture.width = 265;
		imgui_ViveControllerTexture.mip = 0;
		imgui_ViveControllerTexture.slice = 0;
		imgui_ViveControllerTexture.texture = viveControllerTexture;
		
	}
	for(uint16_t i=0;i<VK_MAX;i++)
	{
		KeysDown[i]=false;
	}
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	DarkStyle();
	SetPlatformWindow(w);

#ifdef _MSC_VER
	ImGui_ImplWin32_Init(GetActiveWindow());
	ImGui_ImplWin32_SetFunction_GetCursorPos(&Gui::GetCursorPos);
#endif
#ifdef __ANDROID__
	ImGui_ImplAndroid_Init(platformWindow);
#endif
	ImGui_ImplPlatform_Init(r);
	// NB: Transfer ownership of 'ttf_data' to ImFontAtlas, unless font_cfg_template->FontDataOwnedByAtlas == false. Owned TTF buffer will be deleted after Build().
	platform::core::FileLoader *fileLoader=platform::core::FileLoader::GetFileLoader();
	std::vector<std::string> texture_paths;
	texture_paths.push_back("textures");
	texture_paths.push_back("fonts");
	texture_paths.push_back("assets/textures");
	texture_paths.push_back("assets/fonts");
	ImGuiIO& io = ImGui::GetIO();
	auto AddFont=[texture_paths,fileLoader,&io](const char *font_filename,float size_pixels=32.f,ImFontConfig *config=nullptr,const ImWchar *ranges=nullptr)->ImFont*
	{
		int idx = fileLoader->FindIndexInPathStack(font_filename, texture_paths);
		if(idx<=-2)
		{
			TELEPORT_CERR<<font_filename<<" not found.\n";
			return nullptr;
		}
		void *ttf_data;
		unsigned int ttf_size;
		std::string full_path=((texture_paths[idx]+"/")+ font_filename);
		//return io.Fonts->AddFontFromFileTTF(full_path.c_str(),size_pixels,config,ranges);
		fileLoader->AcquireFileContents(ttf_data,ttf_size,full_path.c_str(),false);
		return io.Fonts->AddFontFromMemoryTTF(ttf_data,ttf_size,size_pixels,config,ranges);
	};
	defaultFont=AddFont("Exo2-SemiBold.ttf");
	// NOTE: imgui expects the ranges pointer to be VERY persistent. Not clear how persistent exactly, but it is used out of the scope of
	// this function. So we have to have a persistent variable for it!
	static ImVector<ImWchar> glyph_ranges1;
	{
		ImFontConfig config;
		config.MergeMode = true;
		config.GlyphMinAdvanceX = 32.0f; 
		ImFontGlyphRangesBuilder builder;
		builder.AddChar('a');
		builder.AddText(ICON_FK_SEARCH);
		builder.AddText(ICON_FK_LONG_ARROW_LEFT);		
		builder.AddText(ICON_FK_BOOK);			
		builder.AddText(ICON_FK_BOOKMARK);		
		builder.AddText(ICON_FK_FOLDER_O);			
		builder.AddText(ICON_FK_FOLDER_OPEN_O);		
		builder.AddText(ICON_FK_COG);				
		builder.AddText(ICON_FK_TIMES);
		builder.AddText(ICON_FK_RENREN);
		builder.AddText(ICON_FK_ARROW_LEFT);									
		builder.AddText(ICON_FK_LONG_ARROW_RIGHT);
		builder.AddText(ICON_FK_PLUS_SQUARE_O);
		builder.AddText(ICON_FK_MINUS_SQUARE_O);
		builder.AddText(ICON_FK_PLUS);
		builder.AddText(ICON_FK_MINUS);
		builder.AddText(ICON_FK_CHEVRON_RIGHT);
		builder.AddText(ICON_FK_CHEVRON_DOWN);
		
										
		builder.BuildRanges(&glyph_ranges1);							// Build the final result (ordered ranges with all the unique characters submitted)
		AddFont("forkawesome-webfont.ttf",32.f,&config,glyph_ranges1.Data);
		io.Fonts->Build();										// Build the atlas while 'ranges' is still in scope and not deleted.
	}
	std::vector<int> fontSizes = {12, 18};
	static std::vector<ImVector<ImWchar>> glyph_ranges2;
	glyph_ranges2.resize(fontSizes.size());
	for(int i=0;i<fontSizes.size();i++)
	{
		int sz=fontSizes[i];
		fontInter[sz] = AddFont("Inter-Medium.ttf", float(sz));
		ImFontConfig config;
		config.MergeMode = true;
		config.GlyphMinAdvanceX = 20.0f;
		ImFontGlyphRangesBuilder builder;
		builder.AddChar('a');
		builder.AddText(ICON_FK_SEARCH);
		builder.AddText(ICON_FK_LONG_ARROW_LEFT);
		builder.AddText(ICON_FK_BOOK);
		builder.AddText(ICON_FK_BOOKMARK);
		builder.AddText(ICON_FK_FOLDER_O);
		builder.AddText(ICON_FK_FOLDER_OPEN_O);
		builder.AddText(ICON_FK_COG);
		builder.AddText(ICON_FK_TIMES);
		builder.AddText(ICON_FK_RENREN);
		builder.AddText(ICON_FK_ARROW_LEFT);
		builder.AddText(ICON_FK_LONG_ARROW_RIGHT);
		builder.AddText(ICON_FK_PLUS_SQUARE_O);
		builder.AddText(ICON_FK_MINUS_SQUARE_O);
		builder.AddText(ICON_FK_PLUS);
		builder.AddText(ICON_FK_MINUS);
		builder.AddText(ICON_FK_CHEVRON_RIGHT);
		builder.AddText(ICON_FK_CHEVRON_DOWN);
		builder.BuildRanges(&glyph_ranges2[i]);							// Build the final result (ordered ranges with all the unique characters submitted)
		AddFont("forkawesome-webfont.ttf", 20.f, &config, glyph_ranges2[i].Data);
		io.Fonts->Build();										// Build the atlas while 'ranges' is still in scope and not deleted.
	}
	io.ConfigFlags|=ImGuiConfigFlags_IsTouchScreen;// VR more like a touch screen.
	io.HoverDelayNormal = 1.0f;
}

void Gui::InvalidateDeviceObjects()
{
	if(renderPlatform)
	{
	#ifdef _MSC_VER
		ImGui_ImplWin32_Shutdown();
	#endif
		ImGui_ImplPlatform_Shutdown();
		ImGui::DestroyContext();
		imgui_vrHeadsetIconTexture.texture=0;
		SAFE_DELETE(vrHeadsetIconTexture);
		imgui_ViveControllerTexture.texture = 0;
		SAFE_DELETE(viveControllerTexture);
		renderPlatform=nullptr;

	}
}

void Gui::LoadShaders()
{
	ImGui_ImplPlatform_LoadShaders();
}

void Gui::RecompileShaders()
{
	ImGui_ImplPlatform_RecompileShaders();
}

void Gui::ShowHide()
{
	if(visible)
	{
		Hide();
	}
	else
	{
		Show();
	}
}

void Gui::Show()
{
#ifdef _MSC_VER
	ImGui_ImplWin32_SetFunction_GetCursorPos(&Gui::GetCursorPos);
#endif
	visible			= true;
	reset_menu_pos	=true;
	keys_pressed.clear();
}

void Gui::Hide()
{
#ifdef _MSC_VER
	ImGui_ImplWin32_SetFunction_GetCursorPos(nullptr);
#endif
	auto &config=client::Config::GetInstance();
	config.SaveOptions();
	visible = false;
}

void Gui::SetScaleMetres()
{
//	visible = false;
}

void Gui::ShowFont()
{
	ImGuiIO& io = ImGui::GetIO();
	ImFontAtlas* atlas = io.Fonts;
	for (int i = 0; i < atlas->Fonts.Size; i++)
	{
		ImFont* font = atlas->Fonts[i];
		using namespace ImGui;
		if (ImGui::TreeNode("Glyphs", "Glyphs (%d)", font->Glyphs.Size))
		{
			ImDrawList* draw_list = GetWindowDrawList();
			const ImU32 glyph_col = GetColorU32(ImGuiCol_Text);
			const float cell_size = font->FontSize * 1;
			const float cell_spacing = GetStyle().ItemSpacing.y;
			for (unsigned int base = 0; base <= IM_UNICODE_CODEPOINT_MAX; base += 256)
			{
				// Skip ahead if a large bunch of glyphs are not present in the font (test in chunks of 4k)
				// This is only a small optimization to reduce the number of iterations when IM_UNICODE_MAX_CODEPOINT
				// is large // (if ImWchar==ImWchar32 we will do at least about 272 queries here)
				if (!(base & 4095) && font->IsGlyphRangeUnused(base, base + 4095))
				{
					base += 4096 - 256;
					continue;
				}

				int count = 0;
				for (unsigned int n = 0; n < 256; n++)
					if (font->FindGlyphNoFallback((ImWchar)(base + n)))
						count++;
				if (count <= 0)
					continue;
				if (!ImGui::TreeNode((void*)(intptr_t)base, "U+%04X..U+%04X (%d %s)", base, base + 255, count, count > 1 ? "glyphs" : "glyph"))
					continue;

				// Draw a 16x16 grid of glyphs
				ImVec2 base_pos = GetCursorScreenPos();
				for (unsigned int n = 0; n < 256; n++)
				{
					// We use ImFont::RenderChar as a shortcut because we don't have UTF-8 conversion functions
					// available here and thus cannot easily generate a zero-terminated UTF-8 encoded string.
					ImVec2 cell_p1(base_pos.x + (n % 16) * (cell_size + cell_spacing), base_pos.y + (n / 16) * (cell_size + cell_spacing));
					ImVec2 cell_p2(cell_p1.x + cell_size, cell_p1.y + cell_size);
					const ImFontGlyph* glyph = font->FindGlyphNoFallback((ImWchar)(base + n));
					draw_list->AddRect(cell_p1, cell_p2, glyph ? IM_COL32(255, 255, 255, 100) : IM_COL32(255, 255, 255, 50));
					if (glyph)
						font->RenderChar(draw_list, cell_size, cell_p1, glyph_col, (ImWchar)(base + n));
					if (glyph && IsMouseHoveringRect(cell_p1, cell_p2))
					{
						BeginTooltip();
						Text("Codepoint: U+%04X", base + n);
						Separator();
						Text("Visible: %d", glyph->Visible);
						Text("AdvanceX: %.1f", glyph->AdvanceX);
						Text("Pos: (%.2f,%.2f)->(%.2f,%.2f)", glyph->X0, glyph->Y0, glyph->X1, glyph->Y1);
						Text("UV: (%.3f,%.3f)->(%.3f,%.3f)", glyph->U0, glyph->V0, glyph->U1, glyph->V1);
						EndTooltip();
					}
				}
				Dummy(ImVec2((cell_size + cell_spacing) * 16, (cell_size + cell_spacing) * 16));
				TreePop();
			}
			TreePop();
		}
		TreePop();
	}
}

void Gui::TreeNode(const std::shared_ptr<clientrender::Node> n,const char *search_text)
{
	auto geometryCache=clientrender::GeometryCache::GetGeometryCache(cache_uid);
	const clientrender::Node *node=n.get();
	if (!node)
		return;
	bool has_children	=node->GetChildren().size()!=0;
	std::string str		=(std::to_string(n->id)+" ")+ node->name;
	bool open			= false;
	bool show			= true;
	if (search_text)
	{
		if (str.find(search_text) >= str.length())
		{
			show = false;
		}
	}
	if (show)
	{
		if(!n->IsVisible())
		{
			ImVec4 grey= ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Text,grey);
		}
		open = ImGui::TreeNodeEx(str.c_str(), ImGuiTreeNodeFlags_OpenOnArrow|(has_children ? 0 : ImGuiTreeNodeFlags_Leaf));
		if(!n->IsVisible())
           ImGui::PopStyleColor();
	}
	else
	{
		open = true;
	}
	if (ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Save..."))
		{
			geometryCache->SaveNodeTree(n);
		}
		ImGui::EndPopup();
	}
	if (ImGui::IsItemClicked())
	{
		if(!show_inspector)
			show_inspector=true;
		Select(n->id);
	}
	if(open)
	{
		for(const auto &r:node->GetChildren())
		{
			TreeNode(r.lock(), search_text);
		}
		ImGui::TreePop();
	}
}

int Gui::GetCursorPos(long p[2]) 
{
	ImGuiIO& io = ImGui::GetIO();
	p[0]=(long)io.MousePos.x;
	p[1]=(long)io.MousePos.y;
	return 1;
}

static int in_debug_gui = 0;
vec2 mouse;
bool mouseButtons[5]={false,false,false,false};
void Gui::SetDebugGuiMouse(vec2 m,bool leftButton)
{
	mouse=m;
	if(mouseButtons[0]!=leftButton)
	{
		mouseButtons[0] = leftButton;
	}
}
#define VK_KEYPAD_ENTER      (VK_RETURN + 256)

int VirtualKeyToChar(unsigned long long key)
{
#ifdef _MSC_VER
#if 0
	std::wstring out;
	int scan;
	WCHAR buff[32];
	UINT size = 32;

	scan = MapVirtualKeyEx((unsigned)key, 0, GetKeyboardLayout(0));
	BYTE m_keys[32];
	int numChars = ToUnicodeEx((unsigned)key, scan, m_keys, buff, size, 0, GetKeyboardLayout(0));
	return buff[0];
#else
	switch (key)
	{
	case VK_TAB: 
	case VK_LEFT: 
	case VK_RIGHT: 
	case VK_UP: 
	case VK_DOWN: 
	case VK_PRIOR: 
	case VK_NEXT: 
	case VK_HOME: 
	case VK_END: 
	case VK_INSERT: 
	case VK_DELETE: 
	case VK_BACK: 
	case VK_SPACE: 
	case VK_RETURN: 
	case VK_ESCAPE: 
	case VK_OEM_7: 
	case VK_OEM_COMMA: 
	case VK_OEM_MINUS: 
	case VK_OEM_PERIOD: 
	case VK_OEM_2: 
	case VK_OEM_1: 
	case VK_OEM_PLUS: 
	case VK_OEM_4: 
	case VK_OEM_5: 
	case VK_OEM_6: 
	case VK_OEM_3: 
	case VK_CAPITAL: 
	case VK_SCROLL: 
	case VK_NUMLOCK: 
	case VK_SNAPSHOT: 
	case VK_PAUSE: 
	case VK_NUMPAD0: 
	case VK_NUMPAD1: 
	case VK_NUMPAD2: 
	case VK_NUMPAD3: 
	case VK_NUMPAD4: 
	case VK_NUMPAD5: 
	case VK_NUMPAD6: 
	case VK_NUMPAD7: 
	case VK_NUMPAD8: 
	case VK_NUMPAD9: 
	case VK_DECIMAL: 
	case VK_DIVIDE: 
	case VK_MULTIPLY: 
	case VK_SUBTRACT: 
	case VK_ADD: 
	case VK_KEYPAD_ENTER:
	case VK_LSHIFT: 
	case VK_LCONTROL: 
	case VK_LMENU: 
	case VK_LWIN: 
	case VK_RSHIFT: 
	case VK_RCONTROL: 
	case VK_RMENU: 
	case VK_RWIN: 
	case VK_APPS: 
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
	case 'G':
	case 'H':
	case 'I':
	case 'J':
	case 'K':
	case 'L':
	case 'M':
	case 'N':
	case 'O':
	case 'P':
	case 'Q':
	case 'R':
	case 'S':
	case 'T':
	case 'U':
	case 'V':
	case 'W':
	case 'X':
	case 'Y':
	case 'Z':
	default:
		break;
	}
#endif
#endif
	return (int)key;
}
#ifdef _MSC_VER
static bool IsVkDown(int vk)
{
	return (::GetKeyState(vk) & 0x8000) != 0;
}
#endif
// There is no distinct VK_xxx for keypad enter, instead it is VK_RETURN + KF_EXTENDED, we assign it an arbitrary value to make code more readable (VK_ codes go up to 255)
#define IM_VK_KEYPAD_ENTER      (VK_RETURN + 256)
void Gui::OnKeyboard(unsigned wParam, bool is_key_down)
{
	ImGuiIO& io = ImGui::GetIO();
	if (!is_key_down)
	{
		int k = VirtualKeyToChar(wParam);
		keys_pressed.push_back(tolower(k));
	}
	auto& config = client::Config::GetInstance();
#if 0
	if (config.options.gui2D)
	{
		if (wParam < 256)
		{
			// Submit modifiers
			io.AddKeyEvent(ImGuiMod_Ctrl, IsVkDown(VK_CONTROL));
			io.AddKeyEvent(ImGuiMod_Shift, IsVkDown(VK_SHIFT));
			io.AddKeyEvent(ImGuiMod_Alt, IsVkDown(VK_MENU));
			io.AddKeyEvent(ImGuiMod_Super, IsVkDown(VK_APPS));

			// Obtain virtual key code
			int vk = (int)wParam;

			// Submit key event
			const ImGuiKey key = ImGui_ImplWin32_VirtualKeyToImGuiKey(vk);
			if (key != ImGuiKey_None)
				io.AddKeyEvent(key, is_key_down);

			// Submit individual left/right modifier events
			if (vk == VK_SHIFT)
			{
				// Important: Shift keys tend to get stuck when pressed together, missing key-up events are corrected in ImGui_ImplWin32_ProcessKeyEventsWorkarounds()
				if (IsVkDown(VK_LSHIFT) == is_key_down) { io.AddKeyEvent(ImGuiKey_LeftShift, is_key_down ); }
				if (IsVkDown(VK_RSHIFT) == is_key_down) { io.AddKeyEvent(ImGuiKey_RightShift, is_key_down ); }
			}
			else if (vk == VK_CONTROL)
			{
				if (IsVkDown(VK_LCONTROL) == is_key_down) { io.AddKeyEvent(ImGuiKey_LeftCtrl, is_key_down); }
				if (IsVkDown(VK_RCONTROL) == is_key_down) { io.AddKeyEvent(ImGuiKey_RightCtrl, is_key_down); }
			}
			else if (vk == VK_MENU)
			{
				if (IsVkDown(VK_LMENU) == is_key_down) { io.AddKeyEvent(ImGuiKey_LeftAlt, is_key_down); }
				if (IsVkDown(VK_RMENU) == is_key_down) { io.AddKeyEvent(ImGuiKey_RightAlt, is_key_down); }
			}
		}
	}
#endif
}
#include <json.hpp>
#include <NodeComponents/SubSceneComponent.h>
using json = nlohmann::json;
json j;
void Gui::OverlayMenu(GraphicsDeviceContext &deviceContext)
{
	static bool loaded=false;
	static std::string binding_filename = "example_binding.json";
	if(!loaded)
	{
		auto *fileLoader = platform::core::FileLoader::GetFileLoader();
		if (fileLoader)
		{
			void *ptr = nullptr;
			unsigned bytelen = 0;
			auto &config = client::Config::GetInstance();
			std::string filename = config.GetStorageFolder() + "/config/"s + binding_filename;
			fileLoader->AcquireFileContents(ptr, bytelen, filename.c_str(), true);
			if (ptr)
			{
				try
				{
					j = json::parse((const char *)ptr);
					fileLoader->ReleaseFileContents(ptr);
					loaded = true;
				}
				catch(...)
				{
				}
			}
		}
	}
	RebindStyle();
	// POSSIBLY can't use ImGui's own Win32 NewFrame as we may want to override the mousepos.
#ifdef _MSC_VER
	ImGui_ImplWin32_NewFrame();
#endif
#ifdef __ANDROID__
	ImGui_ImplAndroid_NewFrame();
#endif
	auto vp = renderPlatform->GetViewport(deviceContext, 0);
	ImGuiIO &io = ImGui::GetIO();
	// MUST do this, or ImGui won't know the "screen" size.
	io.DisplaySize = ImVec2((float)(vp.w), (float)(vp.h));
	ImGui_ImplPlatform_NewFrame(false, vp.w, vp.h);
	ImGui::NewFrame();
	
	// The mouse pos is the position where the controller's pointing direction intersects the OpenXR overlay surface.
	ImGui_ImplPlatform_SetMousePos((int)((0.5f + mouse.x) * float(vp.w)), (int)((0.5f - mouse.y) * float(vp.h)), vp.w, vp.h);

	ImGui_ImplPlatform_SetMouseDown(0, mouseButtons[0]);
	ImGui_ImplPlatform_SetMouseDown(1, mouseButtons[1]);
	ImGui_ImplPlatform_SetMouseDown(2, mouseButtons[2]);

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize |
					  ImGuiWindowFlags_NoMove |
					  ImGuiWindowFlags_NoCollapse |
					  ImGuiWindowFlags_NoScrollbar;

	ImVec2 size((float)vp.w,(float)vp.h);
	ImGui::SetNextWindowPos(ImVec2(0, 0)); // always at the window origin
	ImGui::SetNextWindowSizeConstraints(size, size);
	ImGui::SetNextWindowSize(size);

	ImGui::PushFont(fontInter[18]);
	static vec4 white(1.f, 1.f, 1.f, 1.f);
	try
	{
		if (ImGuiBegin("Rebind Inputs", nullptr, window_flags))
		{
			ImGui::Text("%s",binding_filename.c_str());
			json &actionSets = j["actionSets"];
			static int chosenSet = -1;
			static ImGuiTableFlags flags =  0 ;
			ImGui::BeginTable("actionSets", 3, flags,ImVec2(float(vp.w),float(vp.h)),float(vp.w));
			ImGui::TableSetupColumn("Action Set", ImGuiTableColumnFlags_WidthFixed,float(vp.w)*0.12f);
			ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, float(vp.w) * 0.28f);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, float(vp.w) * 0.4f);
			ImGui::TableHeadersRow();
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			for (int i=0;i<actionSets.size();i++)
			{
				json &actionSet = actionSets[i];
				std::string actionSetName = actionSet["name"];
				std::string actionSetLocalizedName = actionSet["localizedName"];
				if (ImGui::Button(actionSetLocalizedName.c_str()))
				{
					if(chosenSet!=i)
						chosenSet=i;
					else
						chosenSet=-1;
				}
		
			}
			ImGui::TableNextColumn();
			if (chosenSet>=0&&chosenSet<actionSets.size())
			{
				json &actionSet=actionSets[chosenSet];
				int priority = actionSet["priority"];
				ImGui::Text("Priority: ");
				ImGui::SameLine();
				if(ImGui::SliderInt("##priority",&priority,0,10))
					actionSet["priority"] = priority;
				json &actions = actionSet["actions"];
				static int choseAction=-1;
				int i=0;
				for (json::iterator actionPair = actions.begin(); actionPair != actions.end(); ++actionPair,i++)
				{
					json &action = actionPair.value();
					std::string actionName = action["name"];
					std::string localizedName = action["localizedName"];
					if (ImGui::Button(i == choseAction ? ICON_FK_CHEVRON_DOWN : ICON_FK_CHEVRON_RIGHT))
					{
						if (choseAction != i)
							choseAction = i;
						else
							choseAction = -1;
					}
					ImGui::SameLine();
					ImGui::Text("%s",localizedName.c_str());
					if(i==choseAction)
					{
						json &bindingsList = action["bindings"];
						ImVec4 brd = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
						ImGui::PushStyleColor(ImGuiCol_Border, brd);
						ImGui::BeginGroup();
						for(json &binding:bindingsList)
						{
							std::string bindingName = binding.get<std::string>();
							ImGui::Spacing();
							ImGui::SameLine();
							if (ImGui::SmallButton( ICON_FK_MINUS ))
							{
							}
							ImGui::SameLine();
							ImGui::Text("Binding: ");
							ImGui::SameLine();
							ImGui::Button(bindingName.c_str(),ImVec2(150.f,20.f));
						}
						ImGui::Spacing();
						ImGui::SameLine();
						if (ImGui::SmallButton(ICON_FK_PLUS))
						{
						}
						ImGui::EndGroup();
						ImGui::PopStyleColor();
					}
				}
			}
			ImGui::TableNextColumn();
			float imgWidth = ImGui::GetColumnWidth(2);
			ImVec2 imgSize(347.f, 265.f);
			imgSize.y *= imgWidth / imgSize.x;
			imgSize.x = imgWidth;
			ImVec2 availableSize=ImGui::GetContentRegionAvail();
			if(imgSize.y>availableSize.y)
			{
				imgSize.x *= availableSize.y / imgSize.y;
				imgSize.y = availableSize.y;
			}
			ImGui::Image(&imgui_ViveControllerTexture, imgSize);
			ImGui::EndTable();
			ImGuiEnd();
		}
	}
	catch(...)
	{
	}
	ImGui::PopFont();
	ImVec2 window_pos = ImGui::GetWindowPos();
	ImVec2 window_size = ImGui::GetWindowSize();
	ImVec2 window_center = ImVec2(window_pos.x + window_size.x * 0.5f, window_pos.y + window_size.y * 0.5f);

	ImVec2 mouse_pos = io.MousePos;
	if (mouseButtons[0])
		ImGui::GetForegroundDrawList()->AddCircleFilled(mouse_pos, 3.f, IM_COL32(255, 90, 90, 200), 16);
	else
		ImGui::GetForegroundDrawList()->AddCircleFilled(mouse_pos, 2.f, IM_COL32(90, 255, 90, 200), 16);
	ImGui::Render();
	ImGui_ImplPlatform_RenderDrawData(deviceContext, ImGui::GetDrawData());
}

void Gui::BeginFrame(GraphicsDeviceContext &deviceContext)
{
	// POSSIBLY can't use ImGui's own Win32 NewFrame as we may want to override the mousepos.
#ifdef _MSC_VER
	ImGui_ImplWin32_NewFrame();
#endif
#ifdef __ANDROID__
	ImGui_ImplAndroid_NewFrame();
#endif
	auto vp = renderPlatform->GetViewport(deviceContext, 0);
	ImGuiIO &io = ImGui::GetIO();
	// MUST do this, or ImGui won't know the "screen" size.
	io.DisplaySize = ImVec2((float)(vp.w), (float)(vp.h));
	ImGui_ImplPlatform_NewFrame(false, vp.w, vp.h);
	ImGui::NewFrame();
	bool in3D = openXR.IsSessionActive();
	if (in3D)
	{
		// The mouse pos is the position where the controller's pointing direction intersects the OpenXR overlay surface.
		ImGui_ImplPlatform_SetMousePos((int)((0.5f + mouse.x) * float(vp.w)), (int)((0.5f - mouse.y) * float(vp.h)), vp.w, vp.h);

		ImGui_ImplPlatform_SetMouseDown(0, mouseButtons[0]);
		ImGui_ImplPlatform_SetMouseDown(1, mouseButtons[1]);
		ImGui_ImplPlatform_SetMouseDown(2, mouseButtons[2]);
	}
}

void Gui::BeginDebugGui(GraphicsDeviceContext& deviceContext)
{
	DarkStyle();
	ImGuiWindowFlags window_flags = 0;
	auto vp = renderPlatform->GetViewport(deviceContext, 0);
	ImVec2 size((float)vp.w, (float)vp.h);
	bool in3D = openXR.IsSessionActive();
	if(in3D)
	{
		size.x*=0.6f;
		ImGui::SetNextWindowPos(ImVec2(0, 0)); // always at the window origin
		ImGui::SetNextWindowSizeConstraints(size, size);

		window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse ;
	}
	if (in_debug_gui != 0)
	{
		return;
	}
	ImGui::PushFont(fontInter[18]);
	if (ImGuiBegin("Teleport VR", nullptr, window_flags))
		in_debug_gui++;
/*
	static vec4 white(1.f, 1.f, 1.f, 1.f);
	if(sessionClient)
	{
		auto status = sessionClient->GetConnectionStatus();
		auto streamingStatus = sessionClient->GetStreamingConnectionState();
		const auto& setup = sessionClient->GetSetupCommand();
		ImGui::BeginTable("serverstats", 2);
		ImGui::TableSetupColumn("name1", ImGuiTableColumnFlags_WidthFixed, 200.0f);
		ImGui::TableSetupColumn("val11", ImGuiTableColumnFlags_WidthStretch, 400.0f);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Server");
		ImGui::TableNextColumn();
		ImGui::Text(fmt::format("{0}:{1}", sessionClient->GetServerIP().c_str(), sessionClient->GetPort()).c_str());
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Server Session Id");
		ImGui::TableNextColumn();
		ImGui::Text("%llu", setup.session_id, white);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Session Status");
		ImGui::TableNextColumn();
		ImGui::Text(teleport::client::StringOf(status), white);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Streaming Status");
		ImGui::TableNextColumn();
		ImGui::Text( avs::stringOf(streamingStatus), white);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Latency");
		ImGui::TableNextColumn();
		ImGui::Text("%4.4f ms", sessionClient->GetLatencyMs(), white);
		ImGui::EndTable();
	}*/
}

void Gui::LinePrint(const std::string& str, const float* clr)
{
	LinePrint(str.c_str());
}

void Gui::LinePrint(const char* txt,const float *clr)
{
	if (clr == nullptr)
		ImGui::Text("%s",txt);
	else
		ImGui::TextColored(*(reinterpret_cast<const ImVec4*>(clr)), "%s",txt);
}

std::map<uint64_t,ImGui_ImplPlatform_TextureView> drawTextures;
void Gui::DelegatedDrawTexture(platform::crossplatform::GraphicsDeviceContext &deviceContext, platform::crossplatform::Texture* texture,float mip,int slice)
{
	platform::crossplatform::Texture *t=texture;
	auto vp=renderPlatform->GetViewport(deviceContext,0);
	if(texture->IsCubemap())
	{
		renderPlatform->DrawCubemap(deviceContext,texture,0.f,0.f,1.0f,1.0f,1.0f,mip);
	}
	else
	{
		renderPlatform->DrawTexture(deviceContext,0,0,vp.w,vp.h,t,1.0f,false,1.0f,false,{0,0},{0,0},mip,slice);
	}
}

void Gui::DrawTexture(const Texture* texture,float m,int slice)
{
	if (!texture)
		return;
	if (!texture->IsValid())
		return;
	static float mip = 0;
	if (m < 0)
	{
		ImGui::SliderFloat("Mip", &mip, 0.f, 10.0f);
		m = mip;
	}

	int width = texture->width;
	int height = texture->length;
	width=std::max(width,256);
	if(width!=texture->width)
	{
		height=(width*texture->length)/texture->width;
	}
	const char* name = texture->GetName();
	const float aspect = static_cast<float>(width) / static_cast<float>(height);
	const ImVec2 regionSize =  ImGui::GetContentRegionAvail();
	const ImVec2 textureSize = ImVec2(static_cast<float>(width), static_cast<float>(height));
	float showWidth=std::min(regionSize.x, textureSize.x);
	const ImVec2 size = ImVec2(showWidth, float(showWidth)/aspect);
		
	platform::crossplatform::RenderDelegate drawTexture=std::bind(&Gui::DelegatedDrawTexture,this,std::placeholders::_1,const_cast<Texture*>(texture),m,slice);
	ImGui_ImplPlatform_DrawTexture(drawTexture, texture->GetName(), m, slice, (int)showWidth,(int)size.y);
}

static void DoRow(const char* title, const char* text, ...)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::Text("%s", title);
	ImGui::TableNextColumn();
	va_list args;
	va_start(args, text);
	va_list args2;
	va_copy(args2, args);
	size_t bufferSize = static_cast<size_t>(vsnprintf(nullptr, 0, text, args2)) + 1;
	va_end(args2);
	char* bufferData = new char[bufferSize];
	vsnprintf(bufferData, bufferSize, text, args);
	va_end(args);
	ImGui::Text("%s", bufferData);
	delete[] bufferData;
};

static void DoRow(const char* title, vec3 pos)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::Text("%s", title);
	ImGui::TableNextColumn();
	ImGui::Text("%2.2f", pos.x);
	ImGui::TableNextColumn();
	ImGui::Text("%2.2f", pos.y);
	ImGui::TableNextColumn();
	ImGui::Text("%2.2f", pos.z);
	ImGui::TableNextColumn();
};

static void DoRow(const char* title, vec4 q)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::Text("%s",title);
	ImGui::TableNextColumn();
	ImGui::Text("%2.2f", q.x);
	ImGui::TableNextColumn();
	ImGui::Text("%2.2f", q.y);
	ImGui::TableNextColumn();
	ImGui::Text("%2.2f", q.z);
	ImGui::TableNextColumn();
	ImGui::Text("%2.2f", q.w);
	ImGui::TableNextColumn();
};

static std::pair<std::string, std::string> GetCurrentDateTimeStrings()
{
	auto now = std::chrono::system_clock::now();
	time_t now_t = std::chrono::system_clock::to_time_t(now);
	tm* t = localtime(&now_t);
	const char* leadingZero0 = (t->tm_mon < 10) ? "0" : "";
	const char* leadingZero1 = (t->tm_mday < 10) ? "0" : "";
	std::string dateStr = std::to_string(1900 + t->tm_year) + "/" + leadingZero0 + std::to_string(1 + t->tm_mon) + "/" + leadingZero1 + std::to_string(t->tm_mday);
	const char* leadingZero2 = (t->tm_hour < 10) ? "0" : "";
	const char* leadingZero3 = (t->tm_min < 10) ? "0" : "";
	std::string timeStr = leadingZero2 + std::to_string(t->tm_hour) + ":" + leadingZero3 + std::to_string(t->tm_min);
	return { dateStr, timeStr };
}

void Gui::EndDebugGui(GraphicsDeviceContext& deviceContext)
{
	if(in_tabs)
		ImGui::EndTabBar();
		in_tabs=false;
	if (in_debug_gui != 1)
	{
		return;
	}
	ImGuiEnd();
	//ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
	{
		const float PAD = 10.0f;
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
		ImVec2 work_size = viewport->WorkSize;
		ImVec2 window_pos, window_pos_pivot;
		window_pos.x = (work_pos.x + work_size.x - PAD);
		window_pos.y = (work_pos.y + PAD);
		window_pos_pivot.x = 1.0f;
		window_pos_pivot.y = 0.0f;
		ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
		//window_flags |= ImGuiWindowFlags_NoMove;
	}
	auto geometryCache=clientrender::GeometryCache::GetGeometryCache(cache_uid);
	if(geometryCache&&show_inspector)
	{
		ImGuiWindowFlags window_flags =ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_MenuBar;
			//| ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;|ImGuiWindowFlags_NoDecoration
		if (ImGuiBegin("Properties", &show_inspector, window_flags))
		{
			if (ImGui::BeginMenuBar())
			{
				ImGui::BeginDisabled(selection_cursor==0);
				if(ImGui::ArrowButton("Back",ImGuiDir_Left))
				{
					SelectPrevious();
				}
				ImGui::EndDisabled();
				ImGui::BeginDisabled(selection_cursor+1>=selection_history.size());
				if(ImGui::ArrowButton("Forward",ImGuiDir_Right))
				{
					SelectNext();
				}
				ImGui::EndDisabled();
				/*if (ImGui::BeginMenu("Examples"))
				{
					ImGui::MenuItem("Example1", NULL, false,true);
					ImGui::EndMenu();
				}*/
				ImGui::EndMenuBar();
			}
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			avs::uid selected_uid=GetSelectedUid();
			std::shared_ptr<const clientrender::Node> selected_node				=geometryCache->mNodeManager->GetNode(selected_uid);
			std::shared_ptr<const clientrender::Material> selected_material		=geometryCache->mMaterialManager.Get(selected_uid);
			std::shared_ptr<const clientrender::Texture> selected_texture		=geometryCache->mTextureManager.Get(selected_uid);
			std::shared_ptr<const clientrender::Animation> selected_animation	=geometryCache->mAnimationManager.Get(selected_uid);

			std::shared_ptr<const clientrender::SubSceneCreate> selected_subscene= geometryCache->mSubsceneManager.Get(selected_uid);
		
			if (selected_node.get())
			{
				vec3 pos = selected_node->GetLocalPosition();
				vec3 gpos = selected_node->GetGlobalPosition();
				vec3 sc = selected_node->GetLocalScale();
				vec4 q = selected_node->GetLocalRotation();
				vec4 gq = selected_node->GetGlobalRotation();
				
				vec3 v=selected_node->GetGlobalVelocity();
		
				vec3 gs = selected_node->GetGlobalScale();
				ImGui::Text("%llu: %s %s", selected_node->id,selected_node->name.c_str(),selected_node->IsHighlighted()?"HIGHLIGHTED":"");
				ImGui::Text("owners %lu", selected_node.use_count());
				avs::uid gi_uid=selected_node->GetGlobalIlluminationTextureUid();
				if (ImGui::BeginTable("selected", 2))
				{
					ImGui::TableNextColumn();
					ImGui::Text("Hidden");
					ImGui::TableNextColumn();
					bool hidden=!selected_node->IsVisible();
					ImGui::Checkbox("##isHidden", &hidden);
					ImGui::TableNextColumn();
					ImGui::Text("Stationary");
					ImGui::TableNextColumn();
					bool stationary=selected_node->IsStatic();
					ImGui::Checkbox("##isStatic", &stationary);
					ImGui::EndTable();
				}
				if (ImGui::BeginTable("selected1", 5))
				{
					DoRow("GI"			,"%d", gi_uid);
					DoRow("Local Pos"	, pos);
					DoRow("Local Rot"	,q);
					DoRow("Local Scale"	,sc);
					ImGui::Separator();
					DoRow("Global Pos"	,gpos);
					DoRow("Global Rot"	,gq);
					DoRow("Global Scale", gs);
					ImGui::EndTable();
				}
				if (ImGui::BeginTable("selected2", 2))
				{
					if(selected_node->GetMesh())
					{
						auto m=selected_node->GetMesh();
						DoRow("Mesh"		,"%d : %s", m->GetMeshCreateInfo().id, m->GetMeshCreateInfo().name.c_str());
						if (ImGui::IsItemClicked())
						{
							Select(m->GetMeshCreateInfo().id);
						}
					}
					const auto &sn=selected_node->GetSkeletonNode();
					if(auto n=sn.lock())
					{
						DoRow("Skeleton Node"	,"%s",n->name.c_str());
						if (ImGui::IsItemClicked())
						{
							Select(n->id);
						}
					}
					if(selected_node->GetSkeletonInstance())
					{
						auto s=selected_node->GetSkeletonInstance();
						DoRow("Skeleton"	,"%d : %s",s->GetBones().size(),s->GetSkeleton()->name.c_str());
						if (ImGui::IsItemClicked())
						{
							//Select(s->GetSkeleton()->id);
						}
					}
					ImGui::EndTable();
				}
				std::shared_ptr<clientrender::SkeletonInstance> skeletonInstance = selected_node->GetSkeletonInstance();
				if (skeletonInstance)
				{
					auto animC=selected_node->GetComponent<clientrender::AnimationComponent>();
					if(animC)
					{
						float anim_time_s=animC->GetCurrentAnimationTimeSeconds();

						ImGui::Text("Animation Time: %3.3f", anim_time_s);
					}
					ImGui::BeginGroup();
					const auto &skeleton	=skeletonInstance->GetSkeleton();
					const auto &bones	=skeletonInstance->GetBones();
					{
						for (auto b : bones)
						{
							if(b->GetParent()==nullptr)
								BoneTreeNode(b, nullptr);
						}
					}
					ImGui::EndGroup();
				}
				for (const auto& m : selected_node->GetMaterials())
				{
					if(m)
					{
						const char *name=m->GetMaterialCreateInfo().name.c_str();
						ImGui::TreeNodeEx(name, flags, "%llu: %s", m->id, name);
						if (ImGui::IsItemClicked())
						{
							Select(m->id);
						}
					}
				}

				auto s = selected_node->GetComponent<clientrender::SubSceneComponent>();
				if (s)
				{
					ImGui::TreeNodeEx("##subsc", flags, " SubScene resource: %ull", s->sub_scene_uid);
					if (ImGui::IsItemClicked())
					{
						Select(s->sub_scene_uid);
					}
				}
			}
			else if (selected_material.get())
			{
				const auto& mci = selected_material->GetMaterialCreateInfo();
				const clientrender::Material::MaterialData& md = selected_material->GetMaterialData();
				ImGui::Text("%llu: %s", selected_material->id, mci.name.c_str());
				if(mci.diffuse.texture.get())
				{
					const char *name=mci.diffuse.texture->GetTextureCreateInfo().name.c_str();
					ImGui::TreeNodeEx(name, flags," Diffuse: %s",  name);
					
					if (ImGui::IsItemClicked())
					{
						Select(mci.diffuse.texture->GetTextureCreateInfo().uid);
					}
				}

				if (mci.combined.texture.get())
				{
					const char *name=mci.combined.texture->GetTextureCreateInfo().name.c_str();
					ImGui::TreeNodeEx(name, flags,"Combined: %s", name);
					ImGui::TreeNodeEx(name, flags,"Metal: %3.3f", md.combinedOutputScalarRoughMetalOcclusion.y);
					ImGui::TreeNodeEx(name, flags,"Roughness: R = %3.3f a + %3.3f",  md.combinedOutputScalarRoughMetalOcclusion.x,md.combinedOutputScalarRoughMetalOcclusion.w);
					
					if (ImGui::IsItemClicked())
					{
						Select( mci.combined.texture->GetTextureCreateInfo().uid);
					}
				}
				if (mci.emissive.texture.get())
				{
					ImGui::TreeNodeEx("", flags,"Emissive: %s", mci.emissive.texture->GetTextureCreateInfo().name.c_str());
					
					if (ImGui::IsItemClicked())
					{
						Select( mci.emissive.texture->GetTextureCreateInfo().uid);
					}
				}
			}
			else if (selected_texture.get())
			{
				const auto& tci = selected_texture->GetTextureCreateInfo();
				ImGui::Text("%llu: %s", tci.uid, tci.name.c_str());
				
				const char* mips[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10","11","12" };
				static int mip_current = 0;
				ImGui::Combo("Mip", &mip_current, mips, IM_ARRAYSIZE(mips));
				const clientrender::Texture* pct = static_cast<const clientrender::Texture*>(selected_texture.get());
				DrawTexture(selected_texture->GetSimulTexture(),(float)mip_current,0);
			}
			else if(selected_animation.get())
			{
				ImGui::Text("%llu: %s", selected_uid,selected_animation->name.c_str());
				if (ImGui::BeginTable("selected", 2))
				{
					auto DoRow=[this](const int index,const char *text, auto ...rest)-> void{
						ImGui::TableNextColumn();
						ImGui::Text("%s",fmt::format("{0}", index).c_str());
						ImGui::TableNextColumn();
						ImGui::Text(text,rest...);
					};
					for(const auto &a:selected_animation->boneKeyframeLists)
					{
						DoRow((int)a.boneIndex,"%d,%d",(int)a.positionKeyframes.size(),(int)a.rotationKeyframes.size());
					}
					ImGui::EndTable();
				}
			}
			else if(selected_subscene.get())
			{
				ImGui::Text("Subscene resource %llu",selected_subscene->uid);

				if (selected_subscene->subscene_uid)
				{
					auto g = clientrender::GeometryCache::GetGeometryCache(selected_subscene->subscene_uid);
					ImGui::TreeNodeEx("##name111", flags, " Subscene Geometry Cache: %ull", selected_subscene->subscene_uid);

					if (g)
					{
						auto rn=g->mNodeManager->GetRootNodes();
						if(rn.size())
						{
							auto b=rn.begin();
							if(b!=rn.end())
							{
								ImGui::Text("Root %s", b->lock()->name.c_str());
							}
						}
					}
				}
			}
			ImGuiEnd();
		}
	}
	in_debug_gui--;
	ImGui::PopFont();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 window_center = ImVec2(window_pos.x + window_size.x * 0.5f, window_pos.y + window_size.y * 0.5f);
	
	ImGuiIO& io = ImGui::GetIO();
	ImVec2 mouse_pos=io.MousePos;
	if(io.MouseClicked[0])
		ImGui::GetForegroundDrawList()->AddCircleFilled(mouse_pos, 3.f, IM_COL32(255, 90, 90, 200), 16);
	else
		ImGui::GetForegroundDrawList()->AddCircleFilled(mouse_pos, 2.f, IM_COL32(90, 255, 90, 200), 16);
}

void Gui::EndFrame(GraphicsDeviceContext &deviceContext)
{
	ImGui::Render();
	ImGui_ImplPlatform_RenderDrawData(deviceContext, ImGui::GetDrawData());
}

void Gui::BoneTreeNode(const std::shared_ptr<clientrender::Bone> n, const char* search_text)
{
	const clientrender::Bone* bone = n.get();
	bool has_children = bone->GetChildren().size() != 0;
	const auto& l = bone->GetLocalTransform().m_Rotation;
	const auto& p = bone->GetLocalTransform().m_Translation;
	const auto& g = bone->GetGlobalTransform().m_Rotation;
	std::string str =fmt::format("{0} {1}, rot {2: .3f},{3: .3f},{4: .3f},{5: .3f}, pos {6: .3f},{7: .3f},{8: .3f}",n->id,bone->name,l.i,l.j,l.k,l.s,p.x,p.y,p.z);
	bool open = false;
	bool show = true;
	if (search_text)
	{
		if (str.find(search_text) >= str.length())
		{
			show = false;
		}
	}
	if (show)
	{
		open = ImGui::TreeNodeEx(bone->name.c_str(), (has_children ? 0 : ImGuiTreeNodeFlags_Leaf),"%s",str.c_str());
	}
	else
	{
		open = true;
	}
	if (open)
	{
		for (const auto& r : bone->GetChildren())
		{
			BoneTreeNode(r.lock(), search_text);
		}
		ImGui::TreePop();
	}
}
void Gui::Textures(const ResourceManager<avs::uid,clientrender::Texture>& textureManager)
{
	ImGui::BeginGroup();
	const auto& ids= textureManager.GetAllIDs();
	for (auto id : ids)
	{
		const auto &texture=textureManager.Get(id);
		ImGui::TreeNodeEx(fmt::format("{0}: {1} ",id, texture->GetTextureCreateInfo().name.c_str()).c_str(),ImGuiTreeNodeFlags_Leaf);
		if (ImGui::IsItemClicked())
		{
			if(!show_inspector)
				show_inspector=true;
			Select(id);
		}
		ImGui::TreePop();
	}
	ImGui::EndGroup();
}

void Gui::Anims(const ResourceManager<avs::uid,clientrender::Animation>& animManager)
{
	ImGui::BeginGroup();
	const auto& ids= animManager.GetAllIDs();
	for (auto id : ids)
	{
		const auto &anim=animManager.Get(id);
		ImGui::TreeNodeEx(fmt::format("{0}: {1} ",id, anim->name.c_str()).c_str());
		if (ImGui::IsItemClicked())
		{
			if(!show_inspector)
				show_inspector=true;
			Select(id);
		}
		ImGui::TreePop();
	}
	
	ImGui::EndGroup();
}

void Gui::InputsPanel(avs::uid server_uid,client::SessionClient *sessionClient,teleport::client::OpenXR *openXR)
{
	if (ImGui::BeginTable("controls", 4))
	{
		ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Mapped", ImGuiTableColumnFlags_WidthStretch, 200.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		const auto &M=openXR->GetServerInputMappings(server_uid);
		const auto& I= openXR->GetServerInputs(server_uid,0);
		std::string val;
		for (const auto& m : M)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::LabelText("##clientActionId","%s", teleport::client::stringof(m.clientActionId));
			ImGui::TableNextColumn();
			ImGui::LabelText("##inputType", "%s", avs::stringof(m.serverInputDefinition.inputType));
			ImGui::TableNextColumn();
			ImGui::LabelText("##inputType", "%s", m.serverInputDefinition.regexPath.c_str());
			ImGui::TableNextColumn();
			switch (m.serverInputDefinition.inputType)
			{
			case avs::InputType::FloatState:
				val = fmt::format("{0}", I.analogueStates[m.serverInputDefinition.inputId]);
				break;
			case avs::InputType::IntegerState:
				val = fmt::format("{0}", I.binaryStates[m.serverInputDefinition.inputId]);
				break;
			case avs::InputType::FloatEvent:
				val = fmt::format("{0}", fmt::format("{0}", I.getLastAnalogueEvent(m.serverInputDefinition.inputId).strength));
				break;
			case avs::InputType::IntegerEvent:
				val = fmt::format("{0}", I.getLastBinaryEvent(m.serverInputDefinition.inputId).activated);
				break;
			default:
				break;
			};
			ImGui::LabelText("##inputVal", "%s", val.c_str());
		}
		ImGui::EndTable();
	}
	if (ImGui::BeginTable("poses", 2))
	{
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 200.0f);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 200.0f);
		const auto &m=sessionClient->GetNodePosePaths();
		for (auto p : m)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::LabelText("##frst", "%s", fmt::format("{0}", p.first).c_str());
			ImGui::TableNextColumn();
			ImGui::LabelText("##second", "%s", fmt::format("{0}", p.second).c_str());
		}
		ImGui::EndTable();
	}
}

void Gui::NetworkPanel(const teleport::client::ClientPipeline &clientPipeline)
{
	const avs::NetworkSourceCounters counters = clientPipeline.source->getCounterValues();
	const avs::DecoderStats vidStats = clientPipeline.decoder.GetStats();
	const auto& streams = clientPipeline.source->GetStreams();
	const auto & streamStatus=clientPipeline.source->GetStreamStatus();


	if (ImGui::BeginTable("bandwidth", 4))
	{
		ImGui::TableSetupColumn("Ch.", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 200.0f);
		ImGui::TableSetupColumn("In", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Out", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		for (size_t i = 0; i < streamStatus.size(); i++)
		{
			float kps_in = streamStatus[i].inwardBandwidthKps;
			float kps_out = streamStatus[i].outwardBandwidthKps;
			const auto& s = streams[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::LabelText("##chnl", "%zu",i);
			ImGui::TableNextColumn();
			ImGui::LabelText("##slabel", "%s",s.label.c_str());
			ImGui::TableNextColumn();
			ImGui::LabelText("##kps_in", "%3.1f",kps_in);
			ImGui::TableNextColumn();
			if(kps_out>0)
				ImGui::LabelText("##kps_out", "%3.1f",kps_out);
		}
		ImGui::EndTable();
	}

/*	LinePrint(platform::core::QuickFormat("Start timestamp: %d", clientPipeline.pipeline.GetStartTimestamp()));
	LinePrint(platform::core::QuickFormat("Current timestamp: %d", clientPipeline.pipeline.GetTimestamp()));
	LinePrint(platform::core::QuickFormat("Bandwidth KBs: %4.2f", counters.bandwidthKPS));
	LinePrint(platform::core::QuickFormat("Network packets received: %d", counters.networkPacketsReceived));
	LinePrint(platform::core::QuickFormat("Decoder packets received: %d", counters.decoderPacketsReceived));
	LinePrint(platform::core::QuickFormat("Network packets dropped: %d", counters.networkPacketsDropped));
	LinePrint(platform::core::QuickFormat("Decoder packets dropped: %d", counters.decoderPacketsDropped));
	LinePrint(platform::core::QuickFormat("Decoder packets incomplete: %d", counters.incompleteDecoderPacketsReceived));
	LinePrint(platform::core::QuickFormat("Decoder packets per sec: %4.2f", counters.decoderPacketsReceivedPerSec));
	LinePrint(platform::core::QuickFormat("Video frames received per sec: %4.2f", vidStats.framesReceivedPerSec));
	LinePrint(platform::core::QuickFormat("Video frames parseed per sec: %4.2f", vidStats.framesProcessedPerSec));
	LinePrint(platform::core::QuickFormat("Video frames displayed per sec: %4.2f", vidStats.framesDisplayedPerSec));*/
}

void Gui::DebugPanel(clientrender::DebugOptions &debugOptions)
{
	ImGui::Checkbox("Global Origin Axes",&debugOptions.showAxes);
	ImGui::Checkbox("Show Stage Space",&debugOptions.showStageSpace);
}

void Gui::CubemapOSD(crossplatform::Texture *videoTexture)
{
	LinePrint(platform::core::QuickFormat("Cubemap Texture"));

/*	static crossplatform::Texture* debugCubemapTexture = nullptr;
	if (!debugCubemapTexture)
		debugCubemapTexture = renderPlatform->CreateTexture("debugCubemapTexture");
	debugCubemapTexture->ensureTexture2DSizeAndFormat(renderPlatform, 512, 512, 1, crossplatform::RGBA_8_UNORM, false, true);

	debugCubemapTexture->activateRenderTarget(deviceContext);
	renderPlatform->Clear(deviceContext, vec4(0.0f, 0.0f, 0.0f, 1.0f)); //Add in the alpha.
	renderPlatform->DrawCubemap(deviceContext, videoTexture, 0.0f, 0.0f, 1.9f, 1.0f, 1.0f, 0.0f);
	debugCubemapTexture->deactivateRenderTarget(deviceContext);
	*/
	DrawTexture(videoTexture);
}

void Gui::TagOSD(std::vector<clientrender::SceneCaptureCubeTagData> &videoTagDataCubeArray
	,VideoTagDataCube videoTagDataCube[])
{
	auto geometryCache=clientrender::GeometryCache::GetGeometryCache(cache_uid);
	std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
	auto& cachedLights = geometryCache->mLightManager.GetCache(cacheLock);
	const char *name="";
	LinePrint("Tags\n");
	for(int i=0;i<videoTagDataCubeArray.size();i++)
	{
		auto &tag=videoTagDataCubeArray[i];
		LinePrint(platform::core::QuickFormat("%d lights",tag.coreData.lightCount));
		vec3 pos = tag.coreData.cameraTransform.position;
		LinePrint(fmt::format("\t{0},{1},{2}", pos.x, pos.y, pos.z));
		auto *gpu_tag_buffer=videoTagDataCube;
		if(gpu_tag_buffer)
		for(int j=0;j<tag.lights.size();j++)
		{
			auto &l=tag.lights[j];
			auto &t=gpu_tag_buffer[j];
			const LightTag &lightTag=t.lightTags[j];
			vec4 clr={l.color.x,l.color.y,l.color.z,1.0f};
			 
			auto C = cachedLights.find(l.uid);
			auto &c=C->second;
			if (c.resource)
			{
				auto& lcr =c.resource->GetLightCreateInfo();
				name=lcr.name.c_str();
			}
			if(l.lightType==clientrender::LightType::Directional)
				LinePrint(platform::core::QuickFormat("%llu: %s, Type: %s, dir: %3.3f %3.3f %3.3f clr: %3.3f %3.3f %3.3f",l.uid,name,ToString((clientrender::Light::Type)l.lightType)
					,lightTag.direction.x,lightTag.direction.y,lightTag.direction.z
					,l.color.x,l.color.y,l.color.z),clr);
			else
				LinePrint(platform::core::QuickFormat("%llu: %s, Type: %s, pos: %3.3f %3.3f %3.3f clr: %3.3f %3.3f %3.3f",l.uid, name, ToString((clientrender::Light::Type)l.lightType)
					,lightTag.position.x
					,lightTag.position.y
					,lightTag.position.z
					,l.color.x,l.color.y,l.color.z),clr);

		}
	}
}

void Gui::GeometryOSD()
{
	const std::vector<avs::uid> &cache_uids=clientrender::GeometryCache::GetCacheUids();
	static std::vector<std::string> cache_names;
	static std::vector<const char *> cache_strings;
	if(cache_uids.size()!=cache_names.size())
	{
		cache_names.clear();
		cache_strings.resize(cache_uids.size());
		for(size_t i=0;i<cache_uids.size();i++)
		{
			cache_names.push_back(fmt::format("{0}",cache_uids[i]));
		}
		for(size_t i=0;i<cache_uids.size();i++)
		{
			cache_strings[i]=cache_names[i].c_str();
		}
	}
	static int current_choice = 0;
	ImGui::Combo("Cache or Server", &current_choice, cache_strings.data(),(int)cache_strings.size());
	if(current_choice>=0&&current_choice<cache_uids.size())
		cache_uid=cache_uids[current_choice];
	vec4 white(1.f, 1.f, 1.f, 1.f);
	std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
	auto geometryCache=clientrender::GeometryCache::GetGeometryCache(cache_uid);
	if(!geometryCache)
		return;
	LinePrint(platform::core::QuickFormat("Nodes: %d",geometryCache->mNodeManager->GetNodeCount()), white);

	static int nodeLimit = 5;
	auto& rootNodes = geometryCache->mNodeManager->GetRootNodes();
	static int lineLimit = 50;

	LinePrint(platform::core::QuickFormat("Meshes: %d\nLights: %d", geometryCache->mMeshManager.GetCache(cacheLock).size(),
					geometryCache->mLightManager.GetCache(cacheLock).size()), white);
	LinePrint(platform::core::QuickFormat("Transparent Nodes: %d", geometryCache->mNodeManager->GetSortedTransparentNodes().size()), white);
					
	Scene();

	const auto &missing=geometryCache->GetMissingResources();
	if(missing.size())
	{
		LinePrint(fmt::format("Missing Resources: {0}",missing.size()).c_str());
		for(const auto& missingPair : missing)
		{
			const clientrender::MissingResource& missingResource = missingPair.second;
			std::string txt= fmt::format("\t{0} {1} from ", stringOf(missingResource.resourceType), missingResource.id);
			for(auto &u:missingResource.waitingResources)
			{
				auto type= u.get()->type;
				avs::uid id=u.get()->id;
				std::string name;
				if(type==avs::GeometryPayloadType::Node)
				{
					auto n = geometryCache->mNodeManager->GetNode(id);
					if(n)
						name = fmt::format(" ({0})",n->name);
				}
				txt+=fmt::format("{0}{1} {2}, ", stringOf(type),name,(uint64_t)id);
			}
			LinePrint( txt.c_str());
		}
	}
	const auto &req=geometryCache->GetResourceRequests();
	LinePrint(fmt::format("{0} Requests",req.size()).c_str());
	if(req.size())
	{
		std::string lst;
		for(const auto &r:req)
		{
			lst+=fmt::format("%d ",r);
		}
		LinePrint(lst.c_str());
	}
/*	if(sessionClient)
	{
		const auto &sent_req=sessionClient->GetSentResourceRequests();
		LinePrint(fmt::format("{0}: {1} Requests Sent",sessionClient->GetServerIP(),sent_req.size()).c_str());
		if(sent_req.size())
		{
			std::string lst;
			for(const auto &r:sent_req)
			{
				lst+=fmt::format("{0} ",r.first);
			}
			LinePrint(lst.c_str());
		}
	}*/
}

bool Gui::Tab(const char *txt)
{
	if(!in_tabs)
		ImGui::BeginTabBar("tabs");
	in_tabs=true;
	return ImGui::BeginTabItem(txt);
}

void Gui::EndTab()
{
	return ImGui::EndTabItem();
}

void Gui::Scene()
{
	auto geometryCache=clientrender::GeometryCache::GetGeometryCache(cache_uid);
	if(!geometryCache)
		return;
	ImGui::BeginTabBar("Scene");
	if(ImGui::BeginTabItem("Nodes"))
	{
		NodeTree( geometryCache->mNodeManager->GetRootNodes());
		ImGui::EndTabItem();
	}
	if(ImGui::BeginTabItem("Animations"))
	{
		Anims(geometryCache->mAnimationManager);
		ImGui::EndTabItem();
	}
	if(ImGui::BeginTabItem("Textures"))
	{
		Textures(geometryCache->mTextureManager);
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
}

void Gui::NodeTree(const std::vector<std::weak_ptr<clientrender::Node>>& root_nodes)
{
	static const size_t bufferSize = 40;
	static char buffer[bufferSize];
	ImGui::InputText("Search: ", buffer, bufferSize);
	const char* search_text = nullptr;
	if (strlen(buffer) > 0)
		search_text = buffer;
	for(auto &r:root_nodes)
	{
		TreeNode(r.lock(), search_text);
	}
}

void Gui::Update(const std::vector<vec4>& h,bool have_vr)
{
	hand_pos_press = h;
	have_vr_device = have_vr;
}

bool Gui::BeginMainMenuBar()
{
	ImGuiContext& g = *(ImGui::GetCurrentContext());

	// For the main menu bar, which cannot be moved, we honor g.Style.DisplaySafeAreaPadding to ensure text can be visible on a TV set.
	// FIXME: This could be generalized as an opt-in way to clamp window->DC.CursorStartPos to avoid SafeArea?
	// FIXME: Consider removing support for safe area down the line... it's messy. Nowadays consoles have support for TV calibration in OS settings.
	//g.NextWindowData.MenuBarOffsetMinVal = ImVec2(g.Style.DisplaySafeAreaPadding.x, 4.f);
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
	float height = ImGui::GetFrameHeight();
	ImVec2 window_pos= ImVec2(0, 0), window_pos_pivot= ImVec2(0, 0);
	float w = ImGui::GetMainViewport()->Size.x;
	ImGui::SetNextWindowSize(ImVec2(w,48.f));
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always, window_pos_pivot);
	//g.NextWindowData.MenuBarOffsetMinVal = ImVec2(0.0f, 0.0f);
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	bool res=ImGui::Begin("menu",0,window_flags);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
	return res;
}

void Gui::EndMainMenuBar()
{
	ImGui::PopStyleVar(1);
	ImGui::End();
	ImGui::PopStyleVar(1);
}

void Gui::ShowSettings2D()
{
	auto& config = client::Config::GetInstance();
	ImGui::SetNextWindowPos(ImVec2(40, 60));						  // always at the window origin
	float w = ImGui::GetMainViewport()->Size.x-80.0f;
	ImGui::SetNextWindowSize(ImVec2(w, ImGui::GetWindowHeight()-80));
	ImGui::SetNextItemWidth(w);
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
	ImGui::SameLine(ImGui::GetWindowWidth() - 80);
	ImGui::Begin("Settings", 0, window_flags);
	ImGui::PushFont(defaultFont);
	ImGui::LabelText("##Settings","Settings");
	ImGui::PopFont();
	if (ImGui::BeginTable("options", 2))
	{
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 300.0f);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 400.0f);
		MainOptions();
#if TELEPORT_INTERNAL_CHECKS
		if (config.dev_mode)
			DevModeOptions();
#endif
		ImGui::EndTable();
	}
}

void Gui::ListBookmarks()
{
	auto& config = client::Config::GetInstance();
	const std::vector<client::Bookmark>& bookmarks = config.GetBookmarks();
	auto BookmarkEntry = [this](const std::string& url, const std::string& title)
	{
		if (ImGui::TreeNodeEx(url.c_str(), ImGuiTreeNodeFlags_Leaf, "%s", title.c_str()))
		{
			if (ImGui::IsItemClicked())
			{
				selected_url = url;
			}
			else if (!ImGui::IsMouseDown(0) && selected_url == url)
			{
				current_url = url;
				// copy, including null terminator.
				memcpy(url_buffer, current_url.c_str(), std::min((size_t)499, current_url.size()+1));
				connectHandler(current_tab_context,url);
				show_bookmarks = false;
			}
			ImGui::TreePop();
		}
	};
	for (int i = 0; i < bookmarks.size(); i++)
	{
		const client::Bookmark& b = bookmarks[i];
		BookmarkEntry(b.url, b.title);
	}
	ImGui::Separator();
	const std::vector<std::string>& recent = config.GetRecent();
	for (int i = 0; i < recent.size(); i++)
	{
		const std::string& r = recent[i];
		BookmarkEntry(r, r);
	}
}

void Gui::MenuBar2D()
{
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.00f, 0.00f, 0.00f, 0.0f));
	if (ImGui::Button(ICON_FK_RENREN, ImVec2(36, 24)))
	{
		cancelConnectHandler(current_tab_context);
	}
	if (ImGui::IsItemActive() || ImGui::IsItemHovered())
		TIMED_TOOLTIP("Return to lobby");
	auto& config = client::Config::GetInstance();
	{
		if(!current_tab_context)
		{
			current_tab_context = client::TabContext::AddTabContext();
		}
		auto tabContext = client::TabContext::GetTabContext(current_tab_context);
		avs::uid server_uid = tabContext->GetServerUid();
		auto sessionClient = client::SessionClient::GetSessionClient(server_uid);
		bool connecting = sessionClient?sessionClient->IsConnecting():false;
		bool connected = sessionClient?sessionClient->IsConnected() : false;
		bool connect_please=false;
		bool cancel_please=false;
		ImGui::SameLine();
		ImGui::PushItemWidth(ImGui::GetWindowWidth() - 5*40-8);
		if (ImGui::InputText("##URL", url_buffer, IM_ARRAYSIZE(url_buffer),ImGuiInputTextFlags_EnterReturnsTrue))
		{
			current_url = url_buffer;
			connect_please=true;
			if(connecting)
				cancel_please=true;
		}
		url_input = ImGui::IsItemActive();
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (!connecting)
		{
			if (ImGui::Button(ICON_FK_LONG_ARROW_RIGHT, ImVec2(36, 24)))
			{
				connect_please=true;
			}
			if (ImGui::IsItemActive() || ImGui::IsItemHovered())
				TIMED_TOOLTIP("Connect");
		}
		else
		{
			if (ImGui::Button(ICON_FK_TIMES, ImVec2(36, 24)))
			{
				cancel_please=true;
			}
			if (ImGui::IsItemActive() || ImGui::IsItemHovered())
				TIMED_TOOLTIP("Cancel connection");
		}
		if(cancel_please)
		{
			cancelConnectHandler(current_tab_context);
		}
		if(connect_please)
		{
			show_bookmarks = false;
			show_options = false;
			current_url = url_buffer;
			connectHandler(current_tab_context,current_url);
		}

		ImGui::SameLine();
		if (ImGui::Button(ICON_FK_FOLDER_O, ImVec2(36, 24)))
		{
			show_bookmarks = !show_bookmarks;
			selected_url = "";
			if (show_bookmarks)
			{
				show_options = false;
				ImVec2 pos=ImGui::GetCursorScreenPos();
				bookmarks_pos = { ImGui::GetWindowWidth(),pos.y };
			}
		}
		if (ImGui::IsItemActive() || ImGui::IsItemHovered())
			TIMED_TOOLTIP("Bookmarks");
		ImGui::SameLine();
		if (ImGui::Button(ICON_FK_COG, ImVec2(36, 24)))
		{
			show_options = !show_options;
			if (!show_options)
			{
				show_bookmarks = false;
				config.SaveOptions();
			}
		}
		if (ImGui::IsItemActive() || ImGui::IsItemHovered())
			TIMED_TOOLTIP("Settings");
	}
	ImGui::PopStyleColor();
}

void Gui::Render2DGUI(GraphicsDeviceContext& deviceContext)
{
	LightStyle();
	auto vp = renderPlatform->GetViewport(deviceContext, 0);
	ImGui_ImplPlatform_NewFrame(false, vp.w, vp.h);
#ifdef _MSC_VER
	ImGui_ImplWin32_NewFrame();
#endif
#ifdef __ANDROID__
	ImGui_ImplAndroid_NewFrame();
#endif
	ImGui::NewFrame();
	ImGuiIO& io = ImGui::GetIO();
#ifdef __ANDROID__
	// The mouse pos is the position where the controller's pointing direction intersects the OpenXR overlay surface.
	ImGui_ImplPlatform_SetMousePos((int)((0.5f + mouse.x) * float(vp.w)), (int)((0.5f - mouse.y) * float(vp.h)), vp.w, vp.h);
	ImGui_ImplPlatform_SetMouseDown(0, mouseButtons[0]);
#endif
	ImGui::PushFont(fontInter[18]);
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
	
	//ImGuiBegin("Teleport VR", nullptr, window_flags);
	static bool show_demo_window = false;
	if(show_demo_window)
		ImGui::ShowDemoWindow(&show_demo_window);
	BeginMainMenuBar();
	{
		MenuBar2D();
	}
	EndMainMenuBar();
	auto& style = ImGui::GetStyle();
	auto& config = client::Config::GetInstance();
	if(config.dev_mode)
	{
		static float dev_overlay_width=0.f;
		ImGui::PushFont(fontInter[12]);
		ImVec2 pos = {vp.w - dev_overlay_width-12.f, 100.f};
		ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
		ImGui::SetNextWindowPos(pos);
		ImGuiBegin("dev_overlay",0,window_flags);
		{
			dev_overlay_width = ImGui::GetWindowWidth();

			ImGui::BeginTabBar("tabcontexts");
			const auto &tabs = client::TabContext::GetTabIndices();
			for(const auto t:tabs)
			{
				const auto tabContext = client::TabContext::GetTabContext(t);
				if(!tabContext)
					continue;
				std::string str=fmt::format("{0}",t);
				if(ImGui::BeginTabItem(str.c_str()))
				{
					client::ConnectionStatus status=client::ConnectionStatus::UNCONNECTED;
					avs::StreamingConnectionState streamingStatus = avs::StreamingConnectionState::UNINITIALIZED;
					static std::vector<avs::uid > ids;
					ids.resize(0);
					ids.push_back(tabContext->GetServerUid());
					ids.push_back(tabContext->GetNextServerUid());

					vec4 white(1.f, 1.f, 1.f, 1.f);
					ImGui::BeginTable("serverstats", 2);
					ImGui::TableSetupColumn("name2", ImGuiTableColumnFlags_WidthFixed, 120.0f);
					ImGui::TableSetupColumn("val21", ImGuiTableColumnFlags_WidthStretch);

					for(auto id:ids)
					{
						if(!id)
							continue;
						auto sessionClient = client::SessionClient::GetSessionClient(id);
						status = sessionClient->GetConnectionStatus();
						streamingStatus = sessionClient->GetStreamingConnectionState();

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::Text("Server Id");
						ImGui::TableNextColumn();
						ImGui::Text("%llu", id);

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::Text("Session Id");
						ImGui::TableNextColumn();
						ImGui::Text("%lu", sessionClient->GetSetupCommand().session_id);

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::Text("Session Status");
						ImGui::TableNextColumn();
						ImGui::Text("%s", teleport::client::StringOf(status));

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::Text("Streaming Status");
						ImGui::TableNextColumn();
						ImGui::Text("%s", avs::stringOf(streamingStatus));

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::Text("Latency");
						ImGui::TableNextColumn();
						ImGui::Text("%4.4f ms", sessionClient->GetLatencyMs());

						ImGui::TableNextRow();
					}
					ImGui::EndTable();

					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}

		ImGuiEnd();
		
		/*
		if (ImGuiBegin("Keyboard", nullptr, window_flags))
		{
			ImGui::Text(
				"O: Toggle OSD\n"
					"K: Connect/Disconnect\n"
					"N: Toggle Node Overlays\n"
					"R: Recompile shaders\n"
					"NUM 0: PBR\n"
					"NUM 1: Albedo\n"
					"NUM 4: Unswizzled Normals\n"
					"NUM 5: Debug animation\n"
					"NUM 6: Lightmaps\n"
					"NUM 2: Vertex Normals\n");
			ImGuiEnd();
		}*/
		ImGui::PopFont();
	}
	if(config.enable_vr)
	{
		ImVec4 transp= ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_Button,style.Colors[ImGuiCol_WindowBg]);
		ImGui::PushStyleColor(ImGuiCol_WindowBg,transp);
		ImGui::PushStyleColor(ImGuiCol_Border,transp);
		
		ImVec2 pos = { vp.w - 100.f ,vp.h-100.f };
		ImGui::SetNextWindowPos(pos);

		ImGui::Begin("btn",0,window_flags);
			//ImGui::SameLine();
		ImGui::BeginDisabled(!openXR.CanStartSession());
		const char *systname=openXR.GetSystemName();
		if(ImGui::ImageButton("##start_vr", &imgui_vrHeadsetIconTexture, ImVec2(40, 40),ImVec2(0,0),ImVec2(1,1),ImVec4(0,0,0,0),ImVec4(0,0,0,1.0f)))
		{
			startXRSessionHandler();
		}
		if (ImGui::IsItemActive() || ImGui::IsItemHovered())
		{
			if(systname)
			{
				TIMED_TOOLTIP("%s", ("Activate VR: "s + systname).c_str());
			}
			else
				TIMED_TOOLTIP("Activate VR");
		}
		else
		{
			TIMED_TOOLTIP("No OpenXR device found");
		}
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		ImGui::EndDisabled();
		ImGui::End();
	}
	//ImGui::End();
	if (show_options)
	{
		ShowSettings2D();
	}
	else if(show_bookmarks)
	{
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysVerticalScrollbar
										|ImGuiWindowFlags_NoDecoration
										|ImGuiWindowFlags_AlwaysAutoResize
										|ImGuiWindowFlags_NoSavedSettings
										|ImGuiWindowFlags_NoFocusOnAppearing
										|ImGuiWindowFlags_NoNav;

		static float bookmarks_width = 0.0f;
		ImVec2 pos = { bookmarks_pos.x - bookmarks_width ,bookmarks_pos.y };
		ImGui::SetNextWindowPos(pos);

		ImGui::Begin("Bookmarks", 0, window_flags);
		bookmarks_width = ImGui::GetWindowWidth();
		ListBookmarks();
		ImGui::End();
	}
	//ImGuiEnd();
	ImGui::PopFont();
	ImVec2 mouse_pos	= io.MousePos;
	//ImGui::GetForegroundDrawList()->AddCircleFilled(mouse_pos, 2.f, IM_COL32(90, 255, 90, 200), 16);
	ImGui::Render();
	ImGui_ImplPlatform_RenderDrawData(deviceContext, ImGui::GetDrawData());
}

void Gui::MainOptions()
{
	auto& config = client::Config::GetInstance();
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::LabelText("##Passthrough", "PassThrough");
		ImGui::TableNextColumn();
		bool e = config.options.passThrough;
		ImGui::Checkbox("##passThrough", &e);
		if(e!= config.options.passThrough)
		{
				config.options.passThrough =e;
		}
	}
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::LabelText("##LobbyView", "Lobby View");
	ImGui::TableNextColumn();
	int e = (int)config.options.lobbyView;
	ImGui::RadioButton("White", &e, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Neon", &e, 1);
	if ((client::LobbyView)e != config.options.lobbyView)
	{
		config.options.lobbyView = (client::LobbyView)e;
	}
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::PushItemWidth(300.0f);
	ImGui::LabelText("##StartupConnection", "Startup Connection");
	ImGui::TableNextColumn();
	auto conn = magic_enum::enum_entries<teleport::client::StartupConnectOption>();
	int c = (int)config.options.startupConnectOption;
	int i = 0;
	for (const auto& e : conn)
	{
		std::string ename = fmt::format("{0}", e.second);
		ImGui::RadioButton(ename.c_str(), &c, i++);
		ImGui::SameLine();
	}
	if ((client::StartupConnectOption)c != config.options.startupConnectOption)
	{
		config.options.startupConnectOption = (client::StartupConnectOption)c;
	}
}

void Gui::DevModeOptions()
{
	auto& config = client::Config::GetInstance();
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::LabelText("##labelGeometryOffline", "Geometry Visible Offline");
	ImGui::TableNextColumn();
	ImGui::Checkbox("##showGeometryOffline", &config.options.showGeometryOffline);
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::LabelText("##labelSimulateVR", "Simulate VR device");
	ImGui::TableNextColumn();
	ImGui::Checkbox("##cbSimulateVR", &config.options.simulateVR);
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::LabelText("##labelkillstr", "Test: kill streaming");
	ImGui::TableNextColumn();
	if(ImGui::Button("##KillStreaming"))
	{
		if(console)
			console("killstreaming");
	}
}
void Gui::Render3DGUI(GraphicsDeviceContext& deviceContext )
{
	view_pos = deviceContext.viewStruct.cam_pos;
	view_dir = deviceContext.viewStruct.view_dir;
	auto& config = client::Config::GetInstance();
	if(!visible)
		return;
	vec3 pos_diff=view_pos-menu_pos;
	if(length(pos_diff)>1.4f)
	{
		reset_menu_pos=true;
	}
	if(reset_menu_pos)
	{
		menu_pos		= view_pos;
		static float z_offset = -.3f;
		static float distance = 0.4f;
		azimuth			= atan2f(-view_dir.x, view_dir.y);
		tilt			= 3.1415926536f / 4.0f;
		vec3 menu_offset = { distance * -sin(azimuth),distance * cos(azimuth),z_offset };
		menu_pos		+= menu_offset;
		reset_menu_pos=false;
	}
	DarkStyle();
	// Start the Dear ImGui frame
#ifdef _MSC_VER
	ImGui_ImplWin32_NewFrame();
#endif
#ifdef __ANDROID__
	ImGui_ImplAndroid_NewFrame();
#endif
	ImGuiIO& io = ImGui::GetIO();
	static bool in3d=true;
	static float window_width=720.0f;
	static float window_height=260.0f;
	ImVec2 size_min(window_width,window_height);
	ImVec2 size_max(window_width,window_height);
	ImGui_ImplPlatform_NewFrame(in3d,(int)size_max.x,(int)size_max.y,menu_pos,azimuth,tilt,width_m);
	static int refocus=0;
	bool show_hide=true;
	if (have_vr_device)
		ImGui_ImplPlatform_Update3DTouchPos(hand_pos_press);
	else
		ImGui_ImplPlatform_Update3DMousePos();
	ImVec2 mousePos=io.MousePos;
	ImGui::NewFrame();
	// restore mouse pos here to override ImGui's internal shenanigans.
	io.MousePos=mousePos;
	{
		bool show_keyboard = true;
		ImGuiWindowFlags windowFlags =0;
		if(in3d)
		{
			ImGui::SetNextWindowPos(ImVec2(0, 0));						  // always at the window origin
			ImGui::SetNextWindowSizeConstraints(size_min, size_max);
			if(!show_keyboard)
				ImGui::SetNextWindowSize(size_min);
			else
				ImGui::SetNextWindowSize(size_max);
			windowFlags=ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoCollapse|
				ImGuiWindowFlags_NoScrollbar;
		}
		ImGui::LogToTTY();

		const auto& [dateStr, timeStr] = GetCurrentDateTimeStrings();
		std::string title = "Teleport VR";// + dateStr + " " + timeStr;
		ImGuiBegin(title.c_str(),&show_hide, windowFlags);
		#if 0
		std::vector<vec3> client_press;
		ImGui_ImplPlatform_Get3DTouchClientPos(client_press);
		for(int i=0;i<hand_pos_press.size();i++)
		{
			std::string lbl=fmt::format("pos {0}",i);
			vec4 v=hand_pos_press[i];
			std::string txt=fmt::format("{0: .3f},{1: .3f},{2: .3f},{3: .3f}",v.x,v.y,v.z,v.w);
			ImGui::LabelText(lbl.c_str(),txt.c_str());
		}
		for(int i=0;i<client_press.size();i++)
		{
			std::string lbl=fmt::format("press {0}",i);
			vec3 v=client_press[i];
			std::string txt=fmt::format("{0: .3f},{1: .3f},{2: .3f}",v.x,v.y,v.z);
			ImGui::LabelText(lbl.c_str(),txt.c_str());
		}
		#endif
		if(KeysDown[VK_BACK])
		{
			KeysDown[VK_BACK]= false;
			io.AddKeyEvent(ImGuiKey_Backspace, false);
		}
		if(refocus>=2)
		{
			while(keys_pressed.size())
			{
				int k=keys_pressed[0];
				if(k==VK_BACK)
				{
					KeysDown[k] = true;
					io.AddKeyEvent(ImGuiKey_Backspace, true);
				}
				else
				{
					io.AddInputCharacter(k);
				}
				keys_pressed.erase(keys_pressed.begin());
			}
		}
		if(show_options)
		{
			ImGui::Spacing();
			ImGui::SameLine(ImGui::GetWindowWidth()-80);

			if(ImGui::Button(ICON_FK_ARROW_LEFT,ImVec2(64,32)))
			{
				config.SaveOptions();
				show_options=false;
			}
			if (ImGui::BeginTable("options", 2))
			{
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 300.0f);
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 400.0f);
				MainOptions();

				#if TELEPORT_INTERNAL_CHECKS
				if (config.dev_mode)
					DevModeOptions();
				#endif
				ImGui::EndTable();
			}
		}
		else
		{
			if (ImGui::Button(ICON_FK_RENREN, ImVec2(64, 32)))
			{
				cancelConnectHandler(current_tab_context);
			}
			ImGui::SameLine();
			if (ImGui::Button(ICON_FK_FOLDER_O, ImVec2(64, 32)))
			{
				show_bookmarks=!show_bookmarks;
				selected_url="";
			}
			if(show_bookmarks)
			{
				const std::vector<client::Bookmark> &bookmarks=config.GetBookmarks();
				ImGui::SameLine();
				ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysVerticalScrollbar;
				ImGui::BeginChild("Bookmarks", ImVec2(-1,-1), true, window_flags);
				ListBookmarks();
				
				ImGui::EndChild();
			}
			else
			{
				const std::set<int32_t> &tabIndices=client::TabContext::GetTabIndices();
				int32_t tab_index=0;
				if(tabIndices.size())
					tab_index=tabIndices.begin().operator*();
				else
					tab_index = client::TabContext::AddTabContext();
				std::shared_ptr<client::TabContext> tabContext = client::TabContext::GetTabContext(tab_index);
				avs::uid server_uid=tabContext->GetServerUid();
				auto sessionClient = client::SessionClient::GetSessionClient(server_uid);
				bool connecting = false;
				bool connected = false;
				if(sessionClient)
				{
				 connecting=sessionClient->IsConnecting();
					 connected=sessionClient->IsConnected();
				}
				ImGui::SameLine();
				if (refocus == 0)
				{
					ImGui::SetKeyboardFocusHere();
				}
				ImGui::PushItemWidth(ImGui::GetWindowWidth() - 4 * 80);
				if (ImGui::InputText("##URL", url_buffer, IM_ARRAYSIZE(url_buffer)))
				{
					current_url = url_buffer;
				}
				ImGui::PopItemWidth();
				refocus++;

				ImGui::SameLine();
				if (!connecting)
				{
					if (ImGui::Button(ICON_FK_LONG_ARROW_RIGHT, ImVec2(64, 32)))
					{
						current_url = url_buffer;
						connectHandler(current_tab_context,current_url);
					}
				}
				else
				{
					if (ImGui::Button(ICON_FK_TIMES, ImVec2(64, 32)))
					{
						cancelConnectHandler(current_tab_context);
					}
				}

				ImGui::SameLine();
				if (ImGui::Button(ICON_FK_COG, ImVec2(64, 32)))
				{
					show_options=!show_options;
				}

				if (show_keyboard)
				{
					auto KeyboardLine = [&io,this,connecting](const char* key)
					{
						size_t num = strlen(key);
						for (size_t i = 0; i < num; i++)
						{
							char key_label[] = "X";
							key_label[0] = *key;
							ImGui::Button(key_label,ImVec2(46,32));
							if(ImGui::IsItemClicked())
							{
								refocus=0;
								keys_pressed.push_back(*key);
								if(connecting)
								{
									cancelConnectHandler(current_tab_context);
								}
							}
							key++;
							if (i<num-1)
								ImGui::SameLine();
						}
					};
					KeyboardLine("1234567890-");
					ImGui::SameLine();
					//ImGui::Button(ICON_FK_SEARCH " Search");
					if (ImGui::Button(ICON_FK_LONG_ARROW_LEFT,ImVec2(92,32)))
					{
						 refocus=0;
						 keys_pressed.push_back(VK_BACK);
					}
					ImGui::Text("  ");
					ImGui::SameLine();
					KeyboardLine("qwertyuiop");
					//ImGui::SameLine();
					//ImGui::Text(fmt::format("{0: .0f} {1: .0f}",io.MousePos.x,io.MousePos.y).c_str());
					//Sleep(1000);
					ImGui::Text("	");
					ImGui::SameLine();
					KeyboardLine("asdfghjkl:");
					/*ImGui::SameLine();
					static char buf[32] = "Return";
					if (ImGui::Button(buf,ImVec2(92,32)))
					{
						 refocus=0;
						 keys_pressed.push_back(ImGuiKey_Enter);
					}*/
					ImGui::Text("	  ");
					ImGui::SameLine();
					KeyboardLine("zxcvbnm,./");
				}
			}
		}
		ImGuiEnd();
	}
	
	ImGui::GetForegroundDrawList()->AddCircleFilled(mousePos, 2.f, IM_COL32(90, 255, 90, 200), 16);
	static float handleWidth=12.f;
	ImVec2 handle1_min={0.f,120.f};
	ImVec2 handle1_max={handleWidth,480.f};
	ImVec2 handle2_min={window_width-handleWidth,120.f};
	ImVec2 handle2_max={window_width,480.f};
	auto& style = ImGui::GetStyle();
	ImU32 handle1Colour=ImGui::GetColorU32(style.Colors[ImGuiCol_Button]);
	ImU32 handle2Colour=ImGui::GetColorU32(style.Colors[ImGuiCol_Button]);
	ImGui::GetForegroundDrawList()->AddRectFilled(handle1_min,handle1_max,handle1Colour,0.5f);
	ImGui::GetForegroundDrawList()->AddRectFilled(handle2_min,handle2_max,handle2Colour,0.5f);
	ImGui::Render();
	ImGui_ImplPlatform_RenderDrawData(deviceContext, ImGui::GetDrawData());
	if(!show_hide)
		Hide();
}

void Gui::SetConnectHandler(std::function<void(int32_t,const std::string&)> fn)
{
	connectHandler = fn;
}

void Gui::SetCancelConnectHandler(std::function<void(int32_t)> fn)
{
	cancelConnectHandler = fn;
}

void Gui::SetServerIPs(const std::vector<std::string> &s)
{
	server_ips=s;
	if(server_ips.size())
	{
		memcpy(url_buffer,server_ips[0].c_str(),std::min(500,(int)server_ips[0].size()+1));//,server_ips[0].size());
		current_url = url_buffer;
	}

}


void Gui::Select(avs::uid u)
{
	if(selection_cursor+1<selection_history.size())
	{
		selection_history.erase(selection_history.begin()+selection_cursor+1,selection_history.end());
	}
	else if(selection_cursor==selection_history.size()-1&&u==selection_history.back())
		return;
	selection_history.push_back(u);
	selection_cursor=selection_history.size()-1;
}

void Gui::SelectPrevious()
{
	if(selection_cursor>0)
		selection_cursor--;
}

void Gui::SelectNext()
{
	if(selection_cursor+1<selection_history.size())
		selection_cursor++;
}

avs::uid Gui::GetSelectedUid() const
{
	if(selection_cursor<selection_history.size())
		return selection_history[selection_cursor];
	else
		return 0;
}

avs::uid Gui::GetSelectedServer() const
{
	return selected_server;
}