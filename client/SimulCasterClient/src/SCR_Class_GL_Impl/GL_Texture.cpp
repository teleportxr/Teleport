// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_Texture.h"

using namespace scc;
using namespace scr;
using namespace OVR;
void GL_Texture::SetExternalGlTexture(GLuint tex_id)
{
    m_Texture.texture=tex_id;
    m_Texture.target=GL_TEXTURE_EXTERNAL_OES;
    m_Texture.Height=m_CI.height;
    m_Texture.Width=m_CI.width;
}

void GL_Texture::Create(TextureCreateInfo* pTextureCreateInfo)
{
    m_CI = *pTextureCreateInfo;

    assert(TypeToGLTarget(m_CI.type) != 0);
    assert(ToGLFormat(m_CI.format) != 0);
    assert(ToBaseGLFormat(m_CI.format) != 0);

    m_Texture = GlTexture();
    m_Texture.target = TypeToGLTarget(m_CI.type);
    m_Texture.Width = m_CI.width;
    m_Texture.Height = m_CI.height;

    if(pTextureCreateInfo->externalResource)
        return;
    glGenTextures(1, &m_Texture.texture);
    glBindTexture(TypeToGLTarget(m_CI.type), m_Texture.texture);

	GL_CheckErrors("GL_Texture:Create 0");
    if(m_CI.compression == Texture::CompressionFormat::UNCOMPRESSED)
    {
        //Uncompressed Data
        switch (m_CI.type) {
            case Type::TEXTURE_UNKNOWN: {
                return;
            }
            case Type::TEXTURE_1D: {
                SCR_COUT("OpenGLES 3.0 doesn't support GL_TEXTURE_1D");
                return;
            }
            case Type::TEXTURE_2D: {
                glTexImage2D(TypeToGLTarget(m_CI.type), 0, ToGLFormat(m_CI.format), m_CI.width,
                             m_CI.height, 0, ToBaseGLFormat(m_CI.format), GL_UNSIGNED_BYTE,
                             m_CI.data);
                break;
            }
            case Type::TEXTURE_3D: {
                glTexImage3D(TypeToGLTarget(m_CI.type), 0, ToGLFormat(m_CI.format), m_CI.width,
                             m_CI.height, m_CI.depth, 0, ToBaseGLFormat(m_CI.format),
                             GL_UNSIGNED_BYTE, m_CI.data);
                break;
            }
            case Type::TEXTURE_1D_ARRAY:
            {
                SCR_COUT("OpenGLES 3.0 doesn't support GL_TEXTURE_1D_ARRAY");
                return;
            }
            case Type::TEXTURE_2D_ARRAY: {
                glTexImage3D(TypeToGLTarget(m_CI.type), 0, ToGLFormat(m_CI.format), m_CI.width,
                             m_CI.height, m_CI.depth, 0, ToBaseGLFormat(m_CI.format),
                             GL_UNSIGNED_BYTE, m_CI.data);
                break;
            }
            case Type::TEXTURE_2D_MULTISAMPLE: {
                SCR_COUT("OpenGLES 3.0 doesn't support GL_TEXTURE_2D_MULTISAMPLE");
                return;
            }
            case Type::TEXTURE_2D_MULTISAMPLE_ARRAY: {
                SCR_COUT("OpenGLES 3.0 doesn't support GL_TEXTURE_2D_MULTISAMPLE_ARRAY");
                return;
            }
            case Type::TEXTURE_CUBE_MAP: {
            	if(m_CI.data)
				{
					size_t           offset = 0;
					for (
							uint32_t i      = 0; i < 6; i++)
					{
						glTexImage2D(
								GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, ToGLFormat(m_CI.format),
								m_CI.width, m_CI.height, 0, ToBaseGLFormat(m_CI.format),
								GL_UNSIGNED_BYTE, m_CI.data + offset);
						offset += m_CI.size;
					}

					m_CI.size *= 6;
				}
            	else
				{
					GL_CheckErrors("GL_Texture:Create 1");
					GLenum glInternalFormat=ToGLFormat(m_CI.format);
					glTexStorage2D(GL_TEXTURE_CUBE_MAP, m_CI.arrayCount, glInternalFormat, m_CI.width, m_CI.width);
					GL_CheckErrors("GL_Texture:Create 2");
				}
				return;
            }
            case Type::TEXTURE_CUBE_MAP_ARRAY: {
                SCR_COUT("OpenGLES 3.0 doesn't support GL_TEXTURE_CUBE_MAP_ARRAY");
                return;
            }
            default:
				SCR_CERR_BREAK("Unsupported texture type",1);
            	return;
        }
    }
    else
    {
        assert(ToGLCompressedFormat(m_CI.compression, m_CI.bytesPerPixel) != 0);
        //Compressed Data
        switch (m_CI.type) {
            case Type::TEXTURE_UNKNOWN: {
                return;
            }
            case Type::TEXTURE_1D: {
                SCR_COUT("OpenGLES 3.0 doesn't support GL_TEXTURE_1D for compressed textures");
                return;
            }
            case Type::TEXTURE_2D: {
                glCompressedTexImage2D(TypeToGLTarget(m_CI.type), 0, ToGLCompressedFormat(m_CI.compression, m_CI.bytesPerPixel),
                        m_CI.width, m_CI.height, 0, (GLsizei)m_CI.size, m_CI.data);
                break;
            }
            case Type::TEXTURE_3D: {
                glCompressedTexImage3D(TypeToGLTarget(m_CI.type), 0, ToGLCompressedFormat(m_CI.compression, m_CI.bytesPerPixel),
                                       m_CI.width, m_CI.height, m_CI.depth, 0, (GLsizei)m_CI.size, m_CI.data);
                break;
            }
            case Type::TEXTURE_1D_ARRAY: {
                SCR_COUT("OpenGLES 3.0 doesn't support GL_TEXTURE_1D_ARRAY for compressed textures");
                return;
            }
            case Type::TEXTURE_2D_ARRAY: {
                SCR_COUT("OpenGLES 3.0 doesn't support GL_TEXTURE_2D_ARRAY for compressed textures");
                return;
            }
            case Type::TEXTURE_2D_MULTISAMPLE: {
                SCR_COUT("OpenGLES 3.0 doesn't support GL_TEXTURE_2D_MULTISAMPLE for compressed textures");
                return;
            }
            case Type::TEXTURE_2D_MULTISAMPLE_ARRAY: {
                SCR_COUT("OpenGLES 3.0 doesn't support GL_TEXTURE_2D_MULTISAMPLE_ARRAY for compressed textures");
                return;
            }
            case Type::TEXTURE_CUBE_MAP: {
                size_t offset = 0;
                for (uint32_t i = 0; i < 6; i++) {
                    glCompressedTexImage2D(TypeToGLTarget(m_CI.type), 0, ToGLCompressedFormat(m_CI.compression, m_CI.bytesPerPixel),
                                           m_CI.width, m_CI.height, 0, (GLsizei)m_CI.size / 6, m_CI.data + offset);
                    offset += m_CI.size / 6;
                }
            }
            case Type::TEXTURE_CUBE_MAP_ARRAY: {
                SCR_COUT("OpenGLES 3.0 doesn't support GL_TEXTURE_CUBE_MAP_ARRAY for compressed textures");
                return;
            }
			default:
			SCR_CERR_BREAK("Unsupported texture type",1);
				return;
        }
    }
}

