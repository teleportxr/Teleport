#pragma once
#ifdef _MSC_VER

#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/HdrRenderer.h"
#include "Platform/CrossPlatform/MeshRenderer.h"
#include "Platform/Shaders/SL/CppSl.sl"
#include "Platform/Shaders/SL/camera_constants.sl"

#include <libavstream/libavstream.hpp>
#include <libavstream/surfaces/surface_interface.hpp>
#include <libavstream/geometry/mesh_interface.hpp>
#include <libavstream/audio/audiotarget.h>

#include "TeleportClient/SessionClient.h"
#include "ClientRender/ResourceCreator.h"
#include "ClientRender/ResourceManager.h"
#include "ClientRender/IndexBuffer.h"
#include "ClientRender/Texture.h"
#include "ClientRender/UniformBuffer.h"
#include "ClientRender/VertexBuffer.h"
#include "ClientRender/Renderer.h"
#include "TeleportClient/OpenXR.h"
#include "TeleportClient/ClientDeviceState.h"

#ifdef _MSC_VER
#include "TeleportAudio/AudioStreamTarget.h"
#include "TeleportAudio/PC_AudioPlayer.h"
#include "MemoryUtil.h"
#endif

#include "TeleportClient/ClientPipeline.h"

namespace avs
{
	typedef LARGE_INTEGER Timestamp;
}

namespace clientrender
{
	class Material;
}

namespace pc_client
{
	class IndexBuffer;
	class Shader;
	class Texture;
	class UniformBuffer;
	class VertexBuffer;
}

namespace teleport
{
	/// @brief The renderer for a client connection.
	class ClientRenderer :public clientrender::Renderer
	{
	public:
		ClientRenderer(teleport::client::ClientDeviceState *clientDeviceState,teleport::client::SessionClient *sessionClient, teleport::Gui &g,bool dev);
		~ClientRenderer();
	
	};
}
#endif