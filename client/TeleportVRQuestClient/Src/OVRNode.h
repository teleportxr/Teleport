#pragma once

#include <Render/SurfaceRender.h>

#include "crossplatform/Node.h"
#include "GlobalGraphicsResources.h"

class OVRNode : public scr::Node
{
public:
	struct SurfaceInfo
	{
	public:
		OVRFW::ovrSurfaceDef surfaceDef;

		void SetPrograms(OVRFW::GlProgram* newProgram, OVRFW::GlProgram* newHighlightProgram,bool setDefaults);

		void SetHighlighted(bool highLighted);

		operator OVRFW::ovrSurfaceDef&()
		{
			return surfaceDef;
		}

		operator const OVRFW::ovrSurfaceDef&() const
		{
			return surfaceDef;
		}
		PerMeshInstanceData perMeshInstanceData;
		std::shared_ptr<scr::UniformBuffer> s_perMeshInstanceUniformBuffer;
		scr::ShaderResource perMeshInstanceShaderResource;
		bool initialized=false;
	private:
		OVRFW::GlProgram* program = nullptr;
		OVRFW::GlProgram* highlightProgram = nullptr;
		bool highlighted=false;
	};

	OVRNode(avs::uid id, const std::string& name);

	virtual ~OVRNode() = default;

	virtual void SetMesh(std::shared_ptr<scr::Mesh> mesh) override;
	virtual void SetSkin(std::shared_ptr<scr::Skin> skin) override;
	virtual void SetMaterial(size_t index, std::shared_ptr<scr::Material> material) override;
	virtual void SetMaterialListSize(size_t size) override;
	virtual void SetMaterialList(std::vector<std::shared_ptr<scr::Material>>& materials) override;

	virtual void SetHighlighted(bool highlighted) override;

	std::vector<SurfaceInfo>& GetSurfaces()
	{
		return surfaceDefinitions;
	}

	const std::vector<SurfaceInfo>& GetSurfaces() const
	{
		return surfaceDefinitions;
	}

	std::string GetCompleteEffectPassName(const char* effectPassName);
	void ChangeEffectPass(const char* effectPassName);

private:
	std::shared_ptr<scr::Texture>		mlightmapTexture;
	std::vector<SurfaceInfo> surfaceDefinitions;

	SurfaceInfo CreateOVRSurface(size_t materialIndex, std::shared_ptr<scr::Material> material);
	OVRFW::GlProgram* GetEffectPass(const char* effectPassName);

	//Recreates all OVR surfaces from scratch.
	void RefreshOVRSurfaces();
};