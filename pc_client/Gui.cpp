
#include <imgui.h>
#include "backends/imgui_impl_win32.h"
#include "Platform/ImGui/imgui_impl_platform.h"
#include "Platform/Core/DefaultFileLoader.h"
#include "Platform/Core/StringToWString.h"
#include "Gui.h"
#include "IconsForkAwesome.h"
#include "ErrorHandling.h"
#include <direct.h>
using namespace teleport;
using namespace simul;
using namespace platform;
using namespace crossplatform;
ImFont *smallFont=nullptr;
void Gui::RestoreDeviceObjects(simul::crossplatform::RenderPlatform* r)
{
	renderPlatform=r;
	if(!r)
		return;
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
    style.FramePadding = ImVec2(0.f, 0.f);
    style.FramePadding.x=8.f;

  /*  style.Colors[ImGuiCol_Text] = ImVec4(0.73f, 0.73f, 0.73f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.26f, 0.26f, 0.26f, 0.95f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.74f, 0.74f, 0.74f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.74f, 0.74f, 0.74f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.43f, 0.43f, 0.43f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.32f, 0.52f, 0.65f, 1.00f);*/

	ImGui_ImplWin32_Init(GetActiveWindow());
	ImGui_ImplPlatform_Init(r);

    // NB: Transfer ownership of 'ttf_data' to ImFontAtlas, unless font_cfg_template->FontDataOwnedByAtlas == false. Owned TTF buffer will be deleted after Build().

    simul::base::DefaultFileLoader fileLoader;
    std::vector<std::string> texture_paths;
    texture_paths.push_back("textures");
    ImGuiIO& io = ImGui::GetIO();
    char cwd[100];
    _getcwd(cwd,100);
    auto AddFont=[texture_paths,&fileLoader,cwd,&io](const char *font_filename,float size_pixels=32.f,ImFontConfig *config=nullptr,ImWchar *ranges=nullptr)->ImFont*
    {
        int idx = fileLoader.FindIndexInPathStack(font_filename, texture_paths);
        if(idx<=-2)
        {
            TELEPORT_CERR<<font_filename<<" not found.\n";
            return nullptr;
        }
        void *ttf_data;
        unsigned int ttf_size;
        fileLoader.AcquireFileContents(ttf_data,ttf_size,((texture_paths[idx]+"/")+ font_filename).c_str(),false);
        return io.Fonts->AddFontFromMemoryTTF(ttf_data,ttf_size,size_pixels,config,ranges);
    };
    AddFont("Exo-SemiBold.ttf");
    {
        ImFontConfig config;
        config.MergeMode = true;
        config.GlyphMinAdvanceX = 13.0f; 
        ImVector<ImWchar> ranges;
        ImFontGlyphRangesBuilder builder;
        builder.AddText(ICON_FK_LONG_ARROW_LEFT);                  
        builder.BuildRanges(&ranges);                          // Build the final result (ordered ranges with all the unique characters submitted)
        AddFont("forkawesome-webfont.ttf",32.f,&config,ranges.Data);
        io.Fonts->Build();                                     // Build the atlas while 'ranges' is still in scope and not deleted.
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
        ImGui_ImplWin32_Shutdown();
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
    visible = true;
    menu_pos = view_pos;
    static float z_offset = -.3f;
    static float distance = 0.4f;
    azimuth= atan2f(-view_dir.x, view_dir.y);
    tilt = 3.1415926536f / 4.0f;
    vec3 menu_offset = { distance * -sin(azimuth),distance * cos(azimuth),z_offset };
    menu_pos += menu_offset;
    keys_pressed.clear();
}

void Gui::Hide()
{
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
                if (!TreeNode((void*)(intptr_t)base, "U+%04X..U+%04X (%d %s)", base, base + 255, count, count > 1 ? "glyphs" : "glyph"))
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

void Gui::NodeTree(const scr::Node& node)
{
    bool has_children=node.GetChildren().size()!=0;
    bool open=ImGui::TreeNodeEx(node.name.c_str(),(has_children?0:ImGuiTreeNodeFlags_Leaf ) );
  //  ImGui::TableNextColumn();
  //  ImGui::Text("%lu", node.id);
    if(open)
    {
        for(const auto &r:node.GetChildren())
        {
            NodeTree(*r.lock().get());
        }
        ImGui::TreePop();
    }
}

void Gui::DebugGui(simul::crossplatform::GraphicsDeviceContext& deviceContext,const scr::NodeManager::nodeList_t& root_nodes)
{
	ImGui_ImplWin32_NewFrame();
    auto vp=renderPlatform->GetViewport(deviceContext,0);
    ImGui_ImplPlatform_NewFrame(false,vp.w,vp.h);
	ImGui::NewFrame();
    static int corner = 1;
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::PushFont(smallFont);
    
    if (ImGui::Begin("Nodes"))
    {
        for(const auto &r:root_nodes)
        {
                NodeTree(*r.get());
        }
    }
    ImGui::End();

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (corner != -1)
    {
        const float PAD = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;
        window_pos.x = (work_pos.x + work_size.x - PAD);
        window_pos.y = (work_pos.y + PAD);
        window_pos_pivot.x =  1.0f ;
        window_pos_pivot.y =  0.0f;
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
    }
    ImGui::End();
    ImGui::PopFont();
    ImGui::Render();
	ImGui_ImplPlatform_RenderDrawData(deviceContext, ImGui::GetDrawData());
}
std::vector<vec4> hand_pos_press;
void Gui::Update(const std::vector<vec4>& h)
{
    hand_pos_press = h;
}
void Gui::Render(simul::crossplatform::GraphicsDeviceContext& deviceContext)
{
    view_pos = deviceContext.viewStruct.cam_pos;
    view_dir = deviceContext.viewStruct.view_dir;
    if(!visible)
        return;
	// Start the Dear ImGui frame
	ImGui_ImplWin32_NewFrame();
    ImGui_ImplPlatform_Update3DTouchPos(hand_pos_press);
    ImGuiIO& io = ImGui::GetIO();
    static bool in3d=true;
    ImVec2 size_min(720.f,100.f);
    ImVec2 size_max(720.f,240.f);
    ImGui_ImplPlatform_NewFrame(in3d,(int)size_max.x,(int)size_max.y,menu_pos,azimuth,tilt,width_m);
    static int refocus=0;
    bool show_hide=true;
	ImGui::NewFrame();
    {
        bool show_keyboard = true;
        ImGuiWindowFlags windowFlags =0;
        if(in3d)
        {
            ImGui::SetNextWindowPos(ImVec2(0, 0));                          // always at the window origin
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
        if(refocus==0)
        {
            ImGui::SetKeyboardFocusHere();
        }
        io.KeysDown[VK_BACK] = false;
        if(refocus>=2)
        {
            while(keys_pressed.size())
            {
               int k=keys_pressed[0];
                if(k==VK_BACK)
                {
                    io.KeysDown[k] = true;
                }
                else
                {
                    io.AddInputCharacter(k);
                }
                keys_pressed.erase(keys_pressed.begin());
            }
        }
        if(io.KeysDown[VK_ESCAPE])
        {
            show_hide=false;
        }
        if(ImGui::InputText("", buf, IM_ARRAYSIZE(buf)))
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
        if (show_keyboard)
        {
            auto KeyboardLine = [&io,this](const char* key)
            {
                size_t num = strlen(key);
                for (size_t i = 0; i < num; i++)
                {
                    char key_label[] = "X";
                    key_label[0] = *key;
                    if (ImGui::Button(key_label,ImVec2(46,32)))
                    {
                        refocus=0;
                        keys_pressed.push_back(*key);
                    }
                    key++;
                    if (i<num-1)
                        ImGui::SameLine();
                }
            };
            KeyboardLine("1234567890-");
            ImGui::SameLine();
            if (ImGui::Button(ICON_FK_LONG_ARROW_LEFT,ImVec2(92,32)))
            {
                 refocus=0;
                // keys_pressed.push_back(ImGuiKey_Backspace);
                 keys_pressed.push_back(VK_BACK);
            }
            ImGui::Text("  ");
            ImGui::SameLine();
            KeyboardLine("qwertyuiop");
            ImGui::Text("    ");
            ImGui::SameLine();
            KeyboardLine("asdfghjkl:");
            ImGui::SameLine();
            static char buf[32] = "Return";
            if (ImGui::Button(buf,ImVec2(92,32)))
            {
                 refocus=0;
                 keys_pressed.push_back(ImGuiKey_Enter);
            }
            ImGui::Text("      ");
            ImGui::SameLine();
            KeyboardLine("zxcvbnm,./");
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


void Gui::SetServerIPs(const std::vector<std::string> &s)
{
    server_ips=s;
    if(server_ips.size())
    {
        strcpy(buf,server_ips[0].c_str());//,server_ips[0].size());
        current_url = buf;
    }

}
