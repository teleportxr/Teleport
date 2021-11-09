
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
    menu_pos.y += 1.5f;
}



void Gui::Hide()
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
    ImGui_ImplPlatform_NewFrame(true,600,100,menu_pos);
    // Override what win32 did with this
    ImGui_ImplPlatform_Update3DMousePos();
	ImGui::NewFrame();
    {
        bool show_keyboard = true;
        ImGui::SetNextWindowPos(ImVec2(0, 0));                    // always at the window origin
       // ImGui::SetNextWindowSize(ImVec2(float(600), float(100)));    // always at the window size
        ImVec2 size_min(600.f, 100.f);
        ImVec2 size_max(600.f, 600.f);
     /*   ImGui::SetNextWindowSizeConstraints(size_min, size_max);
        if(!show_keyboard)
            ImGui::SetNextWindowSize(size_min);
        else
            ImGui::SetNextWindowSize(size_max);*/
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse|
            ImGuiWindowFlags_NoScrollbar;
        ImGui::LogToTTY();
        ImGui::Begin("Teleport VR",nullptr, windowFlags);                          // Create a window called "Hello, world!" and append into it.
        static char buf[500];
        if(ImGui::InputText("", buf, IM_ARRAYSIZE(buf)))
        {
            current_url=buf;
        }
        ImGui::SameLine();
        if (ImGui::Button("Connect"))
        {
            if(connectHandler)
            {
                connectHandler(current_url);
            }
        }
        ImGuiIO& io = ImGui::GetIO();
        if (show_keyboard)
        {
            auto KeyboardLine = [&io](const char* key)
            {
                size_t num = strlen(key);
                for (size_t i = 0; i < num; i++)
                {
                    char key_label[] = "X";
                    key_label[0] = *key;
                    if (ImGui::Button(key_label,ImVec2(50,40)))
                    {
                        io.AddInputCharacter(*key);
                    }
                    key++;
                    if (i<num-1)
                        ImGui::SameLine();
                }
            };
            KeyboardLine("1234567890-");
            KeyboardLine("qwertyuiop");
            KeyboardLine("asdfghjkl");
        /*    ImGui::SameLine();
            if (ImGui::Button("Return"))
            {
            }*/
            KeyboardLine("zxcvbnm,./");
        }
        ImGui_ImplPlatform_DebugInfo();

        //hasFocus = ImGui::IsAnyItemFocused(); Note: does not work.
        ImGui::End();
    }
    ImGui::Render();
	ImGui_ImplPlatform_RenderDrawData(deviceContext, ImGui::GetDrawData(),true);
}
void Gui::SetConnectHandler(std::function<void(const std::string&)> fn)
{
    connectHandler = fn;
}