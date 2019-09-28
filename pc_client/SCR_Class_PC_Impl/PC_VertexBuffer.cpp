// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_VertexBuffer.h"
#include "PC_RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Buffer.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include <libavstream/geometry/mesh_interface.hpp>

using namespace pc_client;
using namespace scr;
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
		return "TEXCOORD_0";
		break;
	case avs::AttributeSemantic::TEXCOORD_1:
		return "TEXCOORD_1";
		break;
	case avs::AttributeSemantic::COLOR_0:
		return "COLOR_0";
		break;
	case avs::AttributeSemantic::JOINTS_0:
		return "JOINTS_0";
		break;
	case avs::AttributeSemantic::WEIGHTS_0:
		return "WEIGHTS_0";
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
		return 0;
		break;
	case avs::AttributeSemantic::WEIGHTS_0:
		return 0;
		break;
	default:
		return 0;
		break;
	}
}

simul::crossplatform::PixelFormat GetAttributeFormat(const scr::VertexBufferLayout::VertexAttribute &attr)
{
	std::string fmt = "";
	switch(attr.type)
	{
		case scr::VertexBufferLayout::Type::FLOAT:
		{
			switch (attr.componentCount)
			{
			case scr::VertexBufferLayout::ComponentCount::SCALAR:
				return simul::crossplatform::PixelFormat::R_32_FLOAT;
			case scr::VertexBufferLayout::ComponentCount::VEC2:
				return simul::crossplatform::PixelFormat::RG_32_FLOAT;
			case scr::VertexBufferLayout::ComponentCount::VEC3:
				return simul::crossplatform::PixelFormat::RGB_32_FLOAT;
			case scr::VertexBufferLayout::ComponentCount::VEC4:
				return simul::crossplatform::PixelFormat::RGBA_32_FLOAT;
			default:
				break;
			};
		}
		case scr::VertexBufferLayout::Type::HALF:
		{
			switch (attr.componentCount)
			{
			case scr::VertexBufferLayout::ComponentCount::SCALAR:
				return simul::crossplatform::PixelFormat::R_16_FLOAT;
			case scr::VertexBufferLayout::ComponentCount::VEC2:
				return simul::crossplatform::PixelFormat::RG_16_FLOAT;
			case scr::VertexBufferLayout::ComponentCount::VEC3:
				return simul::crossplatform::PixelFormat::RGB_16_FLOAT;
			case scr::VertexBufferLayout::ComponentCount::VEC4:
				return simul::crossplatform::PixelFormat::RGBA_16_FLOAT;
			default:
				break;
			};
		}
		default:
			break;
	};
	return simul::crossplatform::PixelFormat::UNKNOWN;
}

int GetByteSize(const scr::VertexBufferLayout::VertexAttribute &attr)
{
	int unit_size = 0;
	switch (attr.type)
	{
	case scr::VertexBufferLayout::Type::DOUBLE:
		unit_size = 8;
	default:
		unit_size = 4;
		break;
	};
	return unit_size * (int)attr.componentCount;
}


PC_VertexBuffer::PC_VertexBuffer(const scr::RenderPlatform*const r) :scr::VertexBuffer(r),m_layout(nullptr),m_SimulBuffer(nullptr)
{
}

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

void pc_client::PC_VertexBuffer::Create(VertexBufferCreateInfo * pVertexBufferCreateInfo)
{
	m_CI = *pVertexBufferCreateInfo;
	
	size_t num_vertices = m_CI.size / m_CI.layout->m_Stride;
	const auto *const rp = static_cast<const PC_RenderPlatform* const> (renderPlatform);
	auto *srp = rp->GetSimulRenderPlatform();
	m_SimulBuffer = srp->CreateBuffer();
	size_t numAttr = m_CI.layout->m_Attributes.size();
	simul::crossplatform::LayoutDesc *desc = new simul::crossplatform::LayoutDesc[numAttr];

	int byteOffset = 0;
	for (size_t i = 0; i < numAttr; i++)
	{
		auto &attr = m_CI.layout->m_Attributes[i];
		avs::AttributeSemantic s = (avs::AttributeSemantic) attr.location;
		desc[i].semanticName = GetAttributeSemantic(s);
		desc[i].semanticIndex = GetAttributeSemanticIndex((avs::AttributeSemantic)i);
		desc[i].format = GetAttributeFormat(attr);
		desc[i].inputSlot = (int)i;
		size_t this_size = GetByteSize(attr);
		desc[i].alignedByteOffset = byteOffset; 
		if (m_CI.layout->m_PackingStyle == VertexBufferLayout::PackingStyle::INTERLEAVED)
		{
			byteOffset += GetByteSize(attr);
		}
		else if (m_CI.layout->m_PackingStyle == VertexBufferLayout::PackingStyle::GROUPED)
		{
			byteOffset += this_size * m_CI.vertexCount;
		}

		desc[i].perInstance = false;
		desc[i].instanceDataStepRate = 0;
	}
	delete m_layout;
	{
		simul::crossplatform::LayoutDesc desc[] =
				{
					{ "POSITION", 0, simul::crossplatform::RGB_32_FLOAT, 0, 0, false, 0 },
					{ "NORMAL", 0, simul::crossplatform::RGB_32_FLOAT, 0, 12, false, 0 },
					{ "TANGENT", 0, simul::crossplatform::RGBA_32_FLOAT, 0, 24, false, 0 },
					{ "TEXCOORD", 0, simul::crossplatform::RG_32_FLOAT, 0, 40, false, 0 },
					{ "TEXCOORD", 1, simul::crossplatform::RG_32_FLOAT, 0, 48, false, 0 },
				};
		m_layout = srp->CreateLayout(
					sizeof(desc) / sizeof(simul::crossplatform::LayoutDesc)
					, desc,true);
	}
	//m_layout = srp->CreateLayout( (int)m_CI.layout->m_Attributes.size(), desc, m_CI.layout->m_PackingStyle == VertexBufferLayout::PackingStyle::INTERLEAVED);// , m_CI.layout->m_PackingStyle == VertexBufferLayout::PackingStyle::INTERLEAVED);
	//m_layout->SetDesc(desc, (int)m_CI.layout->m_Attributes.size(), m_CI.layout->m_PackingStyle == VertexBufferLayout::PackingStyle::INTERLEAVED);
	m_SimulBuffer->EnsureVertexBuffer(srp, (int)num_vertices, m_layout, m_CI.data);
}
