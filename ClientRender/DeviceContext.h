// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once
#include "Common.h"

#include "ClientRender/FrameBuffer.h"
#include "ClientRender/ShaderStorageBuffer.h"
#include "ClientRender/Camera.h"
#include "ClientRender/Node.h"

namespace clientrender
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
		const char*					effectPassName;

        virtual ~InputCommand() = default;
	};
	typedef InputCommand InputCommandCreateInfo;

	struct InputCommand_Mesh_Material_Transform : public InputCommand
	{
		std::shared_ptr<clientrender::VertexBuffer>			pVertexBuffer;
		std::shared_ptr<clientrender::IndexBuffer>			pIndexBuffer;
		std::shared_ptr<Material>					pMaterial;
		const Transform								&pTransform;

		InputCommand_Mesh_Material_Transform(InputCommandCreateInfo* pInputCommandCreateInfo, const Transform	&transform
		,std::shared_ptr<clientrender::VertexBuffer> vb,std::shared_ptr<clientrender::IndexBuffer> ib,std::shared_ptr<Material> m)
			:pTransform(transform)
		{
			type = INPUT_COMMAND_MESH_MATERIAL_TRANSFORM;
			pFBs = pInputCommandCreateInfo->pFBs;
			frameBufferCount = pInputCommandCreateInfo->frameBufferCount;
			pCamera = pInputCommandCreateInfo->pCamera;
			effectPassName = pInputCommandCreateInfo->effectPassName;

			pVertexBuffer=vb;
			pIndexBuffer=ib;
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
		const ShaderResource							*m_ShaderResources;

		InputCommand_Compute(InputCommandCreateInfo* pInputCommandCreateInfo,
			const uvec3& workGroupSize,
			const std::shared_ptr<Effect>& computeEffect,
			const ShaderResource& shaderResources)
		{
			type = INPUT_COMMAND_COMPUTE;
			pFBs = pInputCommandCreateInfo->pFBs;
			frameBufferCount = pInputCommandCreateInfo->frameBufferCount;
			pCamera = pInputCommandCreateInfo->pCamera;
			effectPassName = pInputCommandCreateInfo->effectPassName;

			m_WorkGroupSize = workGroupSize;
			m_pComputeEffect = computeEffect;
			m_ShaderResources = &shaderResources;
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
		DeviceContext(const RenderPlatform*const r)
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