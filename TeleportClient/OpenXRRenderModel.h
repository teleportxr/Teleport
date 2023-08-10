#pragma once
#include "TeleportCore/ErrorHandling.h"
#include "openxr/openxr.h"
#include <vector>
namespace teleport
{
	namespace client
	{
		class OpenXRExtension
		{
		protected:
			XrInstance xr_instance= XR_NULL_HANDLE;
			XrSession xr_session = XR_NULL_HANDLE;
			bool initialized = false;
		public:
			OpenXRExtension(XrInstance inst)
			{
				xr_instance=inst;
			}
			virtual ~OpenXRExtension() = default;
			virtual bool SessionInit(XrSession session)
			{
				initialized = false;
				xr_session = session;
				return true;
			}
			virtual bool SessionEnd()
			{
				xr_session = XR_NULL_HANDLE;
				return true;
			}
			virtual bool Update(XrSpace currentSpace, XrTime predictedDisplayTime) = 0;
			virtual std::vector<const char*> GetRequiredExtensionNames()=0;
		};

		class OpenXRRenderModel:public OpenXRExtension
		{
		public:
			OpenXRRenderModel(XrInstance inst);

			virtual ~OpenXRRenderModel()
			{
				xrEnumerateRenderModelPathsFB = nullptr;
				xrGetRenderModelPropertiesFB= nullptr;
				xrLoadRenderModelFB = nullptr;
				xrGetControllerModelKeyMSFT			= nullptr;
				xrLoadControllerModelMSFT			= nullptr;
				xrGetControllerModelPropertiesMSFT	= nullptr;
				xrGetControllerModelStateMSFT		= nullptr;
			};

			bool Update(XrSpace currentSpace, XrTime predictedDisplayTime) override
			{
				OneTimeInitialize();
				return true;
			}
	
			std::vector<const char*> GetRequiredExtensionNames() override
			{
				return RequiredExtensionNames();
			}
			static std::vector<const char*> RequiredExtensionNames()
			{
				return { XR_FB_RENDER_MODEL_EXTENSION_NAME };
			}
		public:
			/// Own interface
			const std::vector<XrRenderModelPathInfoFB>& GetPaths() const
			{
				return paths_;
			}
			const std::vector<XrRenderModelPropertiesFB>& GetProperties() const
			{
				return properties_;
			}

			std::vector<uint8_t> LoadRenderModel(std::string path);

		private:
			void OneTimeInitialize();

			/// RenderModel - extension functions
			PFN_xrEnumerateRenderModelPathsFB xrEnumerateRenderModelPathsFB = nullptr;
			PFN_xrGetRenderModelPropertiesFB xrGetRenderModelPropertiesFB = nullptr;
			PFN_xrLoadRenderModelFB xrLoadRenderModelFB = nullptr;
	
			PFN_xrGetControllerModelKeyMSFT				xrGetControllerModelKeyMSFT			= nullptr;
			PFN_xrLoadControllerModelMSFT				xrLoadControllerModelMSFT			= nullptr;
			PFN_xrGetControllerModelPropertiesMSFT		xrGetControllerModelPropertiesMSFT	= nullptr;
			PFN_xrGetControllerModelStateMSFT			xrGetControllerModelStateMSFT		= nullptr;
			/// RenderModel - data buffers
			std::vector<XrRenderModelPathInfoFB> paths_;
			std::vector<XrRenderModelPropertiesFB> properties_;
		};
	}
}
