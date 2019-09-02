// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once
#include "Common.h"

#include "api/FrameBuffer.h"
#include "crossplatform/Camera.h"
#include "crossplatform/Actor.h"

namespace scr
{
	enum InputCommandStructureType : uint32_t
	{
		INPUT_COMMAND,
		INPUT_COMMAND_MESH_MATERIAL_TRANSFORM
	};

	struct InputCommand
	{
		InputCommandStructureType	type;
		FrameBuffer*				pFBs; 
		uint32_t					frameBufferCount;
		Camera*						pCamera;

        virtual ~InputCommand() = default;
	};
	typedef InputCommand InputCommandCreateInfo;

	struct InputCommand_Mesh_Material_Transform : public InputCommand
	{
		std::shared_ptr<Mesh>			pMesh;
		std::shared_ptr<Material>		pMaterial;
		std::shared_ptr<Transform>		pTransform;

		InputCommand_Mesh_Material_Transform(InputCommandCreateInfo* pInputCommandCreateInfo, Actor* pActor)
		{
			type = INPUT_COMMAND_MESH_MATERIAL_TRANSFORM;
			pFBs = pInputCommandCreateInfo->pFBs;
			frameBufferCount = pInputCommandCreateInfo->frameBufferCount;
			pCamera = pInputCommandCreateInfo->pCamera;

			pMesh = pActor->GetMesh();
			pMaterial = pActor->GetMaterials()[0];
			pTransform = pActor->GetTransform();
		};
	};

	//struct MultiMesh_Material_TransformInputCommand : public InputCommand {};
	//struct Mesh_MultiMaterial_TransformInputCommand : public InputCommand {};
	//struct Mesh_Material_MultiTransformInputCommand : public InputCommand {};

	class DeviceContext : public APIObject
	{
	public:
		struct DeviceContextCreateInfo
		{
			void* pNativeDeviceHandle;
		};

	protected:
		DeviceContextCreateInfo m_CI;

	public:
		DeviceContext(RenderPlatform *r) 
			: APIObject(r) {}
		virtual ~DeviceContext() = default;

		virtual void Create(DeviceContextCreateInfo* pDeviceContextCreateInfo) = 0;

		virtual void Draw(InputCommand* pInputCommand) = 0;
		virtual void DispatchCompute(InputCommand* pInputCommand) = 0;

		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;

		inline const DeviceContextCreateInfo& GetDeviceContextCreateInfo() const { return m_CI; }
	};
}