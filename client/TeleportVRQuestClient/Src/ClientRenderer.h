//
// (C) Copyright 2018 Simul
#pragma once
#include "basic_linear_algebra.h"
#include "ClientRender/ResourceCreator.h"
#include "ClientRender/Renderer.h"
#include "GlobalGraphicsResources.h"
#include <Render/SurfaceTexture.h>
#include <libavstream/libavstream.hpp>
#include <VrApi_Input.h>
#include <FrameParams.h>
#include <TeleportClient/ClientPipeline.h>

#include "SCR_Class_GL_Impl/GL_DeviceContext.h"
#include "Controllers.h"
#include "ClientAppInterface.h"


// Placeholders for lights
struct DirectionalLight
{
	avs::vec3 direction;
	float pad;
    avs::vec4 color;
	ovrMatrix4f shadowViewMatrix;
	ovrMatrix4f shadowProjectionMatrix;
};

struct PointLight
{
    avs::vec3 position;
	float range;
    avs::vec3 attenuation;
	float pad;
    avs::vec4 color;
	ovrMatrix4f shadowViewMatrix;
	ovrMatrix4f shadowProjectionMatrix;
};

struct SpotLight
{
    avs::vec3 position;
	float  range;
    avs::vec3 direction;
	float cone;
    avs::vec3 attenuation;
	float pad;
    avs::vec4 color;
	ovrMatrix4f shadowViewMatrix;
	ovrMatrix4f shadowProjectionMatrix;
};

// ALL light data is passed in as tags.
struct __attribute__ ((packed)) LightTag
{
	ovrMatrix4f worldToShadowMatrix;
	avs::vec2 shadowTexCoordOffset;
	avs::vec2 shadowTexCoordScale;
	avs::vec4 colour;
	avs::vec3 position;
	float range;
	avs::vec3 direction;
	uint uid32;
	float is_spot;
	float is_point;
	float shadow_strength;
	float radius;
};

struct __attribute__ ((packed)) VideoTagData2D
{
    avs::vec3 cameraPosition;
	int lightCount;
    avs::vec4 cameraRotation;
	LightTag lightTags[4];
};

struct __attribute__ ((packed)) VideoTagDataCube
{
    avs::vec3 cameraPosition;
	int lightCount;
    avs::vec4 cameraRotation;
	avs::vec4 ambientMultipliers;
	// Some light information
	LightTag lightTags[4];
};

namespace teleport
{
	namespace client
	{
		class ClientDeviceState;
	}
}
class ClientRenderer:public clientrender::Renderer
{
public:
	ClientRenderer(ClientAppInterface *c,teleport::client::ClientDeviceState *s,Controllers *cn);
	~ClientRenderer();

	void CycleShaderMode();
	void WriteDebugOutput();
	void ToggleWebcam();
	void CycleOSD();
	void CycleOSDSelection();
	void  SetStickOffset(float,float);

	void EnteredVR(const ovrJava *java);
	void ExitedVR();
	void OnSetupCommandReceived(const avs::VideoConfig &vc);
	void OnReceiveVideoTagData(const uint8_t* data, size_t dataSize);
	void CopyToCubemaps(scc::GL_DeviceContext &mDeviceContext);
    void RenderVideo(scc::GL_DeviceContext &mDeviceContext,OVRFW::ovrRendererOutput &res);

	void RenderLocalNodes(OVRFW::ovrRendererOutput& res);
	void RenderNode(OVRFW::ovrRendererOutput& res, std::shared_ptr<clientrender::Node> node);

	void SetWebcamPosition(const avs::vec2& position);
	void RenderWebcam(OVRFW::ovrRendererOutput& res);

	void SetMinimumPriority(int32_t p)
	{
		minimumPriority=p;
	}

	int32_t GetMinimumPriority() const
	{
		return minimumPriority;
	}
	Controllers *controllers=nullptr;

	//avs::Decoder mDecoder;
	//avs::TagDataDecoder mTagDataDecoder;

	//avs::Queue mVideoQueue;
	//avs::Queue mTagDataQueue;
	//avs::Queue mAudioQueue;

	clientrender::GeometryCache geometryCache;
	clientrender::ResourceCreator resourceCreator;
	teleport::client::ClientPipeline clientPipeline;
	ClientAppInterface		*clientAppInterface	=nullptr;
	float eyeSeparation=0.06f;
	struct VideoUB
	{
		avs::vec4 eyeOffsets[2];
		ovrMatrix4f invViewProj[2];
		ovrMatrix4f viewProj;
		ovrMatrix4f serverProj;
		avs::vec3 cameraPosition;
		int pad_;
		avs::vec4 cameraRotation;
	};
	VideoUB videoUB;

