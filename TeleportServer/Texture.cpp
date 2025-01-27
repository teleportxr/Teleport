#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Texture.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#include "GeometryStore.h"
#include <filesystem>
#include <fmt/core.h>
#include <ktx.h>
#include <vkformat_enum.h>
#include <fstream>
#include <compressonator.h>
#include <algorithm>

using namespace teleport;
using namespace server;
using namespace std::filesystem;
using std::filesystem::path;
using std::string;
#pragma optimize("",off)
static bool g_bAbortCompression=false;
static float compressionProgress=0.0f;
static bool CompressionCallback(float fProgress, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2)
{
    compressionProgress=fProgress;
    return g_bAbortCompression;
}
float teleport::server::GetCompressionProgress()
{
	return compressionProgress;
}
static CMP_FORMAT VkFormatToCompressonatorFormat(VkFormat f)
{
	switch(f)
	{
	    // Compression formats ------------ GPU Mapping DirectX, Vulkan and OpenGL formats and comments --------
    // Compressed Format 0xSnn1..0xSnnF   (Keys 0x00Bv..0x00Bv) S =1 is signed, 0 = unsigned, B =Block Compressors 1..7 (BC1..BC7) and v > 1 is a variant like signed or swizzle
    case VK_FORMAT_BC2_UNORM_BLOCK: return CMP_FORMAT_BC2;                                   // compressed texture format with explicit alpha for Microsoft DirectX10. Identical to DXT3. Eight bits per pixel.
    case VK_FORMAT_BC3_UNORM_BLOCK: return CMP_FORMAT_BC3;                                   // compressed texture format with interpolated alpha for Microsoft DirectX10. Identical to DXT5. Eight bits per pixel.
    case VK_FORMAT_BC4_UNORM_BLOCK: return CMP_FORMAT_BC4;                                   // compressed texture format for Microsoft DirectX10. Identical to ATI1N. Four bits per pixel.
    case VK_FORMAT_BC4_SNORM_BLOCK: return CMP_FORMAT_BC4_S;                                  // compressed texture format for Microsoft DirectX10. Identical to ATI1N. Four bits per pixel.
    case VK_FORMAT_BC5_UNORM_BLOCK: return CMP_FORMAT_BC5;                                   // compressed texture format for Microsoft DirectX10. Identical to ATI2N_XY. Eight bits per pixel.
    case VK_FORMAT_BC5_SNORM_BLOCK: return CMP_FORMAT_BC5_S;                                   // compressed texture format for Microsoft DirectX10. Identical to ATI2N_XY. Eight bits per pixel.
    case VK_FORMAT_BC6H_UFLOAT_BLOCK: return CMP_FORMAT_BC6H;  //       CMP_FORMAT_BC6H_SF = 0x1061,  //  VK_FORMAT_BC6H_SFLOAT_BLOCK     CMP_FORMAT_BC7     = 0x0071,  //  VK_FORMAT_BC7_UNORM_BLOCK 
	default:
		return CMP_FORMAT_Unknown;

	};
}
static avs::TextureFormat VkFormatToTeleportFormat(VkFormat f)
{
	switch(f)
	{
	    // Compression formats ------------ GPU Mapping DirectX, Vulkan and OpenGL formats and comments --------
    // Compressed Format 0xSnn1..0xSnnF   (Keys 0x00Bv..0x00Bv) S =1 is signed, 0 = unsigned, B =Block Compressors 1..7 (BC1..BC7) and v > 1 is a variant like signed or swizzle
    case VK_FORMAT_BC2_UNORM_BLOCK: return avs::TextureFormat::RGBA8;                                   // compressed texture format with explicit alpha for Microsoft DirectX10. Identical to DXT3. Eight bits per pixel.
    case VK_FORMAT_BC3_UNORM_BLOCK: return avs::TextureFormat::RGBA8;                                   // compressed texture format with interpolated alpha for Microsoft DirectX10. Identical to DXT5. Eight bits per pixel.
    case VK_FORMAT_BC4_UNORM_BLOCK: return avs::TextureFormat::RGBA8;                                   // compressed texture format for Microsoft DirectX10. Identical to ATI1N. Four bits per pixel.
    case VK_FORMAT_BC4_SNORM_BLOCK: return avs::TextureFormat::RGBA8;                                  // compressed texture format for Microsoft DirectX10. Identical to ATI1N. Four bits per pixel.
    case VK_FORMAT_BC5_UNORM_BLOCK: return avs::TextureFormat::RGBA8;                                   // compressed texture format for Microsoft DirectX10. Identical to ATI2N_XY. Eight bits per pixel.
    case VK_FORMAT_BC5_SNORM_BLOCK: return avs::TextureFormat::RGBA8;                                   // compressed texture format for Microsoft DirectX10. Identical to ATI2N_XY. Eight bits per pixel.
    case VK_FORMAT_BC6H_UFLOAT_BLOCK: return avs::TextureFormat::RGBA16F;  //       CMP_FORMAT_BC6H_SF = 0x1061,  //  VK_FORMAT_BC6H_SFLOAT_BLOCK     CMP_FORMAT_BC7     = 0x0071,  //  VK_FORMAT_BC7_UNORM_BLOCK 
	default:
		return avs::TextureFormat::INVALID;
	};
}

