#include "TeleportClient/OpenXRRenderModel.h"
#include "TeleportClient/OpenXR.h"
#include "TeleportCore/Logging.h"

using namespace teleport;
using namespace client;
/*
typedef struct XrControllerModelKeyStateMSFT {
    XrStructureType             type;
    void* XR_MAY_ALIAS          next;
    XrControllerModelKeyMSFT    modelKey;
} XrControllerModelKeyStateMSFT;

typedef struct XrControllerModelNodePropertiesMSFT {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    char                  parentNodeName[XR_MAX_CONTROLLER_MODEL_NODE_NAME_SIZE_MSFT];
    char                  nodeName[XR_MAX_CONTROLLER_MODEL_NODE_NAME_SIZE_MSFT];
} XrControllerModelNodePropertiesMSFT;

typedef struct XrControllerModelPropertiesMSFT {
    XrStructureType                         type;
    void* XR_MAY_ALIAS                      next;
    uint32_t                                nodeCapacityInput;
    uint32_t                                nodeCountOutput;
    XrControllerModelNodePropertiesMSFT*    nodeProperties;
} XrControllerModelPropertiesMSFT;

typedef struct XrControllerModelNodeStateMSFT {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    XrPosef               nodePose;
} XrControllerModelNodeStateMSFT;

typedef struct XrControllerModelStateMSFT {
    XrStructureType                    type;
    void* XR_MAY_ALIAS                 next;
    uint32_t                           nodeCapacityInput;
    uint32_t                           nodeCountOutput;
    XrControllerModelNodeStateMSFT*    nodeStates;
} XrControllerModelStateMSFT;

typedef XrResult (XRAPI_PTR *PFN_xrGetControllerModelKeyMSFT)(XrSession session, XrPath topLevelUserPath, XrControllerModelKeyStateMSFT* controllerModelKeyState);
typedef XrResult (XRAPI_PTR *PFN_xrLoadControllerModelMSFT)(XrSession session, XrControllerModelKeyMSFT modelKey, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, uint8_t* buffer);
typedef XrResult (XRAPI_PTR *PFN_xrGetControllerModelPropertiesMSFT)(XrSession session, XrControllerModelKeyMSFT modelKey, XrControllerModelPropertiesMSFT* properties);
typedef XrResult (XRAPI_PTR *PFN_xrGetControllerModelStateMSFT)(XrSession session, XrControllerModelKeyMSFT modelKey, XrControllerModelStateMSFT* state);
*/
OpenXRRenderModel::OpenXRRenderModel(XrInstance inst):OpenXRExtension(inst)
{
	/// Hook up extensions for device settings

	// First try Facebook extension.
XR_CHECK(xrGetInstanceProcAddr(xr_instance,"xrEnumerateRenderModelPathsFB"	,(PFN_xrVoidFunction*)(&xrEnumerateRenderModelPathsFB)));
	XR_CHECK(xrGetInstanceProcAddr(xr_instance,"xrGetRenderModelPropertiesFB"	,(PFN_xrVoidFunction*)(&xrGetRenderModelPropertiesFB)));
	XR_CHECK(xrGetInstanceProcAddr(xr_instance, "xrLoadRenderModelFB"			,(PFN_xrVoidFunction*)(&xrLoadRenderModelFB)));

	// Now try the Microsoft extension.
	
	XR_CHECK(xrGetInstanceProcAddr(xr_instance, "xrGetControllerModelKeyMSFT"		,(PFN_xrVoidFunction*)(&xrGetControllerModelKeyMSFT)));
	XR_CHECK(xrGetInstanceProcAddr(xr_instance, "xrLoadControllerModelMSFT"			,(PFN_xrVoidFunction*)(&xrLoadControllerModelMSFT)));
	XR_CHECK(xrGetInstanceProcAddr(xr_instance, "xrGetControllerModelPropertiesMSFT",(PFN_xrVoidFunction*)(&xrGetControllerModelPropertiesMSFT)));
	XR_CHECK(xrGetInstanceProcAddr(xr_instance, "xrGetControllerModelStateMSFT"		,(PFN_xrVoidFunction*)(&xrGetControllerModelStateMSFT)));
}

