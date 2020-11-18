#pragma once

#include "SurfaceRender.h"

#include "libavstream/common.hpp"

#include "ActorManager.h"
#include "GlobalGraphicsResources.h"

class OVRActor : public scr::Node
{
public:
	OVRActor(avs::uid id, const std::string& name)
		:Node(id, name)
	{}

	virtual ~OVRActor() = default;

	std::vector<OVR::ovrSurfaceDef> ovrSurfaceDefs;

	virtual void SetMesh(std::shared_ptr<scr::Mesh> mesh) override;
	virtual void SetSkin(std::shared_ptr<scr::Skin> skin) override;
	virtual void SetMaterial(size_t index, std::shared_ptr<scr::Material> material) override;
	virtual void SetMaterialListSize(size_t size) override;
	virtual void SetMaterialList(std::vector<std::shared_ptr<scr::Material>>& materials) override;

	std::string GetCompleteEffectPassName(const char* effectPassName);
	OVR::GlProgram* GetEffectPass(const char* effectPassName);
	void ChangeEffectPass(const char* effectPassName);
private:
	OVR::ovrSurfaceDef CreateOVRSurface(size_t materialIndex, std::shared_ptr<scr::Material> material);

	//Recreates all OVR surfaces from scratch.
	void RefreshOVRSurfaces();
};

class OVRActorManager : public scr::ActorManager
{
public:

    virtual ~OVRActorManager() = default;

    virtual std::shared_ptr<scr::Node> CreateActor(avs::uid id, const std::string& name) const override;

	virtual void AddActor(std::shared_ptr<scr::Node> actor, bool isHand) override;

	//Changes PBR effect used on actors/surfaces to the effect pass with the passed name.
	//Also changes GlobalGraphicsResource::effectPassName.
    void ChangeEffectPass(const char* effectPassName);
};