static CMP_FORMAT TeleportFormatToCompressonatorFormat(avs::TextureFormat f)
{
	switch(f)
	{
	    // Compression formats ------------ GPU Mapping DirectX, Vulkan and OpenGL formats and comments --------
    // Compressed Format 0xSnn1..0xSnnF   (Keys 0x00Bv..0x00Bv) S =1 is signed, 0 = unsigned, B =Block Compressors 1..7 (BC1..BC7) and v > 1 is a variant like signed or swizzle
    case avs::TextureFormat::G8: return CMP_FORMAT_R_8;              
    case avs::TextureFormat::BGRA8: return CMP_FORMAT_BGRA_8888;     
    case avs::TextureFormat::BGRE8: return CMP_FORMAT_BGRA_8888;           
    case avs::TextureFormat::RGBA16: return CMP_FORMAT_RGBA_16;        
    case avs::TextureFormat::RGBA16F: return CMP_FORMAT_RGBA_16F;         
    case avs::TextureFormat::RGBA8: return CMP_FORMAT_RGBA_8888;         
    case avs::TextureFormat::RGBE8: return CMP_FORMAT_RGBA_8888;   
	case avs::TextureFormat::D16F: return CMP_FORMAT_R_16F;
    case avs::TextureFormat::D32F: return CMP_FORMAT_R_32F;            
    case avs::TextureFormat::RGBA32F: return CMP_FORMAT_RGBA_32F;       
    case avs::TextureFormat::RGB8: return CMP_FORMAT_RGB_888;      
	default:
		return CMP_FORMAT_Unknown;
	};
}

static VkFormat GetDesiredVkFormat(avs::TextureFormat f)
{
// TODO: Switch 8bpp to BC7?
	switch(f)
	{
    case avs::TextureFormat::G8: return VK_FORMAT_R8_UINT;              
    case avs::TextureFormat::BGRA8: return VK_FORMAT_BC3_UNORM_BLOCK;     
    case avs::TextureFormat::BGRE8: return VK_FORMAT_BC3_UNORM_BLOCK;           
    case avs::TextureFormat::RGBA16: return VK_FORMAT_R16G16B16A16_UNORM;        
    case avs::TextureFormat::RGBA16F: return VK_FORMAT_BC6H_UFLOAT_BLOCK;         
    case avs::TextureFormat::RGBA8: return VK_FORMAT_BC3_UNORM_BLOCK;         
    case avs::TextureFormat::RGBE8: return VK_FORMAT_BC3_UNORM_BLOCK;   
	case avs::TextureFormat::D16F: return VK_FORMAT_R16_SFLOAT;
    case avs::TextureFormat::D32F: return VK_FORMAT_R32_SFLOAT;            
    case avs::TextureFormat::RGBA32F: return VK_FORMAT_BC6H_UFLOAT_BLOCK;       
    case avs::TextureFormat::RGB8: return VK_FORMAT_BC3_UNORM_BLOCK;      
	default:
		return VK_FORMAT_UNDEFINED;
	};
}

