#include "VertexBuffer.h"
#include "Platform/CrossPlatform/Buffer.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "TeleportCore/ErrorHandling.h"

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

platform::crossplatform::PixelFormat GetAttributeFormat(const clientrender::VertexBufferLayout::VertexAttribute& attr)
{
	switch(attr.type)
	{
	case clientrender::VertexBufferLayout::Type::FLOAT:
		switch(attr.componentCount)
		{
		case clientrender::VertexBufferLayout::ComponentCount::SCALAR:
			return platform::crossplatform::PixelFormat::R_32_FLOAT;
		case clientrender::VertexBufferLayout::ComponentCount::VEC2:
			return platform::crossplatform::PixelFormat::RG_32_FLOAT;
		case clientrender::VertexBufferLayout::ComponentCount::VEC3:
			return platform::crossplatform::PixelFormat::RGB_32_FLOAT;
		case clientrender::VertexBufferLayout::ComponentCount::VEC4:
			return platform::crossplatform::PixelFormat::RGBA_32_FLOAT;
		default:
			break;
		}

		break;
	case clientrender::VertexBufferLayout::Type::HALF:
		switch(attr.componentCount)
		{
		case clientrender::VertexBufferLayout::ComponentCount::SCALAR:
			return platform::crossplatform::PixelFormat::R_16_FLOAT;
		case clientrender::VertexBufferLayout::ComponentCount::VEC2:
			return platform::crossplatform::PixelFormat::RG_16_FLOAT;
		case clientrender::VertexBufferLayout::ComponentCount::VEC3:
			return platform::crossplatform::PixelFormat::RGB_16_FLOAT;
		case clientrender::VertexBufferLayout::ComponentCount::VEC4:
			return platform::crossplatform::PixelFormat::RGBA_16_FLOAT;
		default:
			break;
		}

		break;
	case clientrender::VertexBufferLayout::Type::INT:
		switch(attr.componentCount)
		{
		case clientrender::VertexBufferLayout::ComponentCount::SCALAR:
			return platform::crossplatform::PixelFormat::R_32_INT;
		case clientrender::VertexBufferLayout::ComponentCount::VEC2:
			return platform::crossplatform::PixelFormat::RG_8_SNORM;
		case clientrender::VertexBufferLayout::ComponentCount::VEC3:
			return platform::crossplatform::PixelFormat::RGB_10_A2_INT;
		case clientrender::VertexBufferLayout::ComponentCount::VEC4:
			return platform::crossplatform::PixelFormat::RGBA_32_INT;
		}

		break;
	default:
		break;
	}

	return platform::crossplatform::PixelFormat::UNKNOWN;
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

void VertexBuffer::Create(VertexBufferCreateInfo* pVertexBufferCreateInfo)
{
	m_CI = *pVertexBufferCreateInfo;

	m_SimulBuffer = renderPlatform->CreateBuffer();

	size_t numAttr = m_CI.layout->m_Attributes.size();
	platform::crossplatform::LayoutDesc* desc = new platform::crossplatform::LayoutDesc[numAttr];

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

		if(desc[i].format == platform::crossplatform::UNKNOWN)
		{
			TELEPORT_COUT << "ERROR: Unknown format for attribute: " << desc[i].semanticName << std::endl;
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
	m_layout = renderPlatform->CreateLayout(static_cast<int>(m_CI.layout->m_Attributes.size()), desc, m_CI.layout->m_PackingStyle == VertexBufferLayout::PackingStyle::INTERLEAVED);
	m_SimulBuffer->EnsureVertexBuffer(renderPlatform, (int)m_CI.vertexCount, m_layout, m_CI.data);
	delete[] desc;
}

void VertexBuffer::Destroy()
{
	delete m_SimulBuffer;
	m_SimulBuffer = nullptr;
	delete m_layout;
	m_layout = nullptr;
}

void VertexBuffer::Bind() const
{
}

void VertexBuffer::Unbind() const
{
}
