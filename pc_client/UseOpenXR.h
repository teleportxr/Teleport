#include "Platform/CrossPlatform/RenderPlatform.h"
#include <vector>
#include "Platform/CrossPlatform/RenderDelegate.h"
#include "common_maths.h"		// for avs::Pose
typedef int64_t XrTime;
struct XrCompositionLayerProjectionView;
struct XrCompositionLayerProjection;
struct swapchain_surfdata_t;

namespace teleport
{
	class UseOpenXR
	{
	public:
		bool Init(simul::crossplatform::RenderPlatform* renderPlatform, const char* app_name);
		void MakeActions();
		void PollActions();
		void RenderFrame(simul::crossplatform::GraphicsDeviceContext& deviceContext, simul::crossplatform::RenderDelegate &,vec3 origin);
		void Shutdown();
		void PollEvents(bool& exit);
		bool HaveXRDevice() const;
		const avs::Pose& GetHeadPose() const;
	protected:
		simul::crossplatform::RenderPlatform* renderPlatform = nullptr;
		bool haveXRDevice = false;
		void RenderLayer(simul::crossplatform::GraphicsDeviceContext& deviceContext, XrCompositionLayerProjectionView& view, swapchain_surfdata_t& surface, simul::crossplatform::RenderDelegate& renderDelegate, vec3 origin);
		bool RenderLayer(simul::crossplatform::GraphicsDeviceContext& deviceContext,XrTime predictedTime, std::vector<XrCompositionLayerProjectionView>& views, XrCompositionLayerProjection& layer, simul::crossplatform::RenderDelegate& renderDelegate, vec3 origin);
		avs::Pose headPose;
	};
}
