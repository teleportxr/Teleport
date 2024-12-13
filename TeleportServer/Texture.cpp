#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Texture.h"
#define BASISU_FORCE_DEVEL_MESSAGES 1
#include "basisu_comp.h"
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
	int imageIndex=avsTexture.MipLayerFaceToIndex(m,l,f);
	std::vector<uint8_t> encodedLayer;
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

bool teleport::server::ApplyBasisCompression(ExtractedTexture &extractedTexture,std::string assetPath, std::shared_ptr<PrecompressedTexture> compressionData,uint8_t compressionStrength,uint8_t compressionQuality)
{
	GeometryStore &geometryStore = GeometryStore::GetInstance();
	avs::Texture &avsTexture = extractedTexture.texture;
	// The texture's n
	extractedTexture.SetNameFromPath(assetPath);
	basisu::basis_compressor_params basisCompressorParams; // Parameters for basis compressor.
	basisCompressorParams.m_source_images.clear();
	// Basis stores mip 0 in m_source_images, and subsequent mips in m_source_mipmap_images.
	// They MUST have equal sizes.
	if (compressionData->numMips < 1)
	{
		TELEPORT_CERR << "Bad mipcount " << compressionData->numMips << "\n";
		return false;
	}
	size_t imagesPerMip = compressionData->images.size() / compressionData->numMips;
	if (imagesPerMip * compressionData->numMips != compressionData->images.size())
	{
		TELEPORT_CERR << "Bad image count " << compressionData->images.size() << " for " << compressionData->numMips << " mips.\n";
		return false;
	}
	size_t n = 0;
	int w = avsTexture.width;
	int h = avsTexture.height;
	bool breakout = false;
	for (size_t m = 0; m < compressionData->numMips; m++)
	{
		if (breakout)
			break;
		for (size_t i = 0; i < imagesPerMip; i++)
		{
			if (m > 0 && basisCompressorParams.m_source_mipmap_images.size() < imagesPerMip)
				basisCompressorParams.m_source_mipmap_images.push_back(basisu::vector<basisu::image>());
			basisu::image image(w, h);
			// TODO: This ONLY works for 8-bit rgba.
			basisu::color_rgba_vec &imageData = image.get_pixels();
			std::vector<uint8_t> &img = compressionData->images[n];
			if (img.size() > 4)
			{
				std::string pngString = "XXX";
				memcpy(pngString.data(), img.data() + 1, 3);
				if (pngString == "PNG")
				{
					TELEPORT_CERR << "Texture " << avsTexture.name << " was already a PNG, can't Basis-compress this.\n ";
					breakout = true;
					break;
				}
			}
			if (img.size() > 4 * imageData.size())
			{
				// Actually possible with small mips - the PNG can be bigger than the raw mip data.
				TELEPORT_CERR << "Image data size mismatch.\n";
				continue;
			}
			if (img.size() < 4 * imageData.size())
			{
				TELEPORT_CERR << "Image data size mismatch.\n";
				continue;
			}
			memcpy(imageData.data(), img.data(), img.size());
			if (m == 0)
				basisCompressorParams.m_source_images.push_back(std::move(image));
			else
				basisCompressorParams.m_source_mipmap_images[i].push_back(std::move(image));
			n++;
		}
		w = (w + 1) / 2;
		h = (h + 1) / 2;
	}
	std::string file_name = (geometryStore.GetCachePath() + "/") + extractedTexture.MakeFilename(assetPath);
	if (breakout)
		return false;
/*	if (compressionData->highQualityUASTC)
	{
		compressionData->highQualityUASTC = false;
		TELEPORT_CERR << "highQualityUASTC is not functional in Basis. Reverting to low-quality mode.\n";
	}*/
	// TODO: This doesn't work for mips>0. So can't flip textures from Unity for example.
	// basisCompressorParams.m_y_flip=true;
	basisCompressorParams.m_quality_level = compressionQuality;
	basisCompressorParams.m_compression_level = compressionStrength;

	basisCompressorParams.m_write_output_basis_files = false;
	basisCompressorParams.m_create_ktx2_file = true;
	basisCompressorParams.m_out_filename = (geometryStore.GetCachePath() + "/") + assetPath+ ".ktx2";
	basisCompressorParams.m_uastc = compressionData->highQualityUASTC;
	
	uint32_t num_threads = 32;
	if (compressionData->highQualityUASTC)
	{
		num_threads = 1;
		// Write this to a different filename, it's just for testing.
		/*	auto ext_pos = basisCompressorParams.m_out_filename.find(".basis");
			basisCompressorParams.m_out_filename = basisCompressorParams.m_out_filename.substr(0, ext_pos) + "-dll.basis";
			{
				texturesToCompress.erase(texturesToCompress.begin());
				TELEPORT_CERR << "highQualityUASTC is not functional for texture compression.\n";
				return;
			}*/

		// we want the equivalent of:
		// -uastc -uastc_rdo_m -no_multithreading -debug -stats -output_path "outputPath" "srcPng"
		basisCompressorParams.m_rdo_uastc_multithreading = false;
		basisCompressorParams.m_multithreading = false;
		// basisCompressorParams.m_ktx2_uastc_supercompression = basist::KTX2_SS_NONE;//= basist::KTX2_SS_ZSTANDARD;

		int uastc_level = std::clamp<int>(4, 0, 4);

		// static const uint32_t s_level_flags[5] = { basisu::cPackUASTCLevelFastest, basisu::cPackUASTCLevelFaster, basisu::cPackUASTCLevelDefault, basisu::cPackUASTCLevelSlower, basisu::cPackUASTCLevelVerySlow };

		// basisCompressorParams.m_pack_uastc_flags &= ~basisu::cPackUASTCLevelMask;
		// basisCompressorParams.m_pack_uastc_flags |= s_level_flags[uastc_level];

		// basisCompressorParams.m_rdo_uastc_dict_size = 32768;
		// basisCompressorParams.m_check_for_alpha=true;
		basisCompressorParams.m_debug = true;
		basisCompressorParams.m_status_output = true;
		basisCompressorParams.m_compute_stats = true;
		// basisCompressorParams.m_perceptual=true;
		// basisCompressorParams.m_validate=false;
		basisCompressorParams.m_mip_srgb = true;
		basisCompressorParams.m_quality_level = 128;
	}
	else
	{
		basisCompressorParams.m_mip_gen = compressionData->genMips;
		basisCompressorParams.m_mip_smallest_dimension = 4;
	}
	basisCompressorParams.m_tex_type = basist::basis_texture_type::cBASISTexType2D;
	if (avsTexture.cubemap)
	{
		basisCompressorParams.m_tex_type = basist::basis_texture_type::cBASISTexTypeCubemapArray;
	}
	if (!basisCompressorParams.m_pJob_pool)
	{
		basisCompressorParams.m_pJob_pool = new basisu::job_pool(num_threads);
	}

	if (!basisu::g_library_initialized)
		basisu::basisu_encoder_init(false, false);
	if (!basisu::g_library_initialized)
	{
		TELEPORT_CERR << "basisu_encoder_init failed.\n";
		return false;
	}
	basisu::basis_compressor basisCompressor;
	basisu::enable_debug_printf(true);
	bool ok = basisCompressor.init(basisCompressorParams);
	path filePath(file_name);
	if (ok)
	{
		basisu::basis_compressor::error_code result = basisCompressor.process();
		if (result == basisu::basis_compressor::error_code::cECSuccess)
		{
			basisu::uint8_vec basisTex;
			if(filePath.extension().generic_string()==".basis")
			{
				basisTex= basisCompressor.get_output_basis_file();
				avsTexture.compressedData.resize(basisCompressor.get_basis_file_size());
				assert(basisCompressor.get_basis_file_size()==basisTex.size());
				extractedTexture.texture.compression=avs::TextureCompression::BASIS_COMPRESSED;
				compressionData->textureCompression=avs::TextureCompression::BASIS_COMPRESSED;
			}
			else if(filePath.extension().generic_string()==".ktx2")
			{
				basisTex= basisCompressor.get_output_ktx2_file();
				extractedTexture.texture.compression=avs::TextureCompression::KTX;
				compressionData->textureCompression=avs::TextureCompression::KTX;
			}
			else
				return false;
			avsTexture.compressedData.resize(basisTex.size());
			unsigned char *target = avsTexture.compressedData.data();
			memcpy(target, basisTex.data(), avsTexture.compressedData.size());
			avsTexture.compressed=true;
		}
		else
		{
			TELEPORT_CERR << "Failed to compress texture \"" << avsTexture.name << "\"!\n";
		}
	}
	else
	{
		TELEPORT_CERR << "Failed to compress texture \"" << avsTexture.name << "\"! Basis Universal compressor failed to initialise.\n";
	}
	delete basisCompressorParams.m_pJob_pool;
	basisCompressorParams.m_pJob_pool = nullptr;
	//geometryStore.saveResourceBinary(file_name, extractedTexture);
	
	return true;
}

