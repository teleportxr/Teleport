//
// (C) Copyright 2018 Simul
#pragma once
#include "App.h"
#include "basic_linear_algebra.h"
#include "ResourceCreator.h"
#include "crossplatform/SessionClient.h"
#include "GlobalGraphicsResources.h"
#include <SurfaceTexture.h>
#include <GuiSys.h>
#include <libavstream/decoder.hpp>
#include <libavstream/libavstream.hpp>
#include <OVR_Input.h>

class ClientAppInterface
{
public:
	virtual std::string LoadTextFile(const char *filename)=0;
	virtual const scr::Effect::EffectPassCreateInfo* BuildEffectPass(const char* effectPassName, scr::VertexBufferLayout* vbl, const scr::ShaderSystem::PipelineCreateInfo*, const std::vector<scr::ShaderResource>& shaderResources)=0;

};

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

struct VideoTagData2D
{
    avs::vec3 cameraPosition;
	float pad;
    avs::vec4 cameraRotation;
};

struct VideoTagDataCube
{
    avs::vec3 cameraPosition;
	float pad;
    avs::vec4 cameraRotation;
	// Some light information
};

class ClientRenderer
{
public:
	ClientRenderer(ResourceCreator *r,scr::ResourceManagers *rm,SessionCommandInterface *i,ClientAppInterface *c);
	~ClientRenderer();

	void ToggleTextures();
	void ToggleShowInfo();
	void  SetStickOffset(float,float);

	void EnteredVR(struct ovrMobile *ovrMobile,const ovrJava *java);
	void ExitedVR();
	void Render(const OVR::ovrFrameInput& vrFrame,OVR::OvrGuiSys *mGuiSys);
	void CopyToCubemaps(scc::GL_DeviceContext &mDeviceContext);
	void UpdateHandObjects();

	void RenderLocalActors(OVR::ovrFrameResult& res);
	void RenderActor(OVR::ovrFrameResult& res, std::shared_ptr<scr::Node> actor);

	avs::Decoder       mDecoder;
	avs::NetworkSource mNetworkSource;

	avs::vec3 oculusOrigin;		// in metres. The headPose will be relative to this.
	scr::mat4 transformToOculusOrigin; // Because we're using OVR's rendering, we must position the actors relative to the oculus origin.

	scr::ResourceManagers	*resourceManagers	=nullptr;
	ResourceCreator			*resourceCreator	=nullptr;
	ClientAppInterface		*clientAppInterface	=nullptr;
	ovrMobile				*mOvrMobile			=nullptr;
	avs::Pose headPose;
	avs::Pose controllerPoses[2];
	avs::vec3 cameraPosition;	// in real space.
	avs::VideoConfig videoConfig;
	const scr::quat HAND_ROTATION_DIFFERENCE {0.0000000456194194, 0.923879385, -0.382683367, 0.000000110135019}; //Adjustment to the controller's rotation to get the desired rotation.

	struct VideoUB
	{
		avs::vec4 eyeOffsets[2];
		ovrMatrix4f invViewProj[2];
		avs::vec3 cameraPosition;
		int pad_;
	};
	VideoUB videoUB;

	struct CubemapUB
	{
		scr::ivec2 sourceOffset;
		uint32_t   faceSize;
		uint32_t    mip = 0;
		uint32_t    face = 0;
	};
	CubemapUB cubemapUB;

	OVR::ovrSurfaceDef mVideoSurfaceDef;
	OVR::GlProgram     mVideoSurfaceProgram;
	OVR::SurfaceTexture *mVideoSurfaceTexture=nullptr;
	std::shared_ptr<scr::Texture>       mVideoTexture;
	std::shared_ptr<scr::Texture>       mCubemapTexture;
	std::shared_ptr<scr::Texture>       mDiffuseTexture;
	std::shared_ptr<scr::Texture>       mSpecularTexture;
	std::shared_ptr<scr::Texture>       mRoughSpecularTexture;
	std::shared_ptr<scr::Texture>       mCubemapLightingTexture;
	std::shared_ptr<scr::UniformBuffer> mCubemapUB;
	std::shared_ptr<scr::UniformBuffer> mVideoUB;
	std::shared_ptr<scr::ShaderStorageBuffer> mTagDataIDBuffer;
	std::shared_ptr<scr::ShaderStorageBuffer> mTagDataBuffer;
	std::vector<scr::ShaderResource>    mCubemapComputeShaderResources;
	std::shared_ptr<scr::Effect>        mCopyCubemapEffect;
	std::shared_ptr<scr::Effect>        mCopyCubemapWithDepthEffect;
	std::shared_ptr<scr::Effect>        mExtractTagDataIDEffect;

	std::vector<scr::SceneCapture2DTagData> mVideoTagData2DArray;
	std::vector<scr::SceneCaptureCubeTagData> mVideoTagDataCubeArray;

	std::vector<std::string> passNames;
	int passSelector=0;
	GlobalGraphicsResources& GlobalGraphicsResources = GlobalGraphicsResources::GetInstance();

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
		CONTROLLER_OSD,
		NUM_OSDS
	};
	int show_osd = GEOMETRY_OSD;
	bool mIsCubemapVideo = true;
	void DrawOSD(OVR::OvrGuiSys *mGuiSys);
};
