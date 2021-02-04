#pragma once

#include "SurfaceRender.h"

#include "Node.h"

class OVRNode : public scr::Node
{
public:
	OVRNode(avs::uid id, const std::string& name)
		:Node(id, name)
	{}

	virtual ~OVRNode() = default;

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