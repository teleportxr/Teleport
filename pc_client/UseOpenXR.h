#include "Platform/CrossPlatform/RenderPlatform.h"
#include <vector>
#include "Platform/CrossPlatform/RenderDelegate.h"
#include "Platform/CrossPlatform/Texture.h"
#include "common_maths.h"		// for avs::Pose
#include "common_networking.h"		// for avs::InputState
#include "TeleportClient/Input.h"
typedef int64_t XrTime;
struct XrCompositionLayerProjectionView;
struct XrCompositionLayerProjection;
struct swapchain_surfdata_t;
typedef enum XrResult;

namespace teleport
{
	class UseOpenXR
	{
	public:
		bool Init(platform::crossplatform::RenderPlatform* renderPlatform, const char* app_name);
		void MakeActions();
		void PollActions();
		void RenderFrame(platform::crossplatform::GraphicsDeviceContext& deviceContext, platform::crossplatform::RenderDelegate &, vec3 origin_pos, vec4 origin_orientation);
		void Shutdown();
		void PollEvents(bool& exit);
		bool HaveXRDevice() const;
		void OnInputsSetupChanged(const std::vector<avs::InputDefinition> &inputDefinitions_);
		const avs::Pose& GetHeadPose() const;
		const avs::Pose& GetControllerPose(int index) const;
		const teleport::client::Input& GetInputs() const;
		size_t GetNumControllers() const
		{
			return controllerPoses.size();
		};
		void SetMenuButtonHandler(std::function<void()> f)
		{
			menuButtonHandler = f;
		}
		platform::crossplatform::Texture* GetRenderTexture(int index=0);
	protected:
	
		platform::crossplatform::RenderPlatform* renderPlatform = nullptr;
		bool haveXRDevice = false;
		void RenderLayer(platform::crossplatform::GraphicsDeviceContext& deviceContext, XrCompositionLayerProjectionView& view, swapchain_surfdata_t& surface, platform::crossplatform::RenderDelegate& renderDelegate, vec3 origin_pos, vec4 origin_orientation);
		bool RenderLayer(platform::crossplatform::GraphicsDeviceContext& deviceContext,XrTime predictedTime, std::vector<XrCompositionLayerProjectionView>& views, XrCompositionLayerProjection& layer, platform::crossplatform::RenderDelegate& renderDelegate, vec3 origin_pos, vec4 origin_orientation);
		avs::Pose headPose;
		std::vector<avs::Pose> controllerPoses;
		teleport::client::Input inputs;
		void openxr_poll_predicted(XrTime predicted_time);
		std::function<void()> menuButtonHandler;
	};
}
