// (C) Copyright 2018 Simul.co

#pragma once

#include <VrApi_Types.h>
#include <VrApi_Input.h>

#include <libavstream/libavstream.hpp>

#include "App.h"
#include "SceneView.h"
#include "SoundEffectContext.h"
#include "GuiSys.h"

#include "SessionClient.h"
#include "VideoDecoderProxy.h"

#include "ResourceCreator.h"
#include "GeometryDecoder.h"
#include <libavstream/geometrydecoder.hpp>
#include "SCR_Class_GL_Impl/GL_RenderPlatform.h"

namespace OVR {
	class ovrLocale;
}

namespace scr {
	class Texture;
}
class Application : public OVR::VrAppInterface, public SessionCommandInterface, public DecodeEventInterface
{
public:
	Application();

	virtual    ~Application();

	virtual void Configure(OVR::ovrSettings &settings) override;

	virtual void EnteredVrMode(
			const OVR::ovrIntentType intentType, const char *intentFromPackage,
			const char *intentJSON, const char *intentURI) override;

	virtual void LeavingVrMode() override;

	virtual bool
	OnKeyEvent(const int keyCode, const int repeatCount, const OVR::KeyEventType eventType);

	virtual OVR::ovrFrameResult Frame(const OVR::ovrFrameInput &vrFrame) override;

	class OVR::ovrLocale &GetLocale()
	{
		return *mLocale;
	}

	bool InitializeController();

	/* Begin SessionCommandInterface */
	virtual void OnVideoStreamChanged(const avs::SetupCommand &setupCommand,avs::Handshake &handshake) override;

	virtual void OnVideoStreamClosed() override;

	virtual bool OnActorEnteredBounds(avs::uid actor_uid) override;

	virtual bool OnActorLeftBounds(avs::uid actor_uid) override;
	/* End SessionCommandInterface */

	/* Begin DecodeEventInterface */
	virtual void OnFrameAvailable() override;
	/* End DecodeEventInterface */

private:
	struct CubemapUB
	{
		scr::ivec2 sourceOffset;
		uint32_t   faceSize;
		uint32_t    mip = 0;
		uint32_t    face = 0;
	};
	CubemapUB cubemapUB;
	CubemapUB cubemapUB2;

	static void avsMessageHandler(avs::LogSeverity severity, const char *msg, void *);

	avs::Context  mContext;
	avs::Pipeline mPipeline;

	avs::Decoder       mDecoder;
	avs::Surface       mSurface;
	avs::NetworkSource mNetworkSource;
	bool               mPipelineConfigured;

	static constexpr size_t NumStreams = 1;
	static constexpr bool   GeoStream  = true;

	scc::GL_RenderPlatform renderPlatform;
	GeometryDecoder        geometryDecoder;
	ResourceCreator        resourceCreator;
	avs::GeometryDecoder   avsGeometryDecoder;
	avs::GeometryTarget    avsGeometryTarget;

	struct RenderConstants
	{
		avs::vec4 colourOffsetScale;
		avs::vec4 depthOffsetScale;
	};
	RenderConstants        renderConstants;

	OVR::ovrSoundEffectContext        *mSoundEffectContext;
	OVR::OvrGuiSys::SoundEffectPlayer *mSoundEffectPlayer;

	OVR::OvrGuiSys *mGuiSys;
	OVR::ovrLocale *mLocale;

	OVR::OvrSceneView mScene;

	OVR::ovrSurfaceDef mVideoSurfaceDef;
	OVR::GlProgram     mVideoSurfaceProgram;
	OVR::SurfaceTexture *mVideoSurfaceTexture;
	std::shared_ptr<scr::Texture>       mVideoTexture;
	std::shared_ptr<scr::Texture>       mCubemapTexture;
	std::shared_ptr<scr::Texture>       mDiffuseTexture;
	std::shared_ptr<scr::Texture>       mSpecularTexture;
	std::shared_ptr<scr::Texture>       mRoughSpecularTexture;
	std::shared_ptr<scr::Texture>       mCubemapLightingTexture;
	std::shared_ptr<scr::UniformBuffer> mCubemapUB;
	std::shared_ptr<scr::UniformBuffer> mCubemapUB2;
	std::vector<scr::ShaderResource>    mCubemapComputeShaderResources;
	scr::ShaderResource                 mLightCubemapShaderResources;
	std::shared_ptr<scr::Effect>        mCopyCubemapEffect;
	std::string                         CopyCubemapSrc;
	ovrMobile                           *mOvrMobile;
	SessionClient                       mSession;

	std::vector<float> mRefreshRates;

	ovrDeviceID mControllerID;
	//int		 mControllerIndex;
	ovrVector2f mTrackpadDim;

	int mNumPendingFrames                  = 0;

	scr::ResourceManagers resourceManagers;

	//Clientside Renderering Objects
	scc::GL_DeviceContext mDeviceContext;
	GLint maxFragTextureSlots = 0, maxFragUniformBlocks = 0;

	scr::vec3                    capturePosition;
	std::shared_ptr<scr::Camera> scrCamera;

	scc::GL_Effect                mEffect;
	std::shared_ptr<scr::Sampler> mSampler = renderPlatform.InstantiateSampler();
	struct OVRActor
	{
		std::vector<std::shared_ptr<OVR::ovrSurfaceDef>> ovrSurfaceDefs;
	};
	std::map<avs::uid, std::shared_ptr<OVRActor>> mOVRActors;
	inline void RemoveInvalidOVRActors()
	{
		for(std::map<avs::uid, std::shared_ptr<OVRActor>>::iterator it = mOVRActors.begin(); it != mOVRActors.end(); it++)
		{
			if(resourceManagers.mActorManager.GetActorList().find(it->first) == resourceManagers.mActorManager.GetActorList().end())
			{
				mOVRActors.erase(it);
			}
		}
	}
	inline void ClearOVRActors()
    {
	    for(auto it = mOVRActors.begin(); it != mOVRActors.end(); it++)
        {
	        for(std::shared_ptr<OVR::ovrSurfaceDef> ovrSurfaceDef : it->second->ovrSurfaceDefs)
            {
	            ovrSurfaceDef->geo.Free();
	            OVR::GlProgram::Free(ovrSurfaceDef->graphicsCommand.Program);
            }
	        it->second->ovrSurfaceDefs.clear();
        }
        mOVRActors.clear();
    }
	void CopyToCubemaps();
	void RenderLocalActors(OVR::ovrFrameResult& res);
    const scr::Effect::EffectPassCreateInfo& BuildEffectPass(const char* effectPassName, scr::VertexBufferLayout* vbl, const scr::ShaderSystem::PipelineCreateInfo*, const std::vector<scr::ShaderResource>& shaderResources);
	std::string LoadTextFile(const char *filename);
};
