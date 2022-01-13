// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_VertexBuffer.h"
#include "PC_RenderPlatform.h"
#include "Platform/CrossPlatform/Buffer.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include <libavstream/geometry/mesh_interface.hpp>

using namespace pc_client;
using namespace clientrender;

const char * GetAttributeSemantic(avs::AttributeSemantic sem)
{
	switch (sem)
	{
	case avs::AttributeSemantic::POSITION:
		return "POSITION";
		break;
	case avs::AttributeSemantic::NORMAL:
		return "NORMAL";
		break;
	case avs::AttributeSemantic::TANGENT:
		return "TANGENT";
		break;
	case avs::AttributeSemantic::TEXCOORD_0:
		return "TEXCOORD";
		break;
	case avs::AttributeSemantic::TEXCOORD_1:
		return "TEXCOORD";
		break;
	case avs::AttributeSemantic::COLOR_0:
		return "COLOR_0";
		break;
	case avs::AttributeSemantic::JOINTS_0:
		return "TEXCOORD";
		break;
	case avs::AttributeSemantic::WEIGHTS_0:
		return "TEXCOORD";
		break;
	case avs::AttributeSemantic::COUNT:
		return "COUNT";
		break;
	default:
		return "";
		break;
	}
}

int GetAttributeSemanticIndex(avs::AttributeSemantic sem)
{
	switch (sem)
	{
	case avs::AttributeSemantic::POSITION:
		return 0;
		break;
	case avs::AttributeSemantic::NORMAL:
		return 0;
		break;
	case avs::AttributeSemantic::TANGENT:
		return 0;
		break;
	case avs::AttributeSemantic::TEXCOORD_0:
		return 0;
		break;
	case avs::AttributeSemantic::TEXCOORD_1:
		return 1;
		break;
	case avs::AttributeSemantic::COLOR_0:
		return 0;
		break;
	case avs::AttributeSemantic::JOINTS_0:
		return 2;
		break;
	case avs::AttributeSemantic::WEIGHTS_0:
		return 3;
		break;
	default:
		return 0;
		break;
	}
}

simul::crossplatform::PixelFormat GetAttributeFormat(const clientrender::VertexBufferLayout::VertexAttribute& attr)
{
	switch(attr.type)
	{
	case clientrender::VertexBufferLayout::Type::FLOAT:
		switch(attr.componentCount)
		{
		case clientrender::VertexBufferLayout::ComponentCount::SCALAR:
			return simul::crossplatform::PixelFormat::R_32_FLOAT;
		case clientrender::VertexBufferLayout::ComponentCount::VEC2:
			return simul::crossplatform::PixelFormat::RG_32_FLOAT;
		case clientrender::VertexBufferLayout::ComponentCount::VEC3:
			return simul::crossplatform::PixelFormat::RGB_32_FLOAT;
		case clientrender::VertexBufferLayout::ComponentCount::VEC4:
			return simul::crossplatform::PixelFormat::RGBA_32_FLOAT;
		default:
			break;
		}

		break;
	case clientrender::VertexBufferLayout::Type::HALF:
		switch(attr.componentCount)
		{
		case clientrender::VertexBufferLayout::ComponentCount::SCALAR:
			return simul::crossplatform::PixelFormat::R_16_FLOAT;
		case clientrender::VertexBufferLayout::ComponentCount::VEC2:
			return simul::crossplatform::PixelFormat::RG_16_FLOAT;
		case clientrender::VertexBufferLayout::ComponentCount::VEC3:
			return simul::crossplatform::PixelFormat::RGB_16_FLOAT;
		case clientrender::VertexBufferLayout::ComponentCount::VEC4:
			return simul::crossplatform::PixelFormat::RGBA_16_FLOAT;
		default:
			break;
		}

		break;
	case clientrender::VertexBufferLayout::Type::INT:
		switch(attr.componentCount)
		{
		case clientrender::VertexBufferLayout::ComponentCount::SCALAR:
			return simul::crossplatform::PixelFormat::R_32_INT;
		case clientrender::VertexBufferLayout::ComponentCount::VEC2:
			return simul::crossplatform::PixelFormat::RG_8_SNORM;
		case clientrender::VertexBufferLayout::ComponentCount::VEC3:
			return simul::crossplatform::PixelFormat::RGB_10_A2_INT;
		case clientrender::VertexBufferLayout::ComponentCount::VEC4:
			return simul::crossplatform::PixelFormat::RGBA_32_INT;
		}

		break;
	default:
		break;
	}

	return simul::crossplatform::PixelFormat::UNKNOWN;
}

int GetByteSize(const clientrender::VertexBufferLayout::VertexAttribute &attr)
{
	int unit_size = 0;
	switch (attr.type)
	{
	case clientrender::VertexBufferLayout::Type::DOUBLE:
		unit_size = 8;
		break;
	default:
		unit_size = 4;
		break;
	};

	return unit_size * static_cast<int>(attr.componentCount);
}


PC_VertexBuffer::PC_VertexBuffer(const clientrender::RenderPlatform*const r)
	:clientrender::VertexBuffer(r), m_SimulBuffer(nullptr), m_layout(nullptr)
{}

void PC_VertexBuffer::Destroy()
{
	delete m_SimulBuffer;
	m_SimulBuffer = nullptr;
	delete m_layout;
	m_layout = nullptr;
}

void PC_VertexBuffer::Bind() const
{
}

void PC_VertexBuffer::Unbind() const
{
}

void pc_client::PC_VertexBuffer::Create(VertexBufferCreateInfo* pVertexBufferCreateInfo)
{
	m_CI = *pVertexBufferCreateInfo;

	const auto* const rp = static_cast<const PC_RenderPlatform* const> (renderPlatform);
	auto* srp = rp->GetSimulRenderPlatform();
	m_SimulBuffer = srp->CreateBuffer();

	size_t numAttr = m_CI.layout->m_Attributes.size();
	simul::crossplatform::LayoutDesc* desc = new simul::crossplatform::LayoutDesc[numAttr];

	int byteOffset = 0;
	for(size_t i = 0; i < numAttr; i++)
	{
		auto& attr = m_CI.layout->m_Attributes[i];
		avs::AttributeSemantic semantic = static_cast<avs::AttributeSemantic>(attr.location);
		desc[i].semanticName = GetAttributeSemantic(semantic);
		desc[i].semanticIndex = GetAttributeSemanticIndex(semantic);
		desc[i].format = GetAttributeFormat(attr);
		desc[i].inputSlot = 0;
		desc[i].alignedByteOffset = byteOffset;

		if(desc[i].format == simul::crossplatform::UNKNOWN)
		{
			SCR_COUT << "ERROR: Unknown format for attribute: " << desc[i].semanticName << std::endl;
		}

		size_t this_size = GetByteSize(attr);
		if(m_CI.layout->m_PackingStyle == VertexBufferLayout::PackingStyle::INTERLEAVED)
		{
			byteOffset += (int)this_size;
		}
		else if(m_CI.layout->m_PackingStyle == VertexBufferLayout::PackingStyle::GROUPED)
		{
			byteOffset += static_cast<int>(this_size * m_CI.vertexCount);
		}

		desc[i].perInstance = false;
		desc[i].instanceDataStepRate = 0;
	}

	delete m_layout;
	m_layout = srp->CreateLayout(static_cast<int>(m_CI.layout->m_Attributes.size()), desc, m_CI.layout->m_PackingStyle == VertexBufferLayout::PackingStyle::INTERLEAVED);
	m_SimulBuffer->EnsureVertexBuffer(srp, (int)m_CI.vertexCount, m_layout, m_CI.data);
	delete[] desc;
}