void GL_Texture::Destroy()
{
    if(!m_CI.externalResource)
        glDeleteTextures(1, &m_Texture.texture);
}

void GL_Texture::Bind() const
{
    glActiveTexture(GL_TEXTURE0 + static_cast<unsigned int>(m_CI.slot));
    GLenum gl_target=TypeToGLTarget(m_CI.type);

    glBindTexture(gl_target, m_Texture.texture);
    OVR::GL_CheckErrors("GL_Texture::Bind");
}

void GL_Texture::BindForWrite(uint32_t slot) const
{
    glBindImageTexture(slot,m_Texture.texture,0,GL_TRUE,0,GL_WRITE_ONLY,GL_RGBA8);
	OVR::GL_CheckErrors("GL_Texture::BindForWrite");
}

void GL_Texture::Unbind() const
{
    glBindTexture((unsigned int)m_CI.type, 0);
}

void GL_Texture::UseSampler(const std::shared_ptr<Sampler>& sampler)
{
    /*if(!this)
        return; */

    m_Sampler = sampler;
    const GL_Sampler* glSampler = dynamic_cast<const GL_Sampler*>(m_Sampler.get());

    Bind();
    glTexParameteri(TypeToGLTarget(m_CI.type), GL_TEXTURE_WRAP_S, glSampler->ToGLWrapType(glSampler->GetSamplerCreateInfo().wrapU));
    glTexParameteri(TypeToGLTarget(m_CI.type), GL_TEXTURE_WRAP_T, glSampler->ToGLWrapType(glSampler->GetSamplerCreateInfo().wrapV));
    glTexParameteri(TypeToGLTarget(m_CI.type), GL_TEXTURE_WRAP_R, glSampler->ToGLWrapType(glSampler->GetSamplerCreateInfo().wrapW));

    glTexParameteri(TypeToGLTarget(m_CI.type), GL_TEXTURE_MIN_FILTER, glSampler->ToGLFilterType(glSampler->GetSamplerCreateInfo().minFilter));
    glTexParameteri(TypeToGLTarget(m_CI.type), GL_TEXTURE_MAG_FILTER, glSampler->ToGLFilterType(glSampler->GetSamplerCreateInfo().magFilter));

    if(glSampler->GetSamplerCreateInfo().minFilter == Sampler::Filter::MIPMAP_LINEAR
        || glSampler->GetSamplerCreateInfo().minFilter == Sampler::Filter::MIPMAP_NEAREST)
    {
        GenerateMips();
    }
    Unbind();
}

