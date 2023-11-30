#include "Texture.h"
#define BASISU_FORCE_DEVEL_MESSAGES 1
#include "basisu_comp.h"
#include "TeleportCore/ErrorHandling.h"
#include "GeometryStore.h"
#include <filesystem>

using namespace teleport;
using namespace server;
using namespace std::filesystem;

bool teleport::server::ApplyBasisCompression(ExtractedTexture &extractedTexture, std::shared_ptr<PrecompressedTexture> compressionData
											,uint8_t compressionStrength,uint8_t compressionQuality)
{
	GeometryStore &geometryStore = GeometryStore::GetInstance();
	avs::Texture &avsTexture = extractedTexture.texture;
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
	std::string file_name = (geometryStore.GetCachePath() + "/") + extractedTexture.MakeFilename();
	if (breakout)
		return false;
	if (compressionData->highQualityUASTC)
	{
		compressionData->highQualityUASTC = false;
		TELEPORT_CERR << "highQualityUASTC is not functional in Basis. Reverting to low-quality mode.\n";
	}
	// TODO: This doesn't work for mips>0. So can't flip textures from Unity for example.
	// basisCompressorParams.m_y_flip=true;
	basisCompressorParams.m_quality_level = compressionQuality;
	basisCompressorParams.m_compression_level = compressionStrength;

	basisCompressorParams.m_write_output_basis_files = false;
	basisCompressorParams.m_create_ktx2_file = false;
	basisCompressorParams.m_out_filename = (geometryStore.GetCachePath() + "/") + extractedTexture.path+ ".basis";
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
	if (ok)
	{
		basisu::basis_compressor::error_code result = basisCompressor.process();
		if (result == basisu::basis_compressor::error_code::cECSuccess)
		{
			basisu::uint8_vec basisTex = basisCompressor.get_output_basis_file();
			avsTexture.data.resize(basisCompressor.get_basis_file_size());
			unsigned char *target = avsTexture.data.data();
			memcpy(target, basisTex.data(), avsTexture.data.size());
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
	geometryStore.saveResourceBinary(file_name, extractedTexture);
	
	return true;
}
bool BasisValidate(basist::basisu_transcoder &dec, basist::basisu_file_info &fileinfo, const std::vector< char> &data)
{
#ifndef __ANDROID__

	TELEPORT_ASSERT(fileinfo.m_total_images == fileinfo.m_image_mipmap_levels.size());
	TELEPORT_ASSERT(fileinfo.m_total_images == dec.get_total_images(data.data(), (uint32_t)data.size()));

	TELEPORT_COUT<<"File info: "<<"\n";
	TELEPORT_COUT<<"  Version: "<< fileinfo.m_version<<"\n";
	TELEPORT_COUT<<"  Total header size: "<< fileinfo.m_total_header_size<<"\n";
	TELEPORT_COUT<<"  Total selectors: "<< fileinfo.m_total_selectors<<"\n";
	TELEPORT_COUT<<"  Selector codebook size: "<< fileinfo.m_selector_codebook_size<<"\n";
	TELEPORT_COUT<<"  Total endpoints: "<< fileinfo.m_total_endpoints<<"\n";
	TELEPORT_COUT<<"  Endpoint codebook size: "<< fileinfo.m_endpoint_codebook_size<<"\n";
	TELEPORT_COUT<<"  Tables size: "<< fileinfo.m_tables_size<<"\n";
	TELEPORT_COUT<<"  Slices size: "<< fileinfo.m_slices_size<<"\n";
	TELEPORT_COUT<<"  Texture format: "<<((fileinfo.m_tex_format == basist::basis_tex_format::cUASTC4x4) ? "UASTC" : "ETC1S")<<"\n";
	TELEPORT_COUT<<"  Texture type: "<< basist::basis_get_texture_type_name(fileinfo.m_tex_type)<<"\n";
	TELEPORT_COUT<<"  us per frame: "<< fileinfo.m_us_per_frame<< (fileinfo.m_us_per_frame ? (1.0f / ((float)fileinfo.m_us_per_frame / 1000000.0f)) : 0.0f)<<"\n";
	TELEPORT_COUT<<"  Total slices: "<< (uint32_t)fileinfo.m_slice_info.size()<<"\n";
	TELEPORT_COUT<<"  Total images: "<< fileinfo.m_total_images<<"\n";
	TELEPORT_COUT << "  Y Flipped: " << fileinfo.m_y_flipped << ", Has alpha slices: "<< fileinfo.m_has_alpha_slices << "\n";
	TELEPORT_COUT << "  userdata0: " << fileinfo.m_userdata0 <<" userdata1: 0x%X\n"<< fileinfo.m_userdata1 << "\n";
	TELEPORT_COUT<<"  Per-image mipmap levels: "<<"\n";
	for (uint32_t i = 0; i < fileinfo.m_total_images; i++)
		TELEPORT_COUT<<"%u "<< fileinfo.m_image_mipmap_levels[i]<<"\n";
	TELEPORT_COUT<<"\n"<<"\n";

	uint32_t total_texels = 0;

	TELEPORT_COUT<<"\nImage info:\n"<<"\n";
	for (uint32_t i = 0; i < fileinfo.m_total_images; i++)
	{
		basist::basisu_image_info ii;
		if (!dec.get_image_info(data.data(), (uint32_t)data.size(), ii, i))
		{
			TELEPORT_COUT<<"get_image_info() failed!\n"<<"\n";
			return false;
		}

		TELEPORT_INTERNAL_CERR("Image {}: MipLevels: {} OrigDim: {}{}, BlockDim: {}{}, FirstSlice: {}, HasAlpha: {}\n", i, ii.m_total_levels, ii.m_orig_width, ii.m_orig_height,
			   ii.m_num_blocks_x, ii.m_num_blocks_y, ii.m_first_slice_index, (uint32_t)ii.m_alpha_flag);

		total_texels += ii.m_width * ii.m_height;
	}

	TELEPORT_COUT<<"\nSlice info:\n"<<"\n";

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
		TELEPORT_COUT<<s<<"\n";
		}
		catch(...)
		{
		}
	}
	TELEPORT_COUT<<"\n"<<"\n";

	const float basis_bits_per_texel = data.size() * 8.0f / total_texels;
	// const float comp_bits_per_texel = comp_size * 8.0f / total_texels;

	// TELEPORT_COUT<<"Original size: %u, bits per texel: %3.3f\nCompressed size (Deflate): %u, bits per texel: %3.3f\n", (uint32_t)basis_file_data.size(), basis_bits_per_texel, (uint32_t)comp_size, comp_bits_per_texel<<"\n";
#endif
	return true;
}
void teleport::server::LoadAsBasisFile( ExtractedTexture &textureData, const std::vector<char> &data,const std::string &filename)
{
	GeometryStore &geometryStore=GeometryStore::GetInstance();
	path cachePath = geometryStore.GetCachePath();
	path filename_path = path(filename).replace_extension("");
	path relative_path = filename_path.lexically_relative(cachePath);
	textureData.path = relative_path.generic_string();
	textureData.texture.name = relative_path.filename().generic_u8string();
	size_t hash_pos = textureData.texture.name.rfind('#');
	if(hash_pos<textureData.texture.name.length())
		textureData.texture.name = textureData.texture.name.substr(hash_pos+1,textureData.texture.name.length()-hash_pos-1);
		
	//textureData.fileExtension = relative_path.extension().generic_u8string();
	std::filesystem::file_time_type rawFileTime = std::filesystem::last_write_time(filename);
	textureData.lastModified = rawFileTime.time_since_epoch().count();
	
	// We need a new transcoder for every .basis file.
	basist::basisu_transcoder basis_transcoder;
	
	basist::basisu_file_info fileinfo;
	if (!basis_transcoder.get_file_info(data.data(), (uint32_t)data.size(), fileinfo))
	{
		TELEPORT_CERR << "Failed to transcode texture " << textureData.texture.name << std::endl;
		return;
	}
	static bool basis_transcoder_initialized=false;
	if (!basis_transcoder_initialized)
	{
		basist::basisu_transcoder_init();
		basis_transcoder_initialized=true;
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
				textureData.texture.bytesPerPixel=basist::basis_get_bytes_per_block_or_pixel(basis_transcoder_textureFormat);
				uint32_t outDataSize = textureData.texture.bytesPerPixel * basisWidth * basisHeight;
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
		textureData.texture.data.resize(data.size());
		memcpy(textureData.texture.data.data(),data.data(),data.size());
		textureData.texture.compression = avs::TextureCompression::BASIS_COMPRESSED;
		textureData.texture.format = avs::TextureFormat::RGBA8;
		textureData.texture.compressed = true;
		
	}
	else
	{
		TELEPORT_CERR << "Texture failed to start transcoding." << std::endl;
	}
}