std::string PathToName(ExtractedTexture &textureData,const std::string &filename,string &ext)
{
	path pth(filename);
	ext=pth.extension().generic_string();
	GeometryStore &geometryStore=GeometryStore::GetInstance();
	path cachePath = geometryStore.GetCachePath();
	path filename_path = path(filename).replace_extension("");
	path relative_path = filename_path.lexically_relative(cachePath);
	std::string rel_path_str = relative_path.generic_string();
	textureData.SetNameFromPath(rel_path_str);
	std::filesystem::file_time_type rawFileTime = std::filesystem::last_write_time(filename);
	textureData.lastModified = rawFileTime.time_since_epoch().count();
	return textureData.getName();
}

// Encode a single uncompressed image into a specified format ready for compression.
static std::vector<uint8_t> EncodeLayer(VkFormat targetForamt,const avs::Texture &avsTexture,int m,int l,int f)
{
	std::vector<uint8_t> encodedLayer;
	int imageIndex=avsTexture.MipLayerFaceToIndex(m,l,f);
	if(imageIndex>=avsTexture.images.size())
	{
		TELEPORT_WARN("Texture {}: Image count does not match texture format.",avsTexture.name);
		return std::move(encodedLayer);
	}
	const std::vector<uint8_t> &sourceLayer=avsTexture.images[imageIndex].data;
	//CMP_MipSet dstMipSet;
	//CMP_CreateMipSet(&dstMipSet, avsTexture.width, avsTexture.height, 1, CMP_ChannelFormat::CF_Float16, CMP_TextureType::TT_2D);
	CMP_Texture destTexture;
	int mip_width=std::max(uint32_t(1),avsTexture.width>>m);
	int mip_height=std::max(uint32_t(1),avsTexture.height>>m);
	
	CMP_FORMAT sourceFormat=TeleportFormatToCompressonatorFormat(avsTexture.format);
	CMP_Texture srcTexture;
	//CMP_MipSet srcMipSet;
	//CMP_CreateMipSet(&srcMipSet, avsTexture.width, avsTexture.height, 1, CMP_ChannelFormat::CF_Float16, CMP_TextureType::TT_2D);
	//srcMipSet.m_nMipLevels=avsTexture.mipCount;
	srcTexture.dwSize     = sizeof(srcTexture);
	srcTexture.dwWidth    =mip_width;
	srcTexture.dwHeight   =mip_height;
	//srcTexture.pMipSet		=&srcMipSet;
	srcTexture.dwPitch    = 0;
	srcTexture.format     = sourceFormat;
	srcTexture.dwDataSize = CMP_CalculateBufferSize(&srcTexture);
	srcTexture.pData      = const_cast<uint8_t*>(sourceLayer.data());
	if(srcTexture.dwDataSize!=sourceLayer.size())
	{
		TELEPORT_WARN("Texture size mismatch.");
		return std::move(encodedLayer);
	}

	CMP_FORMAT destFormat=CMP_FORMAT_BC6H;
	destTexture.dwSize     = sizeof(destTexture);
	destTexture.dwWidth    = srcTexture.dwWidth;
	destTexture.dwHeight   = srcTexture.dwHeight;
	//destTexture.pMipSet		=&dstMipSet;
	destTexture.dwPitch    = 0;
	destTexture.format     = destFormat;
	destTexture.dwDataSize = CMP_CalculateBufferSize(&destTexture);
	encodedLayer.resize(destTexture.dwDataSize);
	destTexture.pData      = encodedLayer.data();
	static float fQuality=1.0f;
	CMP_CompressOptions options = {0};
	options.dwSize              = sizeof(options);
	options.fquality            = fQuality;  // Quality
	options.dwnumThreads        = 0;         // Number of threads to use per texture set to auto

	CMP_ERROR cmp_status;
	try
	{
		cmp_status = CMP_ConvertTexture(&srcTexture, &destTexture, &options, &CompressionCallback);
	}
	catch (const std::exception& ex)
	{
		TELEPORT_WARN("Error: {0}\n", ex.what());
	}
	if (cmp_status != CMP_OK)
	{
		TELEPORT_WARN("Texture transcoding failed.\n");
	}
	return std::move(encodedLayer);
}