void OpenXRRenderModel::OneTimeInitialize()
{
	if (initialized)
		return;
	if (xrEnumerateRenderModelPathsFB)
	{
		/// query path count
		uint32_t pathCount = 0;
		XR_CHECK(xrEnumerateRenderModelPathsFB(xr_session, pathCount, &pathCount, nullptr));
		if (pathCount > 0)
		{
			TELEPORT_LOG("OpenXRRenderModel: found %u models ", pathCount);
			paths_.resize(pathCount, { XR_TYPE_RENDER_MODEL_PATH_INFO_FB });
			/// Fill in the path data
			XR_CHECK(xrEnumerateRenderModelPathsFB(xr_session, pathCount, &pathCount, &paths_[0]));
			/// Print paths for debug purpose
			for (const auto& p : paths_)
			{
				char buf[256];
				uint32_t bufCount = 0;
				XR_CHECK(xrPathToString(xr_instance, p.path, bufCount, &bufCount, nullptr));
				XR_CHECK(xrPathToString(xr_instance, p.path, bufCount, &bufCount, &buf[0]));
				TELEPORT_LOG("OpenXRRenderModel: path=%u `%s`", (uint32_t)p.path, &buf[0]);
			}
			/// Get properties
			for (const auto& p : paths_)
			{
				XrRenderModelPropertiesFB prop{ XR_TYPE_RENDER_MODEL_PROPERTIES_FB };
				XrResult result = xrGetRenderModelPropertiesFB(xr_session, p.path, &prop);
				if (result == XR_SUCCESS)
				{
					properties_.push_back(prop);
				}
			}
		}
	}
	if(xrGetControllerModelKeyMSFT)
	{
		XrControllerModelKeyStateMSFT controllerModelKeyState;
		XrPath topLevelUserPath=MakeXrPath(xr_instance,"input/user/left");
		xrGetControllerModelKeyMSFT(xr_session,topLevelUserPath,&controllerModelKeyState);
	}
	initialized = true;
}

std::vector<uint8_t> OpenXRRenderModel::LoadRenderModel(std::string path)
{
	OneTimeInitialize();
	std::vector<uint8_t> buffer;
	std::string strToCheck=path;
/*	if (remote)
	{
		strToCheck = "/model_fb/keyboard/remote";
	}
	else
	{
		strToCheck = "/model_fb/keyboard/local";
	}*/

	for (const auto& p : paths_)
	{
		char buf[256];
		uint32_t bufCount = 0;
		// OpenXR two call pattern. First call gets buffer size, second call gets the buffer
		// data
		XR_CHECK(xrPathToString(xr_instance, p.path, bufCount, &bufCount, nullptr));
		XR_CHECK(xrPathToString(xr_instance, p.path, bufCount, &bufCount, &buf[0]));
		std::string pathString = buf;
		if (pathString.rfind(strToCheck, 0) == 0)
		{
			XrRenderModelPropertiesFB prop{ XR_TYPE_RENDER_MODEL_PROPERTIES_FB };
			XrResult result = xrGetRenderModelPropertiesFB(xr_session, p.path, &prop);
			if (result == XR_SUCCESS)
			{
				if (prop.modelKey != XR_NULL_RENDER_MODEL_KEY_FB)
				{
					XrRenderModelLoadInfoFB loadInfo = { XR_TYPE_RENDER_MODEL_LOAD_INFO_FB };
					loadInfo.modelKey = prop.modelKey;

					XrRenderModelBufferFB rmb{ XR_TYPE_RENDER_MODEL_BUFFER_FB };
					rmb.next = nullptr;
					rmb.bufferCapacityInput = 0;
					rmb.buffer = nullptr;
					XrResult rs=xrLoadRenderModelFB(xr_session, &loadInfo, &rmb);
					if (XR_UNQUALIFIED_SUCCESS(rs))
					{
						TELEPORT_LOG(
							"OpenXRRenderModel: loading modelKey %u size %u ",
							prop.modelKey,
							rmb.bufferCountOutput);
						buffer.resize(rmb.bufferCountOutput);
						rmb.buffer = (uint8_t*)buffer.data();
						rmb.bufferCapacityInput = rmb.bufferCountOutput;
						if (!XR_UNQUALIFIED_SUCCESS(xrLoadRenderModelFB(xr_session, &loadInfo, &rmb)))
						{
							TELEPORT_LOG(
								"OpenXRRenderModel: FAILED to load modelKey %u on pass 2",
								prop.modelKey);
							buffer.resize(0);
						}
						else
						{
							TELEPORT_LOG(
								"OpenXRRenderModel: loaded modelKey %u buffer size is %u",
								prop.modelKey,
								buffer.size());
							return buffer;
						}
					}
					else
					{
						TELEPORT_LOG(
							"OpenXRRenderModel: FAILED to load modelKey %u on pass 1",
							prop.modelKey);
					}
				}
			}
			else
			{
				TELEPORT_LOG(
					"OpenXRRenderModel: FAILED to load prop for path '%s'",
					pathString.c_str());
			}
		}
	}

	return buffer;
}