bool BasisValidate(basist::basisu_transcoder &dec, basist::basisu_file_info &fileinfo, const std::vector< char> &data)
{
#ifndef __ANDROID__

	TELEPORT_ASSERT(fileinfo.m_total_images == fileinfo.m_image_mipmap_levels.size());
	TELEPORT_ASSERT(fileinfo.m_total_images == dec.get_total_images(data.data(), (uint32_t)data.size()));

	TELEPORT_INFO("File info: ");
	TELEPORT_INFO("  Version: {0}", fileinfo.m_version);
	TELEPORT_INFO("  Total header size: {0}", fileinfo.m_total_header_size);
	TELEPORT_INFO("  Total selectors: {0}", fileinfo.m_total_selectors);
	TELEPORT_INFO("  Selector codebook size: {0}", fileinfo.m_selector_codebook_size);
	TELEPORT_INFO("  Total endpoints: {0}", fileinfo.m_total_endpoints);
	TELEPORT_INFO("  Endpoint codebook size: {0}", fileinfo.m_endpoint_codebook_size);
	TELEPORT_INFO("  Tables size: {0}", fileinfo.m_tables_size);
	TELEPORT_INFO("  Slices size: {0}", fileinfo.m_slices_size);
	TELEPORT_INFO("  Texture format: {0}",((fileinfo.m_tex_format == basist::basis_tex_format::cUASTC4x4) ? "UASTC" : "ETC1S"));
	TELEPORT_INFO("  Texture type: {0}", basist::basis_get_texture_type_name(fileinfo.m_tex_type));
	TELEPORT_INFO("  us per frame: {0} ({1})", fileinfo.m_us_per_frame, (fileinfo.m_us_per_frame ? (1.0f / ((float)fileinfo.m_us_per_frame / 1000000.0f)) : 0.0f));
	TELEPORT_INFO("  Total slices: {0}", (uint32_t)fileinfo.m_slice_info.size());
	TELEPORT_INFO("  Total images: {0}", fileinfo.m_total_images);
	TELEPORT_INFO("  Y Flipped: {0}, Has alpha slices: {1}", fileinfo.m_y_flipped , fileinfo.m_has_alpha_slices);
	TELEPORT_INFO("  userdata0: {0} userdata1: {1}", fileinfo.m_userdata0, fileinfo.m_userdata1);
	TELEPORT_INFO("  Per-image mipmap levels: ");
	for (uint32_t i = 0; i < fileinfo.m_total_images; i++)
		TELEPORT_INFO("%u {0}", fileinfo.m_image_mipmap_levels[i]);

	uint32_t total_texels = 0;

	TELEPORT_INFO("Image info:");
	for (uint32_t i = 0; i < fileinfo.m_total_images; i++)
	{
		basist::basisu_image_info ii;
		if (!dec.get_image_info(data.data(), (uint32_t)data.size(), ii, i))
		{
			TELEPORT_WARN("get_image_info() failed!\n");
			return false;
		}

		TELEPORT_WARN("Image {}: MipLevels: {} OrigDim: {}{}, BlockDim: {}{}, FirstSlice: {}, HasAlpha: {}\n", i, ii.m_total_levels, ii.m_orig_width, ii.m_orig_height,
			   ii.m_num_blocks_x, ii.m_num_blocks_y, ii.m_first_slice_index, (uint32_t)ii.m_alpha_flag);

		total_texels += ii.m_width * ii.m_height;
	}

	//TELEPORT_INFO("\nSlice info:\n");

	for (uint32_t i = 0; i < fileinfo.m_slice_info.size(); i++)
	{
		const basist::basisu_slice_info &sliceinfo = fileinfo.m_slice_info[i];
		try
		{
		std::string s=fmt::format("{}: OrigWidthHeight: {}{}, BlockDim: {}{}, TotalBlocks: {}, Compressed size: {}, Image: {}, Level: {}, UnpackedCRC16: 0x{}, alpha: {}, iframe: {}\n",
			   i,
			   sliceinfo.m_orig_width, sliceinfo.m_orig_height,
			   sliceinfo.m_num_blocks_x, sliceinfo.m_num_blocks_y,
			   sliceinfo.m_total_blocks,
			   sliceinfo.m_compressed_size,
			   sliceinfo.m_image_index, sliceinfo.m_level_index,
			   sliceinfo.m_unpacked_slice_crc16,
			   (uint32_t)sliceinfo.m_alpha_flag,
			   (uint32_t)sliceinfo.m_iframe_flag);
//		TELEPORT_INFO(s);
		}
		catch(...)
		{
		}
	}

	const float basis_bits_per_texel = data.size() * 8.0f / total_texels;
	// const float comp_bits_per_texel = comp_size * 8.0f / total_texels;

	// TELEPORT_INFO("Original size: %u, bits per texel: %3.3f\nCompressed size (Deflate): %u, bits per texel: %3.3f\n", (uint32_t)basis_file_data.size(), basis_bits_per_texel, (uint32_t)comp_size, comp_bits_per_texel);
#endif
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

void teleport::server::LoadAsBasisFile( ExtractedTexture &textureData, const std::vector<char> &data,const std::string &filename)
{
	string ext;
	textureData.texture.name=PathToName(textureData,filename,ext);
	textureData.texture.compressedData.resize(data.size());
	memcpy(textureData.texture.compressedData.data(),data.data(),data.size());
	static bool basis_transcoder_initialized=false;
	if (!basis_transcoder_initialized)
	{
		basist::basisu_transcoder_init();
		basis_transcoder_initialized=true;
	}
	path pth(filename);
	if(ext==".ktx2")
	{
		basist::ktx2_transcoder ktx_transcoder;
	
		static bool transcoder_initialized=false;
		if (!transcoder_initialized)
		{
			basist::basisu_transcoder_init();
			basis_transcoder_initialized=true;
		}
		ktx_transcoder.init(data.data(), (uint32_t)data.size());
		if (ktx_transcoder.start_transcoding())
		{
			basist::transcoder_texture_format basis_transcoder_textureFormat = basist::transcoder_texture_format::cTFRGBA32;
			basist::basis_tex_format basis_tex_format = ktx_transcoder.get_format();
			textureData.texture.mipCount = ktx_transcoder.get_levels();
			textureData.texture.arrayCount = ktx_transcoder.get_layers();
			textureData.texture.arrayCount=textureData.texture.arrayCount?textureData.texture.arrayCount:1;
			textureData.texture.cubemap=(ktx_transcoder.get_faces()==6);
			uint32_t numFaces=textureData.texture.cubemap?6:1;
			uint16_t numImages = textureData.texture.arrayCount * textureData.texture.mipCount*numFaces;
			textureData.texture.images.resize(numImages);
			
			uint16_t imageIndex = 0;
			size_t totalSize=0;
			for (uint32_t mipIndex = 0; mipIndex < textureData.texture.mipCount; mipIndex++)
			{
				for (uint32_t arrayIndex = 0; arrayIndex < textureData.texture.arrayCount; arrayIndex++)
				{
					for (uint32_t faceIndex = 0; faceIndex < numFaces; faceIndex++)
					{
						auto &image=textureData.texture.images[imageIndex];
						basist::ktx2_image_level_info ktx2_image_level_info;
						ktx_transcoder.get_image_level_info(ktx2_image_level_info,arrayIndex,mipIndex,faceIndex);
					
						uint32_t bytesPerPixel=basist::basis_get_bytes_per_block_or_pixel(basis_transcoder_textureFormat);
						size_t num_pixels=ktx2_image_level_info.m_width * ktx2_image_level_info.m_height;
						uint32_t outDataSize = bytesPerPixel * (uint32_t)num_pixels;
						
						auto &img = textureData.texture.images[imageIndex];
						img.data.resize(outDataSize);
						if (!ktx_transcoder.transcode_image_level(mipIndex,arrayIndex,faceIndex,img.data.data(), (uint32_t)num_pixels, basis_transcoder_textureFormat))
						{
							TELEPORT_CERR << "Texture failed to transcode mipmap level " << mipIndex << "." << std::endl;
						}
						imageIndex++;
						if(arrayIndex==0&&mipIndex==0&&faceIndex==0)
						{
							textureData.texture.width = ktx2_image_level_info.m_width;
							textureData.texture.height = ktx2_image_level_info.m_height;
							textureData.texture.depth = 1;
						}
					}
				}
			}
			textureData.texture.compression = avs::TextureCompression::KTX;
			// We don't know if the texture was originally RGBA or BGRA:
			textureData.texture.format= avs::TextureFormat::UNKNOWN;
			textureData.texture.compressed = true;
			if(textureData.texture.compressedData.size()==0)
			{
				throw(std::runtime_error("No data in texture."));
			}
		}
	}
	// We need a new transcoder for every basis file.
	else if(ext==".basis")
	{
		basist::basisu_transcoder basis_transcoder;
	
		basist::basisu_file_info fileinfo;
		if (!basis_transcoder.get_file_info(data.data(), (uint32_t)data.size(), fileinfo))
		{
			TELEPORT_CERR << "Failed to transcode texture " << textureData.texture.name << std::endl;
			return;
		}
		BasisValidate(basis_transcoder, fileinfo, data);
		if (basis_transcoder.start_transcoding(data.data(), (uint32_t)data.size()))
		{
			basist::transcoder_texture_format basis_transcoder_textureFormat = basist::transcoder_texture_format::cTFRGBA32;
			textureData.texture.mipCount = basis_transcoder.get_total_image_levels(data.data(), (uint32_t)data.size(), 0);		 
			textureData.texture.arrayCount = uint16_t(basis_transcoder.get_total_images(data.data(), (uint32_t)data.size()));
			uint16_t numImages = textureData.texture.arrayCount * textureData.texture.mipCount;
			std::vector < std::vector < uint8_t >> images;
			images.resize(textureData.texture.mipCount * textureData.texture.arrayCount);

			if (!basis_is_format_supported(basis_transcoder_textureFormat, fileinfo.m_tex_format))
			{
				TELEPORT_CERR << "Failed to transcode texture." << std::endl;
				return;
			}
			uint16_t imageIndex = 0;
			if (numImages != uint16_t(textureData.texture.arrayCount * textureData.texture.mipCount))
			{
				TELEPORT_CERR << "Failed to transcode texture." << std::endl;
				return;
			}
			size_t totalSize=0;
			std::vector<size_t> offsets;
			size_t offset = sizeof(uint16_t) + numImages*sizeof(size_t);
			for (uint32_t arrayIndex = 0; arrayIndex < textureData.texture.arrayCount; arrayIndex++)
			{
				for (uint32_t mipIndex = 0; mipIndex < textureData.texture.mipCount; mipIndex++)
				{
					uint32_t basisWidth, basisHeight, basisBlocks;

					basis_transcoder.get_image_level_desc(data.data(), (uint32_t)data.size(), arrayIndex, mipIndex, basisWidth, basisHeight, basisBlocks);
					uint32_t bytesPerPixel=basist::basis_get_bytes_per_block_or_pixel(basis_transcoder_textureFormat);
					uint32_t outDataSize = bytesPerPixel * basisWidth * basisHeight;
					auto &img = (images)[imageIndex];
					img.resize(outDataSize);
					if (!basis_transcoder.transcode_image_level(data.data(), (uint32_t)data.size(), arrayIndex, mipIndex, img.data(), basisWidth* basisHeight, basis_transcoder_textureFormat))
					{
						TELEPORT_CERR << "Texture failed to transcode mipmap level " << mipIndex << "." << std::endl;
					}
					offsets.push_back(offset);
					offset += outDataSize;
					imageIndex++;
					if(arrayIndex==0&&mipIndex==0)
					{
						textureData.texture.width = basisWidth;
						textureData.texture.height = basisHeight;
						textureData.texture.depth = 1;
					}
				}
			}
			textureData.texture.compressedData.resize(data.size());
			memcpy(textureData.texture.compressedData.data(),data.data(),data.size());
			textureData.texture.compression = avs::TextureCompression::BASIS_COMPRESSED;
			textureData.texture.format = avs::TextureFormat::RGBA8;
			textureData.texture.compressed = true;
			if(textureData.texture.compressedData.size()==0)
			{
				throw(std::runtime_error("No data in texture."));
			}
		}
	}
	else
	{
		TELEPORT_CERR << "Texture failed to start transcoding." << std::endl;

	}
}