void teleport::server::LoadAsKtxFile( ExtractedTexture &extractedTexture, const std::vector<char> &data,const std::string &filename)
{
	string ext;
	extractedTexture.texture.name=PathToName(extractedTexture,filename,ext);
	extractedTexture.texture.compressedData.resize(data.size());
	memcpy(extractedTexture.texture.compressedData.data(),data.data(),data.size());
	ktxTextureCreateFlags createFlags=KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT;
	ktxTexture *ktxt=nullptr;
	KTX_error_code 	result=ktxTexture_CreateFromMemory (extractedTexture.texture.compressedData.data(), extractedTexture.texture.compressedData.size()
		,  createFlags, &ktxt);
	ktxTexture2 *ktx2Texture = (ktxTexture2* )ktxt;
	if(!ktx2Texture)
		return;
	avs::Texture &avsTexture = extractedTexture.texture;

	avsTexture.width	=ktx2Texture->baseWidth;
	avsTexture.height	=ktx2Texture->baseHeight;
	avsTexture.depth	=ktx2Texture->baseDepth;

	avsTexture.arrayCount	=ktx2Texture->numLayers;
	avsTexture.mipCount		=ktx2Texture->numLevels;
	avsTexture.cubemap		=ktx2Texture->isCubemap;

	avsTexture.format		=VkFormatToTeleportFormat((VkFormat)(ktx2Texture->vkFormat));
	avsTexture.valueScale	=1.0f;
	avsTexture.compression	=avs::TextureCompression::KTX;
	avsTexture.compressed	=true;
	if(ktxt)
		ktxTexture_Destroy(ktxt);
}

