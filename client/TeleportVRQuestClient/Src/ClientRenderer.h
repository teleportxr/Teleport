//
// (C) Copyright 2018 Simul
#pragma once
#include "basic_linear_algebra.h"
#include "crossplatform/ResourceCreator.h"
#include "GlobalGraphicsResources.h"
#include <Render/SurfaceTexture.h>
#include <libavstream/decoder.hpp>
#include <libavstream/libavstream.hpp>
#include <VrApi_Input.h>
#include <FrameParams.h>

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
	// Some light information
	LightTag lightTags[4];
};

class ClientDeviceState;

class ClientRenderer
{
public:
	ClientRenderer(ResourceCreator *r,scr::ResourceManagers *rm,ClientAppInterface *c,ClientDeviceState *s,Controllers *cn);
	~ClientRenderer();

	void CycleShaderMode();
	void WriteDebugOutput();
	void CycleOSD();
	void  SetStickOffset(float,float);

	void EnteredVR(const ovrJava *java);
	void ExitedVR();
	void OnVideoStreamChanged(const avs::VideoConfig &vc);
	void OnReceiveVideoTagData(const uint8_t* data, size_t dataSize);
	void CopyToCubemaps(scc::GL_DeviceContext &mDeviceContext);
    void RenderVideo(scc::GL_DeviceContext &mDeviceContext,OVRFW::ovrRendererOutput &res);

	void RenderLocalNodes(OVRFW::ovrRendererOutput& res);
	void RenderNode(OVRFW::ovrRendererOutput& res, std::shared_ptr<scr::Node> node);

	Controllers *controllers=nullptr;

	avs::Decoder       mDecoder;
	avs::NetworkSource mNetworkSource;
	avs::Queue mVideoQueue;
	avs::Queue mAudioQueue;
	avs::Queue mGeometryQueue;

	scr::ResourceManagers	*resourceManagers	=nullptr;
	ResourceCreator			*resourceCreator	=nullptr;
	ClientAppInterface		*clientAppInterface	=nullptr;
	float eyeSeparation=0.06f;
	avs::VideoConfig videoConfig;
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
		scr::uvec2 dimensions;
		scr::ivec2 sourceOffset;
		uint32_t   faceSize;
		uint32_t    mip = 0;
		uint32_t    face = 0;
	};
	CubemapUB cubemapUB;

	OVRFW::ovrSurfaceDef mVideoSurfaceDef;
	OVRFW::GlProgram     mCubeVideoSurfaceProgram;
	OVRFW::GlProgram     m2DVideoSurfaceProgram;
	OVRFW::SurfaceTexture* mVideoSurfaceTexture = nullptr;
	std::shared_ptr<scr::Texture>       mVideoTexture;
	std::shared_ptr<scr::Texture>       mRenderTexture;
	std::shared_ptr<scr::Texture>       diffuseCubemapTexture;
	std::shared_ptr<scr::Texture>       specularCubemapTexture;
	std::shared_ptr<scr::Texture>       mCubemapLightingTexture;
	std::shared_ptr<scr::UniformBuffer> mCubemapUB;
	std::shared_ptr<scr::UniformBuffer> mVideoUB;
	std::shared_ptr<scr::ShaderStorageBuffer> mTagDataIDBuffer;
	std::shared_ptr<scr::ShaderStorageBuffer> mTagDataArrayBuffer;
	scr::ShaderResource				    mColourAndDepthShaderResources;
	scr::ShaderResource				    mCopyCubemapShaderResources;
	scr::ShaderResource				    mCopyPerspectiveShaderResources;
	scr::ShaderResource				    mExtractTagShaderResources;
	std::shared_ptr<scr::Effect>        mCopyCubemapEffect;
	std::shared_ptr<scr::Effect>        mCopyCubemapWithDepthEffect;
	std::shared_ptr<scr::Effect>        mCopyPerspectiveEffect;
	std::shared_ptr<scr::Effect>        mExtractTagDataIDEffect;
	std::shared_ptr<scr::Effect>        mExtractOneTagEffect;

	std::vector<scr::SceneCaptureCubeTagData> videoTagDataCubeArray;

	std::vector<std::string> passNames;
	int passSelector=0;

	static constexpr int MAX_TAG_DATA_COUNT = 32;

	scr::uvec4 mTagDataID;

	std::string                         CopyCubemapSrc;
	std::string                         ExtractTagDataIDSrc;
	enum
	{
		NO_OSD,
		CAMERA_OSD,
		NETWORK_OSD,
		GEOMETRY_OSD,
		TAG_OSD,
		CONTROLLER_OSD,
		NUM_OSDS
	};
	int show_osd = CAMERA_OSD;
	bool mIsCubemapVideo = true;
	void DrawOSD();
	avs::SetupCommand lastSetupCommand;
protected:
	void ListNode(const std::shared_ptr<scr::Node>& node, int indent, size_t& linesRemaining);
	ClientDeviceState *clientDeviceState=nullptr;
	void UpdateTagDataBuffers();
	static constexpr float INFO_TEXT_DURATION = 0.017f;
	static constexpr size_t MAX_RESOURCES_PER_LINE = 3;
};
