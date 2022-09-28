
#include <imgui.h>
#ifdef _MSC_VER
#include "backends/imgui_impl_win32.h"
#endif
#ifdef __ANDROID__
#include "backends/imgui_impl_android.h"
#endif
#include "Platform/ImGui/imgui_impl_platform.h"
#include "Platform/Core/FileLoader.h"
#include "Platform/Core/StringToWString.h"
#include "Gui.h"
#include "IconsForkAwesome.h"
#include "TeleportCore/ErrorHandling.h"
#include <fmt/format.h>

#ifdef _MSC_VER
#include <direct.h>
#include <Windows.h>
#endif

#ifdef __ANDROID__
#define VK_BACK           0x01
#define VK_ESCAPE         0x02
#endif
#define VK_MAX         0x10
bool KeysDown[VK_MAX];
using namespace teleport;
using namespace platform;
using namespace platform;
using namespace crossplatform;
ImFont *smallFont=nullptr;
ImFont *symbolFont=nullptr;
#define STR_VECTOR3 "%3.3f %3.3f %3.3f"
#define STR_VECTOR4 "%3.3f %3.3f %3.3f %3.3f"
PlatformWindow *platformWindow=nullptr;
void Gui::SetPlatformWindow(PlatformWindow *w)
{
#ifndef _MSC_VER
	if(renderPlatform!=nullptr&&w!=platformWindow)
		ImGui_ImplAndroid_Init(w);
#endif
	platformWindow=w;
}

static inline ImVec4 ImLerp(const ImVec4& a, const ImVec4& b, float t)
{
	return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

void Gui::RestoreDeviceObjects(RenderPlatform* r,PlatformWindow *w)
{
	renderPlatform=r;
	if(!r)
		return;
	for(uint16_t i=0;i<VK_MAX;i++)
	{
		KeysDown[i]=false;
	}
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	
	auto& style = ImGui::GetStyle();

	style.GrabRounding = 1.f;
	style.WindowRounding = 12.f;
	style.ScrollbarRounding = 3.f;
	style.FrameRounding = 12.f;
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
	style.FramePadding = ImVec2(8.f,2.f);
	style.FramePadding.x=4.f;
	style.FrameBorderSize=2.f;
    ImVec4* colors = style.Colors;
	ImVec4 imWhite(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_Text]                   = imWhite;
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.43f, 0.43f, 0.43f, 0.50f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.29f, 0.29f, 0.29f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.45f, 0.45f, 0.45f, 0.40f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.65f, 0.65f, 0.65f, 0.67f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.30f, 0.30f, 0.30f, 0.40f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator]              = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImLerp(colors[ImGuiCol_Header],       colors[ImGuiCol_TitleBgActive], 0.80f);
    colors[ImGuiCol_TabHovered]             = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_TabActive]              = ImLerp(colors[ImGuiCol_HeaderActive], colors[ImGuiCol_TitleBgActive], 0.60f);
    colors[ImGuiCol_TabUnfocused]           = ImLerp(colors[ImGuiCol_Tab],          colors[ImGuiCol_TitleBg], 0.80f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImLerp(colors[ImGuiCol_TabActive],    colors[ImGuiCol_TitleBg], 0.40f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);   // Prefer using Alpha=1.0 here
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);   // Prefer using Alpha=1.0 here
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.65f, 0.65f, 0.65f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	SetPlatformWindow(w);
#ifdef _MSC_VER
	ImGui_ImplWin32_Init(GetActiveWindow());
#else
	ImGui_ImplAndroid_Init(platformWindow);
#endif
	ImGui_ImplPlatform_Init(r);

	// NB: Transfer ownership of 'ttf_data' to ImFontAtlas, unless font_cfg_template->FontDataOwnedByAtlas == false. Owned TTF buffer will be deleted after Build().

	platform::core::FileLoader *fileLoader=core::FileLoader::GetFileLoader();
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
	AddFont("Exo-SemiBold.ttf");
	// NOTE: imgui expects the ranges pointer to be VERY persistent. Not clear how persistent exactly, but it is used out of the scope of
	// this function. So we have to have a persistent variable for it!
	static ImVector<ImWchar> glyph_ranges;
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
		builder.AddText(ICON_FK_ARROW_LEFT);									
		builder.BuildRanges(&glyph_ranges);							// Build the final result (ordered ranges with all the unique characters submitted)
		symbolFont=AddFont("forkawesome-webfont.ttf",32.f,&config,glyph_ranges.Data);
		io.Fonts->Build();										// Build the atlas while 'ranges' is still in scope and not deleted.
	}
	{
		smallFont=AddFont("Inter-SemiBold.otf",16.f);
	}
	io.ConfigFlags|=ImGuiConfigFlags_IsTouchScreen;// VR more like a touch screen.
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
		renderPlatform=nullptr;
	}
}