void GL_Texture::GenerateMips()
{
    glGenerateMipmap(TypeToGLTarget(m_CI.type));
}

GLenum GL_Texture::TypeToGLTarget(Type type) const
{
    switch (type)
    {
        case Type::TEXTURE_UNKNOWN:
        	exit(1);
        case Type::TEXTURE_1D:
            return 0; //GL_TEXTURE_1D;
        case Type::TEXTURE_2D:
            return GL_TEXTURE_2D;
        case Type::TEXTURE_2D_EXTERNAL_OES:
            return GL_TEXTURE_EXTERNAL_OES;
        case Type::TEXTURE_3D: return GL_TEXTURE_3D;
        case Type::TEXTURE_1D_ARRAY: return 0; //GL_TEXTURE_1D_ARRAY;
        case Type::TEXTURE_2D_ARRAY: return GL_TEXTURE_2D_ARRAY;
        case Type::TEXTURE_2D_MULTISAMPLE: return GL_TEXTURE_2D_MULTISAMPLE;
        case Type::TEXTURE_2D_MULTISAMPLE_ARRAY: return GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
        case Type::TEXTURE_CUBE_MAP: return GL_TEXTURE_CUBE_MAP;
        case Type::TEXTURE_CUBE_MAP_ARRAY: return 0; //TEXTURE_CUBE_MAP_ARRAY;
        default:
			exit(1);
    }
}

GLenum GL_Texture::ToBaseGLFormat(Format format) const
{
    switch(format) {

        case Format::RGBA32F:
        case Format::RGBA32UI:
        case Format::RGBA32I:
        case Format::RGBA16F:
        case Format::RGBA16UI:
        case Format::RGBA16I:
        case Format::RGBA16_SNORM:
        case Format::RGBA16:
        case Format::RGBA8UI:
        case Format::RGBA8I:
        case Format::RGBA8_SNORM:
        case Format::RGBA8:
        case Format::BGRA8:
            return GL_RGBA;

        case Format::RGB32F:
        case Format::R11F_G11F_B10F:
        case Format::RGB10_A2UI:
        case Format::RGB10_A2:
            return GL_RGB;

        case Format::RG32F:
        case Format::RG32UI:
        case Format::RG32I:
        case Format::RG16F:
        case Format::RG16UI:
        case Format::RG16I:
        case Format::RG16_SNORM:
        case Format::RG16:
        case Format::RG8UI:
        case Format::RG8I:
        case Format::RG8_SNORM:
        case Format::RG8:
            return GL_RG;

        case Format::R32F:
        case Format::R32UI:
        case Format::R32I:
        case Format::R16F:
        case Format::R16UI:
        case Format::R16I:
        case Format::R16_SNORM:
        case Format::R16:
        case Format::R8UI:
        case Format::R8I:
        case Format::R8_SNORM:
        case Format::R8:
            return GL_RED;

        case Format::DEPTH_COMPONENT32F:
        case Format::DEPTH_COMPONENT32:
        case Format::DEPTH_COMPONENT24:
        case Format::DEPTH_COMPONENT16:
            return GL_DEPTH_COMPONENT;

        case Format::DEPTH_STENCIL:
        case Format::DEPTH32F_STENCIL8:
        case Format::DEPTH24_STENCIL8:
            return GL_DEPTH_STENCIL;

        case Format::UNSIGNED_INT_24_8:
        case Format::FLOAT_32_UNSIGNED_INT_24_8_REV:
            return GL_RGBA;
        case Format::FORMAT_UNKNOWN:
            return 0;
        default:
            SCR_COUT("Unknown texture format");
            return 0;
    }
}