bool teleport::server::CompressToKtx2(ExtractedTexture &extractedTexture,std::string assetPath, std::shared_ptr<PrecompressedTexture> compressionData)
{
	GeometryStore &geometryStore = GeometryStore::GetInstance();
	avs::Texture &avsTexture = extractedTexture.texture;
	// The texture's name:
	extractedTexture.SetNameFromPath(assetPath);
 
	// First, compress the texture into BC6H format:
	ktxTexture2* ktx2Texture=nullptr; 
	ktxTextureCreateInfo createInfo;
	KTX_error_code result;
	ktx_uint32_t  layer, faceSlice;
	ktx_size_t srcSize=0;
 
	createInfo.vkFormat = VK_FORMAT_BC6H_UFLOAT_BLOCK;
	createInfo.baseWidth = avsTexture.width;
	createInfo.baseHeight = avsTexture.height;
	createInfo.baseDepth = avsTexture.depth;
	createInfo.numDimensions = avsTexture.depth<=1?2:3;
	// Note: it is not necessary to provide a full mipmap pyramid.
	createInfo.numLevels =avsTexture.mipCount;// log2(createInfo.baseWidth) + 1;
	createInfo.numLayers = 1;
	createInfo.numFaces = avsTexture.cubemap?6:1;
	createInfo.isArray = KTX_FALSE;
	createInfo.generateMipmaps = KTX_FALSE;

	// Call ktxTexture1_Create to create a KTX texture.
	result = ktxTexture2_Create(&createInfo,
								KTX_TEXTURE_CREATE_ALLOC_STORAGE,
								&ktx2Texture);
	if(result!=KTX_SUCCESS)
	{
		TELEPORT_WARN("Ktx failed to encode texture {0}.",extractedTexture.getName());
		return false;
	}
	// Allocate the compressed storage.
	avsTexture.compressedData.resize(ktx2Texture->dataSize);
	//ktx2Texture->pData=avsTexture.compressedData.data();
	layer = 0;
	faceSlice = 0;
	ktx_size_t dataSizeUnc=ktxTexture_GetDataSizeUncompressed(ktxTexture(ktx2Texture));
	VkFormat targetFormat=GetDesiredVkFormat(avsTexture.format);
	for(uint32_t m=0;m<avsTexture.mipCount;m++)
	{
		std::vector<uint8_t> encodedImage=EncodeLayer( targetFormat,avsTexture, m,0,0);
		srcSize=encodedImage.size();
		result = ktxTexture_SetImageFromMemory(ktxTexture(ktx2Texture),
										   m, layer, faceSlice,
										   encodedImage.data(), srcSize);
		if(result!=KTX_SUCCESS)
		{
			TELEPORT_WARN("Ktx failed to encode mip {0} of texture {1}.",m,extractedTexture.getName());
			return false;
		}
	}
	// We don't know how much space to set aside for the ktx file, and the ktx lib won't tell us...
	// ktx library insists on allocating its own memory for this:
	uint8_t *dest=nullptr;
	ktx_size_t  dataSize=0;
	ktx_error_code_e err=ktxTexture_WriteToMemory(ktxTexture(ktx2Texture),(ktx_uint8_t** )&dest, &dataSize);
	if(err!=KTX_SUCCESS)
	{
		TELEPORT_WARN("Ktx failed to encode texture {0}.",extractedTexture.getName());
		return false;
	}
	avsTexture.compressedData.resize(dataSize);
	memcpy(avsTexture.compressedData.data(),dest,dataSize);
	free(dest);
	{
		ktxTextureCreateFlags createFlags=KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT;
		ktxTexture2* ktx2TextureB=nullptr;
		ktxTexture *ktxt=ktxTexture(ktx2TextureB);
		KTX_error_code result=ktxTexture_CreateFromMemory (extractedTexture.texture.compressedData.data(), extractedTexture.texture.compressedData.size()
			,  createFlags, &(ktxt));
		if(result!=KTX_SUCCESS)
		{
			TELEPORT_WARN("Ktx failed to encode texture {0}.",extractedTexture.getName());
			return false;
		}
	}
	ktxTexture_Destroy(ktxTexture(ktx2Texture));
	return true;
}


void teleport::server::LoadAsPng(ExtractedTexture &textureData, const std::vector<char> &data,const std::string &filename)
{
	path pth(filename);
	string ext;
	textureData.texture.name=PathToName(textureData,filename,ext);
	textureData.texture.compressedData.resize(data.size());
	memcpy(textureData.texture.compressedData.data(),data.data(),data.size());
}
template<typename T> void read_from_buffer(T &result,const uint8_t * &mem)
{
	if(!mem)
		return;
	const T *src=(const T*)mem;
	result=*src;
	src++;
	mem=(const uint8_t*)src;
}

void teleport::server::LoadAsTeleportTexture(ExtractedTexture &textureData, const std::vector<char> &data,const std::string &filename)
{
	path pth(filename);
	string ext;
	textureData.texture.name=PathToName(textureData,filename,ext);
	textureData.texture.compressedData.resize(data.size());
	memcpy(textureData.texture.compressedData.data(),data.data(),data.size());
	const uint8_t *src = textureData.texture.compressedData.data();
	if(!src)
		return;
	
		// dimensions.
	read_from_buffer(textureData.texture.width ,src);
	read_from_buffer(textureData.texture.height,src);
	read_from_buffer(textureData.texture.depth ,src);

		// additional information.
	read_from_buffer(textureData.texture.arrayCount		,src);
	read_from_buffer(textureData.texture.mipCount		,src);
	read_from_buffer(textureData.texture.cubemap,src);

		// format.
	read_from_buffer(textureData.texture.format,src);

		//Value scale - brightness number to scale the final texel by.
	read_from_buffer(textureData.texture.valueScale,src);
	textureData.texture.compression=avs::TextureCompression::PNG;

}
