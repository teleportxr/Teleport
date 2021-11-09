
#include <imgui.h>
#include "backends/imgui_impl_win32.h"
#include "Platform/ImGui/imgui_impl_platform.h"
#include "Platform/Core/DefaultFileLoader.h"
#include "Gui.h"
#include <direct.h>

using namespace teleport;
using namespace simul;
using namespace platform;
using namespace crossplatform;

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
    std::string fontFile="Exo-SemiBold.ttf";
    size_t idx = fileLoader.FindIndexInPathStack(fontFile.c_str(), texture_paths);
    void *ttf_data;
    unsigned int ttf_size;
    char cwd[100];
    _getcwd(cwd,100);
    fileLoader.AcquireFileContents(ttf_data,ttf_size,((texture_paths[idx]+"/")+ fontFile).c_str(),false);
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromMemoryTTF(ttf_data,ttf_size,32.0f);
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
    menu_pos.y += 0.5f;
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

void Gui::Render(simul::crossplatform::GraphicsDeviceContext& deviceContext)
{
    view_pos = deviceContext.viewStruct.cam_pos;
    if(!visible)
        return;
	// Start the Dear ImGui frame
	ImGui_ImplWin32_NewFrame();
    static bool refocus=true;
    static bool focused_on_edit=false;
    ImGuiIO& io = ImGui::GetIO();
    static bool in3d=true;
     ImVec2 size_min(640.f, 100.f);
     ImVec2 size_max(640.f,260.f);
    ImGui_ImplPlatform_NewFrame(in3d,size_max.x,size_max.y,menu_pos,width_m);
    // Override what win32 did with this
    ImGui_ImplPlatform_Update3DMousePos();
	ImGui::NewFrame();
    {
        bool show_keyboard = true;
        ImGui::SetNextWindowPos(ImVec2(0, 0));                    // always at the window origin
       // ImGui::SetNextWindowSize(ImVec2(float(600), float(100)));    // always at the window size
       ImGui::SetNextWindowSizeConstraints(size_min, size_max);
        if(!show_keyboard)
            ImGui::SetNextWindowSize(size_min);
        else
            ImGui::SetNextWindowSize(size_max);
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse|
            ImGuiWindowFlags_NoScrollbar;
        ImGui::LogToTTY();
        ImGui::Begin("Teleport VR",nullptr, windowFlags);                          // Create a window called "Hello, world!" and append into it.
        static char buf[500];
        if(refocus)
        {
            ImGui::SetKeyboardFocusHere();
        }
        if(focused_on_edit)
        {
            while(keys_pressed.size())
            {
                char inp[10];
                inp[0]=keys_pressed[0];
                inp[1]=0;
                io.AddInputCharacter(keys_pressed[0]);
                keys_pressed.erase(keys_pressed.begin());
            }
        }
        if(ImGui::InputText("", buf, IM_ARRAYSIZE(buf)))
        {
            current_url=buf;
        }
        if(refocus)
        {
            refocus=false;
            focused_on_edit=true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Connect"))
        {
            if(connectHandler)
            {
                connectHandler(current_url);
            }
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
                    if (ImGui::Button(key_label,ImVec2(46,32)))
                    {
                        refocus=true;
                        focused_on_edit=false;
                        keys_pressed.push_back(*key);
                    }
                    key++;
                    if (i<num-1)
                        ImGui::SameLine();
                }
            };
            ImGui::Text("  ");
            KeyboardLine("1234567890-");
            ImGui::Text("  ");
            ImGui::SameLine();
            KeyboardLine("qwertyuiop");
            ImGui::Text("    ");
            ImGui::SameLine();
            KeyboardLine("asdfghjkl");
            ImGui::SameLine();
            static char buf[32] = "Return";
            if (ImGui::Button(buf,ImVec2(92,32)))
            {
            }
            ImGui::Text("      ");
            ImGui::SameLine();
            KeyboardLine("zxcvbnm,./");
        }
        //ImGui_ImplPlatform_DebugInfo();

        //hasFocus = ImGui::IsAnyItemFocused(); Note: does not work.
        ImGui::End();
    }
    ImGui::Render();
	ImGui_ImplPlatform_RenderDrawData(deviceContext, ImGui::GetDrawData());
}
void Gui::SetConnectHandler(std::function<void(const std::string&)> fn)
{
    connectHandler = fn;
}