GLenum GL_Texture::ToGLFormat(Format format) const {
    switch (format) {

        case Format::RGBA32F:
            return GL_RGBA32F;
        case Format::RGBA32UI:
            return GL_RGBA32UI;
        case Format::RGBA32I:
            return GL_RGBA32I;
        case Format::RGBA16F:
            return GL_RGBA16F;
        case Format::RGBA16UI:
            return GL_RGBA16UI;
        case Format::RGBA16I:
            return GL_RGBA16I;
        case Format::RGBA16_SNORM:
            return 0; //GL_RGBA16_SNORM;
        case Format::RGBA16:
            return 0; //GL_RGBA16;
        case Format::RGBA8UI:
            return GL_RGBA8UI;
        case Format::RGBA8I:
            return GL_RGBA8I;
        case Format::RGBA8_SNORM:
            return GL_RGBA8_SNORM;
        case Format::RGBA8:
            return GL_RGBA8;
        case Format::BGRA8:
            return GL_BGRA_EXT; //TO DO: Look into this extension!

        case Format::RGB32F:
            return GL_RGB32F;
        case Format::R11F_G11F_B10F:
            return GL_R11F_G11F_B10F;
        case Format::RGB10_A2UI:
            return GL_RGB10_A2UI;
        case Format::RGB10_A2:
            return GL_RGB10_A2;

        case Format::RG32F:
            return GL_RG32F;
        case Format::RG32UI:
            return GL_RG32UI;
        case Format::RG32I:
            return GL_RG32I;
        case Format::RG16F:
            return GL_RG16F;
        case Format::RG16UI:
            return GL_RG16UI;
        case Format::RG16I:
            return GL_RG16I;
        case Format::RG16_SNORM:
            return 0; //GL_RG16_SNORM;
        case Format::RG16:
            return 0; //GL_RG16;
        case Format::RG8UI:
            return GL_RG8UI;
        case Format::RG8I:
            return GL_RG8I;
        case Format::RG8_SNORM:
            return GL_RG8_SNORM;
        case Format::RG8:
            return GL_RG8;

        case Format::R32F:
            return GL_R32F;
        case Format::R32UI:
            return GL_R32UI;
        case Format::R32I:
            return GL_R32I;
        case Format::R16F:
            return GL_R16F;
        case Format::R16UI:
            return GL_R16UI;
        case Format::R16I:
            return GL_R16I;
        case Format::R16_SNORM:
            return 0; //GL_R16_SNORM;
        case Format::R16:
            return 0; //GL_R16;
        case Format::R8UI:
            return GL_R8UI;
        case Format::R8I:
            return GL_R8I;
        case Format::R8_SNORM:
            return GL_R8_SNORM;
        case Format::R8:
            return GL_R8;

        case Format::DEPTH_COMPONENT32F:
            return GL_DEPTH_COMPONENT32F;
        case Format::DEPTH_COMPONENT32:
            return 0; //GL_DEPTH_COMPONENT32;
        case Format::DEPTH_COMPONENT24:
            return GL_DEPTH_COMPONENT24;
        case Format::DEPTH_COMPONENT16:
            return GL_DEPTH_COMPONENT16;

        case Format::DEPTH_STENCIL:
            return GL_DEPTH_STENCIL;
        case Format::DEPTH32F_STENCIL8:
            return GL_DEPTH32F_STENCIL8;
        case Format::DEPTH24_STENCIL8:
            return GL_DEPTH24_STENCIL8;

        case Format::UNSIGNED_INT_24_8:
            return GL_UNSIGNED_INT_24_8;
        case Format::FLOAT_32_UNSIGNED_INT_24_8_REV:
            return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
        case Format::FORMAT_UNKNOWN:
            return 0;
            default:
                 SCR_CERR("Unknown format");
                 return 0;

    }
}

GLenum GL_Texture::ToGLCompressedFormat(CompressionFormat format, uint32_t bytesPerPixel) const
{
    switch (format)
    {
        case Texture::CompressionFormat::UNCOMPRESSED:
            return 0;
        case Texture::CompressionFormat::BC1:
            if(GL_EXT_texture_compression_s3tc)
                return bytesPerPixel == 4 ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : bytesPerPixel == 3 ? GL_COMPRESSED_RGB_S3TC_DXT1_EXT : 0;
            else
                return 0;
        case Texture::CompressionFormat::BC3:
            if(GL_EXT_texture_compression_s3tc)
                return bytesPerPixel == 4 ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : 0;
            else
                return 0;
        case Texture::CompressionFormat::BC4:
            return 0;
        case Texture::CompressionFormat::BC5:
                return 0;
        case Texture::CompressionFormat::ETC1:
            if(GL_OES_compressed_ETC1_RGB8_texture)
                return bytesPerPixel == 4 ? 0 : bytesPerPixel == 3 ? GL_OES_compressed_ETC1_RGB8_texture : 0;
            else
                return 0;
        case Texture::CompressionFormat::ETC2:
            return bytesPerPixel == 4 ? GL_COMPRESSED_RGBA8_ETC2_EAC : bytesPerPixel == 3 ? GL_COMPRESSED_RGB8_ETC2 : 0;
        case Texture::CompressionFormat::PVRTC1_4_OPAQUE_ONLY:
            if(GL_IMG_texture_compression_pvrtc)
                return bytesPerPixel == 4 ? GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG : bytesPerPixel == 3 ? GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG : 0;
            else
                return 0;
        case Texture::CompressionFormat::BC7_M6_OPAQUE_ONLY:
            return 0;
    }
}