
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
    ImGui_ImplPlatform_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

}

void Gui::RecompileShaders()
{
	ImGui_ImplPlatform_RecompileShaders();
}


void Gui::Render(simul::crossplatform::GraphicsDeviceContext& deviceContext)
{
	// Start the Dear ImGui frame
	ImGui_ImplWin32_NewFrame();
    ImGui_ImplPlatform_NewFrame(true,600,300);
    // Override what win32 did with this
    ImGui_ImplPlatform_Update3DMousePos();
	ImGui::NewFrame();
    {
        static bool show_demo_window=false;
        static bool show_another_window = false;
        static float f = 0.0f;
        static int counter = 0;
        static vec4 clear_color(0,0,0,0);
        ImGui::SetNextWindowPos(ImVec2(0, 0));                    // always at the window origin
        ImGui::SetNextWindowSize(ImVec2(float(600), float(300)));    // always at the window size
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoBringToFrontOnFocus |         
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse|
            ImGuiWindowFlags_NoScrollbar;
        ImGui::Begin("Hello, world!",nullptr, windowFlags);                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("A");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui_ImplPlatform_DebugInfo();
       
        ImGui::End();
    }
    ImGui::Render();
	ImGui_ImplPlatform_RenderDrawData(deviceContext, ImGui::GetDrawData(),true);
}