	struct CubemapUB
	{
		clientrender::uvec2 dimensions;
		clientrender::ivec2 sourceOffset;
		uint32_t   faceSize;
		uint32_t    mip = 0;
		uint32_t    face = 0;
	};
	CubemapUB cubemapUB;

	struct WebcamUB
	{
		clientrender::uvec2 sourceTexSize;
		clientrender::ivec2 sourceOffset;
		clientrender::uvec2 camTexSize;
        clientrender::uvec2 pad = {0,0};
	};

	struct WebcamResources
	{
		OVRFW::GlProgram program;
		OVRFW::ovrSurfaceDef surfaceDef;
		std::shared_ptr<clientrender::VertexBuffer> vertexBuffer;
		std::shared_ptr<clientrender::IndexBuffer> indexBuffer;
		WebcamUB webcamUBData;
		std::shared_ptr<clientrender::UniformBuffer> webcamUB;
		ovrMatrix4f transform;
		bool initialized = false;

		void Init(ClientAppInterface* clientAppInterface,const char *shader_name);
		void SetPosition(const avs::vec2& position);
		void Destroy();
	};
	WebcamResources mWebcamResources;
	WebcamResources mDebugTextureResources;

	OVRFW::ovrSurfaceDef mVideoSurfaceDef;
	OVRFW::GlProgram     mCubeVideoSurfaceProgram;
	OVRFW::GlProgram     m2DVideoSurfaceProgram;
	OVRFW::SurfaceTexture* mVideoSurfaceTexture = nullptr;
	OVRFW::SurfaceTexture* mAlphaSurfaceTexture = nullptr;
	std::shared_ptr<clientrender::Texture>       mVideoTexture;
	std::shared_ptr<clientrender::Texture>       mAlphaVideoTexture;
	std::shared_ptr<clientrender::Texture>       mRenderTexture;
	std::shared_ptr<clientrender::Texture>       diffuseCubemapTexture;
	std::shared_ptr<clientrender::Texture>       specularCubemapTexture;
	std::shared_ptr<clientrender::Texture>       mCubemapLightingTexture;
	std::shared_ptr<clientrender::UniformBuffer> mCubemapUB;
	std::shared_ptr<clientrender::UniformBuffer> mVideoUB;
	std::shared_ptr<clientrender::ShaderStorageBuffer> mTagDataIDBuffer;
	std::shared_ptr<clientrender::ShaderStorageBuffer> mTagDataArrayBuffer;
	clientrender::ShaderResource				    mColourAndDepthShaderResources;
	clientrender::ShaderResource				    mCopyCubemapShaderResources;
	clientrender::ShaderResource				    mCopyPerspectiveShaderResources;
	clientrender::ShaderResource				    mExtractTagShaderResources;
	std::shared_ptr<clientrender::Effect>        mCopyCubemapEffect;
	std::shared_ptr<clientrender::Effect>        mCopyCubemapWithDepthEffect;
	std::shared_ptr<clientrender::Effect>        mCopyCubemapWithAlphaLayerEffect;
	std::shared_ptr<clientrender::Effect>        mCopyPerspectiveEffect;
	std::shared_ptr<clientrender::Effect>        mCopyPerspectiveWithDepthEffect;
	std::shared_ptr<clientrender::Effect>        mExtractTagDataIDEffect;
	std::shared_ptr<clientrender::Effect>        mExtractOneTagEffect;

	std::vector<clientrender::SceneCaptureCubeTagData> videoTagDataCubeArray;

	std::vector<std::string> passNames;
	std::vector<std::string> debugPassNames;
	int passSelector=0;

	static constexpr int MAX_TAG_DATA_COUNT = 32;

	static constexpr float WEBCAM_WIDTH = 0.2f;
	static constexpr float WEBCAM_HEIGHT = 0.2f;

	clientrender::uvec4 mTagDataID;

	std::string                         CopyCubemapSrc;
	std::string                         ExtractTagDataIDSrc;
	uint32_t osd_selection;
	void DrawOSD(OVRFW::ovrRendererOutput& res);
protected:
	void ListNode(const std::shared_ptr<clientrender::Node>& node, int indent, size_t& linesRemaining);
	teleport::client::ClientDeviceState *clientDeviceState=nullptr;
	void UpdateTagDataBuffers();
	static constexpr float INFO_TEXT_DURATION = 0.017f;
	static constexpr size_t MAX_RESOURCES_PER_LINE = 3;

private:
	void InitWebcamResources();
	bool mShowWebcam;
};