void Gui::RecompileShaders()
{
	ImGui_ImplPlatform_RecompileShaders();
}

void Gui::ShowHide()
{
	if(visible)
		Hide();
	else
		Show();
}

void Gui::Show()
{
	visible			= true;
	menu_pos		= view_pos;
	static float z_offset = -.3f;
	static float distance = 0.4f;
	azimuth			= atan2f(-view_dir.x, view_dir.y);
	tilt			= 3.1415926536f / 4.0f;
	vec3 menu_offset = { distance * -sin(azimuth),distance * cos(azimuth),z_offset };
	menu_pos		+= menu_offset;
	keys_pressed.clear();
}

void Gui::Hide()
{
	config->SaveOptions();
	visible = false;
}

void Gui::SetScaleMetres()
{
	visible = false;
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


void Gui::TreeNode(const std::shared_ptr<clientrender::Node>& n,const char *search_text)
{
	const clientrender::Node *node=n.get();
	bool has_children=node->GetChildren().size()!=0;
	std::string str =(std::to_string(n->id)+" ")+ node->name;
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

static int in_debug_gui = 0;
void Gui::BeginDebugGui(GraphicsDeviceContext& deviceContext)
{
	if (in_debug_gui != 0)
	{
		return;
	}
	// POSSIBLY can't use ImGui's own Win32 NewFrame as we may want to override the mousepos.
#ifdef _MSC_VER
	ImGui_ImplWin32_NewFrame();
#endif
#ifdef __ANDROID__
	ImGui_ImplAndroid_NewFrame();
#endif
	auto vp = renderPlatform->GetViewport(deviceContext, 0);
	ImGui_ImplPlatform_NewFrame(false, vp.w, vp.h);
	ImGui::NewFrame();
	ImGuiIO& io = ImGui::GetIO();
	ImGui::PushFont(smallFont);
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
	if (ImGui::Begin("Teleport VR", nullptr, window_flags))
		in_debug_gui++;

		
	//	ShowFont();
}

void Gui::LinePrint(const char* txt,const float *clr)
{
	if (in_debug_gui != 1)
	{
		return;
	}
	if (clr == nullptr)
		ImGui::Text("%s",txt);
	else
		ImGui::TextColored(*(reinterpret_cast<const ImVec4*>(clr)), "%s",txt);
}

void Gui::DrawTexture(Texture* texture)
{
	if (!texture)
		return;
	if (!texture->IsValid())
		return;

	const int width = texture->width;
	const int height = texture->length;
	const float aspect = static_cast<float>(width) / static_cast<float>(height);
	const ImVec2 regionSize = ImGui::GetContentRegionAvail();
	const ImVec2 textureSize = ImVec2(static_cast<float>(width), static_cast<float>(height));
	const ImVec2 size = ImVec2(std::min(regionSize.x, textureSize.x), std::min(regionSize.x, textureSize.x) * aspect);
	ImTextureID imTextureID = (ImTextureID)texture;

	ImGui::Image(imTextureID, size);
}

static void DoRow(const char* title, const char* text, ...)
{
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

void Gui::EndDebugGui(GraphicsDeviceContext& deviceContext)
{
	if (in_debug_gui != 1)
	{
		return;
	}
	ImGui::End();
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
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
		window_flags |= ImGuiWindowFlags_NoMove;
	}
	ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
	if (ImGui::Begin("Keyboard", nullptr, window_flags))
	{
		ImGui::Text("K: Connect/Disconnect\n"
			"O: Toggle OSD\n"
			"V: Show video\n"
			"C: Toggle render from centre\n"
			"T: Toggle Textures\n"
			"N: Toggle Node Overlays\n"
			"M: Change rendermode\n"
			"R: Recompile shaders\n"
			"NUM 0: PBR\n"
			"NUM 1: Albedo\n"
			"NUM 4: Unswizzled Normals\n"
			"NUM 5: Debug animation\n"
			"NUM 6: Lightmaps\n"
			"NUM 2: Vertex Normals\n");
		ImGui::End();
	}
	if(geometryCache&&show_inspector)
	{
		ImGuiWindowFlags window_flags =ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_MenuBar|ImGuiWindowFlags_AlwaysAutoResize;//  | 
			//| ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;|ImGuiWindowFlags_NoDecoration
		if (ImGui::Begin("Properties", &show_inspector, window_flags))
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
			std::shared_ptr<const clientrender::Node> selected_node=geometryCache->mNodeManager->GetNode(selected_uid);
			std::shared_ptr<const clientrender::Material> selected_material=geometryCache->mMaterialManager.Get(selected_uid);
			std::shared_ptr<const clientrender::Texture> selected_texture=geometryCache->mTextureManager.Get(selected_uid);
			std::shared_ptr<const clientrender::Animation> selected_animation=geometryCache->mAnimationManager.Get(selected_uid);
			if (selected_node.get())
			{
				avs::vec3 pos = selected_node->GetLocalPosition();
				avs::vec3 gpos = selected_node->GetGlobalPosition();
				avs::vec3 sc = selected_node->GetLocalScale();
				vec4 q = selected_node->GetLocalRotation();
				vec4 gq = selected_node->GetGlobalRotation();
				avs::vec3 gs = selected_node->GetGlobalScale();
				ImGui::Text("%llu: %s %s", selected_node->id,selected_node->name.c_str(),selected_node->IsHighlighted()?"HIGHLIGHTED":"");
				avs::uid gi_uid=selected_node->GetGlobalIlluminationTextureUid();
				if (ImGui::BeginTable("selected", 2))
				{
					ImGui::TableNextColumn();
					ImGui::Text("Hidden");
					ImGui::TableNextColumn();
					bool hidden=!selected_node->IsVisible();
					ImGui::Checkbox("isHidden", &hidden);
					ImGui::TableNextColumn();
					ImGui::Text("Stationary");
					ImGui::TableNextColumn();
					bool stationary=selected_node->IsStatic();
					ImGui::Checkbox("isStatic", &stationary);
					DoRow("GI"			,"%d", gi_uid);
					DoRow("Local Pos"	,"%3.3f %3.3f %3.3f", pos.x, pos.y, pos.z);
					DoRow("Rot"			,"%3.3f %3.3f %3.3f %3.3f", q.x, q.y, q.z, q.w);
					DoRow("Scale"		,"%3.3f %3.3f %3.3f", sc.x, sc.y, sc.z);
					DoRow("global Pos"	,"%3.3f %3.3f %3.3f", gpos.x, gpos.y, gpos.z);
					DoRow("Rot"			,"%3.3f %3.3f %3.3f %3.3f", gq.x, gq.y, gq.z, gq.w);
					DoRow("Scale"		,"%3.3f %3.3f %3.3f", gs.x, gs.y, gs.z);
					if(selected_node->GetMesh())
					{
						auto m=selected_node->GetMesh();
						DoRow("Mesh"		,"%s", m->GetMeshCreateInfo().name.c_str());
					}
					ImGui::EndTable();
				}
				std::shared_ptr<clientrender::SkinInstance> skinInstance = selected_node->GetSkinInstance();
				if (skinInstance)
				{
					float anim_time_s=selected_node->animationComponent.GetCurrentAnimationTimeSeconds();

					ImGui::Text("Animation Time: %3.3f", anim_time_s);
					ImGui::BeginGroup();
					const auto &skin	=skinInstance->GetSkin();
					const auto &bones	=skinInstance->GetBones();
					{
						for (auto b : bones)
						{
							if(b->GetParent()==nullptr)
								BoneTreeNode(b, nullptr);
						}
						/*
					if (ImGui::BeginTable("Bones", 4))
					const auto& l = b->GetLocalTransform().m_Rotation;
							const auto& g = b->GetGlobalTransform().m_Rotation;
							ImGui::TableNextColumn(); 
							ImGui::Text("%d", b->id);
							ImGui::TableNextColumn();
							ImGui::Text("%s", b->name.c_str());
							ImGui::TableNextColumn();
							ImGui::Text(STR_VECTOR4,l.i, l.j, l.k, l.s);
							ImGui::TableNextColumn();
							ImGui::Text(STR_VECTOR4, g.i, g.j, g.k, g.s);
					
						ImGui::EndTable();	*/
					}
					ImGui::EndGroup();
				}
				for (const auto& m : selected_node->GetMaterials())
				{
					if(m)
					{
						ImGui::TreeNodeEx("", flags, "%llu: %s", m->id, m->GetMaterialCreateInfo().name.c_str());
						if (ImGui::IsItemClicked())
						{
							Select(m->id);
						}
					}
				}
				ImGui::End();
			}
			else if (selected_material.get())
			{
				const auto& mci = selected_material->GetMaterialCreateInfo();
				ImGui::Text("%llu: %s", selected_material->id, mci.name.c_str());
				if(mci.diffuse.texture.get())
				{
					ImGui::TreeNodeEx("", flags," Diffuse: %s",  mci.diffuse.texture->GetTextureCreateInfo().name.c_str());
					
					if (ImGui::IsItemClicked())
					{
						Select(mci.diffuse.texture->GetTextureCreateInfo().uid);
					}
				}

				if (mci.combined.texture.get())
				{
					ImGui::TreeNodeEx("", flags,"Combined: %s", mci.combined.texture->GetTextureCreateInfo().name.c_str());
					
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
		}
		ImGui::End();
	}
	in_debug_gui--;
	ImGui::PopFont();
	ImGui::Render();
	ImGui_ImplPlatform_RenderDrawData(deviceContext, ImGui::GetDrawData());
}

void Gui::BoneTreeNode(const std::shared_ptr<clientrender::Bone>& n, const char* search_text)
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
	}
	
	ImGui::EndGroup();
}

void Gui::Scene()
{
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
	ImGui::EndTabBar();
}

void Gui::NodeTree(const clientrender::NodeManager::nodeList_t& root_nodes)
{
	static const size_t bufferSize = 40;
	static char buffer[bufferSize];
	ImGui::InputText("Search: ", buffer, bufferSize);
	const char* search_text = nullptr;
	if (strlen(buffer) > 0)
		search_text = buffer;
	for(auto &r:root_nodes)
	{
		TreeNode(r, search_text);
	}
}

void Gui::Update(const std::vector<vec4>& h,bool have_vr)
{
	hand_pos_press = h;
	have_vr_device = have_vr;
}

void Gui::Render(GraphicsDeviceContext& deviceContext)
{
	view_pos = deviceContext.viewStruct.cam_pos;
	view_dir = deviceContext.viewStruct.view_dir;
	if(!visible)
		return;
	// Start the Dear ImGui frame
#ifdef _MSC_VER
	ImGui_ImplWin32_NewFrame();
#endif
#ifdef __ANDROID__
	ImGui_ImplAndroid_NewFrame();
#endif
	if (have_vr_device)
		ImGui_ImplPlatform_Update3DTouchPos(hand_pos_press);
	else
		ImGui_ImplPlatform_Update3DMousePos();
	ImGuiIO& io = ImGui::GetIO();
	static bool in3d=true;
	static float window_width=720.0f;
	static float window_height=240.0f;
	ImVec2 size_min(window_width,100.f);
	ImVec2 size_max(window_width+40.0f,window_height);
	ImGui_ImplPlatform_NewFrame(in3d,(int)size_max.x,(int)size_max.y,menu_pos,azimuth,tilt,width_m);
	static int refocus=0;
	bool show_hide=true;
	ImGui::NewFrame();
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
		ImGui::Begin("Teleport VR",&show_hide, windowFlags);
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
			io.AddKeyEvent(0x20b, false);
		}
		if(refocus>=2)
		{
			while(keys_pressed.size())
			{
				int k=keys_pressed[0];
				if(k==VK_BACK)
				{
					KeysDown[k] = true;
					//io.KeysDown[ImGuiKey_Backspace] = true;
					//io.AddInputCharacter(ImGuiKey_Backspace);
					io.AddKeyEvent(0x20b, true);
				}
				else
				{
					io.AddInputCharacter(k);
				}
				keys_pressed.erase(keys_pressed.begin());
			}
		}
	//	if(io.KeysDown[VK_ESCAPE])
		{
	//		show_hide=false;
		}
		static bool show_bookmarks=false;
		static bool show_options=false;
		if(show_options)
		{
            ImGui::Spacing();
            ImGui::SameLine(ImGui::GetWindowWidth()-70);
    //ImGui::PushItemWidth(-ImGui::GetWindowWidth() * 0.35f);
			if(ImGui::Button(ICON_FK_ARROW_LEFT,ImVec2(64,32)))
			{
				config->SaveOptions();
				show_options=false;
			}
			if (ImGui::BeginTable("options", 2))
			{
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 300.0f); // Default to 100.0f
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 400.0f); // Default to 200.0f
                ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::LabelText("##LobbyView","Lobby View");
				ImGui::TableNextColumn();
				int e = (int)config->options.lobbyView;
				ImGui::RadioButton("White", &e, 0);
				ImGui::SameLine();
				ImGui::RadioButton("Neon", &e, 1);
				if((client::LobbyView)e!=config->options.lobbyView)
				{
					config->options.lobbyView=(client::LobbyView)e;
				}
				ImGui::EndTable();
			}
		}
		else
		{
			if (ImGui::Button(ICON_FK_FOLDER_O,ImVec2(64,32)))
			{
				show_bookmarks=!show_bookmarks;
				selected_url="";
			}
			if(show_bookmarks)
			{
				const std::vector<client::Bookmark> &bookmarks=config->GetBookmarks();
				ImGui::SameLine();
				ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysVerticalScrollbar;
				ImGui::BeginChild("Bookmarks", ImVec2(-1,-1), true, window_flags);
				for (int i = 0; i < bookmarks.size(); i++)
				{
					const client::Bookmark &b=bookmarks[i];
					if(ImGui::TreeNodeEx(b.url.c_str(), ImGuiTreeNodeFlags_Leaf,"%s",b.title.c_str()))
					{
						if (ImGui::IsItemClicked())
						{
							selected_url=b.url;
						}
						else if(!ImGui::IsMouseDown(0)&&selected_url==b.url)
						{
							current_url=b.url;
							memcpy(buf,current_url.c_str(),std::min((size_t)499,current_url.size()));
							connectHandler(b.url);
							show_bookmarks=false;
							connecting=true;
						}
						ImGui::TreePop();
					}
				}
				ImGui::EndChild();
			}
			else
			{
				ImGui::SameLine();
				if(refocus==0)
				{
					ImGui::SetKeyboardFocusHere();
				}
				if(ImGui::InputText("##URL", buf, IM_ARRAYSIZE(buf)))//,ImGuiInputTextFlags))
				{
					current_url=buf;
				}
				refocus++;
				ImGui::SameLine();
				ImGui::BeginDisabled(connecting);
				if (ImGui::Button("Connect"))
				{
					if(connectHandler)
					{
						connectHandler(current_url);
						connecting=true;
					}
				}
				ImGui::EndDisabled();
				ImGui::SameLine(ImGui::GetWindowWidth()-70);
				if (ImGui::Button(ICON_FK_COG,ImVec2(64,32)))
				{
					show_options=!show_options;
				}
				if (show_keyboard)
				{
					auto KeyboardLine = [&io,this](const char* key)
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
									cancelConnectHandler();
									connecting=false;
								}
							}
							key++;
							if (i<num-1)
								ImGui::SameLine();
						}
					};
					KeyboardLine("1234567890-");
					ImGui::SameLine();
					ImGui::PushFont(symbolFont);
		//ImGui::Button(ICON_FK_SEARCH " Search");
					if (ImGui::Button(ICON_FK_LONG_ARROW_LEFT,ImVec2(92,32)))
					{
						 refocus=0;
						// keys_pressed.push_back(ImGuiKey_Backspace);
						 keys_pressed.push_back(VK_BACK);
					}
					ImGui::PopFont();
					ImGui::Text("  ");
					ImGui::SameLine();
					KeyboardLine("qwertyuiop");
					ImGui::Text("	");
					ImGui::SameLine();
					KeyboardLine("asdfghjkl:");
					ImGui::SameLine();
					static char buf[32] = "Return";
					if (ImGui::Button(buf,ImVec2(92,32)))
					{
						 refocus=0;
						 keys_pressed.push_back(ImGuiKey_Enter);
					}
					ImGui::Text("	  ");
					ImGui::SameLine();
					KeyboardLine("zxcvbnm,./");
				}
			}
		}
		//ShowFont();
		//ImGui_ImplPlatform_DebugInfo();
		//hasFocus = ImGui::IsAnyItemFocused(); Note: does not work.
		ImGui::End();
	}
	ImGui::Render();
	ImGui_ImplPlatform_RenderDrawData(deviceContext, ImGui::GetDrawData());
	if(!show_hide)
		Hide();
}

void Gui::SetConnectHandler(std::function<void(const std::string&)> fn)
{
	connectHandler = fn;
}
void Gui::SetCancelConnectHandler(std::function<void()> fn)
{
	cancelConnectHandler = fn;
}


void Gui::SetServerIPs(const std::vector<std::string> &s)
{
	server_ips=s;
	if(server_ips.size())
	{
		memcpy(buf,server_ips[0].c_str(),std::min(500,(int)server_ips[0].size()+1));//,server_ips[0].size());
		current_url = buf;
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