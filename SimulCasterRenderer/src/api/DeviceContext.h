// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once
#include "Common.h"

#include "api/FrameBuffer.h"
#include "api/ShaderStorageBuffer.h"
#include "crossplatform/Camera.h"
#include "crossplatform/Actor.h"

namespace scr
{
	enum InputCommandStructureType : uint32_t
	{
		INPUT_COMMAND,
		INPUT_COMMAND_MESH_MATERIAL_TRANSFORM,
		INPUT_COMMAND_COMPUTE
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
		std::shared_ptr<scr::VertexBuffer>			pVertexBuffer;
		std::shared_ptr<scr::IndexBuffer>			pIndexBuffer;
		std::shared_ptr<Material>					pMaterial;
		const Transform								&pTransform;

		InputCommand_Mesh_Material_Transform(InputCommandCreateInfo* pInputCommandCreateInfo, const Transform	&transform
		,std::shared_ptr<scr::VertexBuffer> vb,std::shared_ptr<scr::IndexBuffer> ib,std::shared_ptr<Material> m)
			:pTransform(transform)
		{
			type = INPUT_COMMAND_MESH_MATERIAL_TRANSFORM;
			pFBs = pInputCommandCreateInfo->pFBs;
			frameBufferCount = pInputCommandCreateInfo->frameBufferCount;
			pCamera = pInputCommandCreateInfo->pCamera;
			pIndexBuffer=ib;

			pVertexBuffer=vb;
			pMaterial = m;
		};
	};
	//struct MultiMesh_Material_TransformInputCommand : public InputCommand {};
	//struct Mesh_MultiMaterial_TransformInputCommand : public InputCommand {};
	//struct Mesh_Material_MultiTransformInputCommand : public InputCommand {};

	struct InputCommand_Compute : public InputCommand
	{
		uvec3												m_WorkGroupSize;
		std::shared_ptr<Effect>								m_pComputeEffect;
		std::vector<ShaderResource>							m_ShaderResources;

		InputCommand_Compute(InputCommandCreateInfo* pInputCommandCreateInfo,
			const uvec3& workGroupSize,
			const std::shared_ptr<Effect>& computeEffect,
			const std::vector<ShaderResource>& shaderResources)
		{
			type = INPUT_COMMAND_COMPUTE;
			pFBs = pInputCommandCreateInfo->pFBs;
			frameBufferCount = pInputCommandCreateInfo->frameBufferCount;
			pCamera = pInputCommandCreateInfo->pCamera;

			m_WorkGroupSize = workGroupSize;
			m_pComputeEffect = computeEffect;
			m_ShaderResources = shaderResources;
		};
	};


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