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
		MESH_MATERIAL_TRANSFORM_INPUT_COMMAND
	};

	struct InputCommand
	{
		InputCommandStructureType	type;
		FrameBuffer*				pFBs; 
		uint32_t					frameBufferCount;
		Camera*						pCamera;
	};
	typedef InputCommand InputCommandCreateInfo;

	struct Mesh_Material_Transform_InputCommand : public InputCommand
	{
		const Mesh*			pMesh;
		const Material*		pMaterial;
		const Transform*	pTransform;

		Mesh_Material_Transform_InputCommand(InputCommandCreateInfo* pInputCommandCreateInfo, const Actor* pActor)
		{
			type = MESH_MATERIAL_TRANSFORM_INPUT_COMMAND;
			pFBs = pInputCommandCreateInfo->pFBs;
			frameBufferCount = pInputCommandCreateInfo->frameBufferCount;
			pCamera = pInputCommandCreateInfo->pCamera;

			pMesh = pActor->GetMesh();
			pMaterial = pActor->GetMaterial();
			pTransform = pActor->GetTransform();
		};
	};

	//struct MultiMesh_Material_TransformInputCommand : public InputCommand {};
	//struct Mesh_MultiMaterial_TransformInputCommand : public InputCommand {};
	//struct Mesh_Material_MultiTransformInputCommand : public InputCommand {};

	class DeviceContext
	{
	public:
		struct DeviceContextCreateInfo
		{
			void* pNativeDeviceHandle;
		};

	protected:
		DeviceContextCreateInfo m_CI;

	public:
		DeviceContext(RenderPlatform *r) :APIObject(r) {}
		virtual ~DeviceContext() = default;

		virtual void Create(DeviceContextCreateInfo* pDeviceContextCreateInfo) = 0;

		virtual void Draw(InputCommand*) = 0;
		virtual void DispatchCompute(InputCommand*) = 0;

		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;

		inline const DeviceContextCreateInfo& GetDeviceContextCreateInfo() const { return m_CI; }
	};
}