// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd
#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "VideoDecoder.h"
#include "Common.h"
#include "Platform/CrossPlatform/Macros.h"
#include "Platform/CrossPlatform/Texture.h"
#include "AVParser/HevcParser.h"

#if TELEPORT_CLIENT_USE_D3D12
#include <dxva.h>
#include "Platform/DirectX12/VideoDecoder.h"
#else
#include "Platform/Vulkan/VideoDecoder.h"
#endif
#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"

using namespace avs;
using namespace avparser;
using namespace teleport;
using namespace clientrender;

namespace cp = platform::crossplatform;

VideoDecoder::VideoDecoder(cp::RenderPlatform* renderPlatform, cp::Texture* surfaceTexture)
	: mRenderPlatform(renderPlatform)
	, mSurfaceTexture(surfaceTexture)
	, mOutputTexture(nullptr)
	, mTextureConversionEffect(nullptr)
	, mCurrentFrame(0)
	, mStatusID(0)
{
	recompileShaders();
}

VideoDecoder::~VideoDecoder()
{
}

Result VideoDecoder::initialize(const DeviceHandle& device, int frameWidth, int frameHeight, const DecoderParams& params)
{
	if (device.type == DeviceType::Invalid)
	{
		TELEPORT_CERR << "VideoDecoder: Invalid device handle" << std::endl;
		return Result::DecoderBackend_InvalidDevice;
	}
	if (device.type != DeviceType::Direct3D12&&device.type!=DeviceType::Vulkan)
	{
		TELEPORT_CERR << "VideoDecoder: Platform only supports D3D12 and Vulkan video decoder currently." << std::endl;
		return Result::DecoderBackend_InvalidDevice;
	}
	if (params.codec == VideoCodec::Invalid)
	{
		TELEPORT_CERR << "VideoDecoder: Invalid video codec type" << std::endl;
		return Result::DecoderBackend_InvalidParam;
	}

	mDPB.clear();
	mPocFrameIndexMap.clear();

	clearDecodeArguments();
	mDecodeArgs.push_back({ cp::VideoDecodeArgumentType::PictureParameters, 0, nullptr });
	mDecodeArgs.push_back({ cp::VideoDecodeArgumentType::SliceControl, 0, nullptr });

	cp::VideoDecoderParams decParams;

	switch (params.codec)
	{
	case VideoCodec::H264:
		decParams.codec = cp::VideoCodec::H264;
		mDPB.resize(17);
		// TODO: Implement
		//mParser.reset(new h264::H264Parser());
		break;
	case VideoCodec::HEVC:
		decParams.codec = cp::VideoCodec::HEVC;
		mDPB.resize(16);
		mParser.reset(new hevc::HevcParser());
		break;
	default:
		TELEPORT_CERR << "VideoDecoder: Unsupported video codec type selected" << std::endl;
		return Result::DecoderBackend_CodecNotSupported;
	}

	decParams.decodeFormat = cp::PixelFormat::NV12;
	decParams.outputFormat = cp::PixelFormat::NV12;
	decParams.bitRate = 0;
	decParams.frameRate = 60;
	decParams.deinterlaceMode = cp::DeinterlaceMode::None;
	decParams.width = frameWidth;
	decParams.height = frameHeight;
	decParams.minWidth = frameWidth;
	decParams.minHeight = frameHeight;
	decParams.maxDecodePictureBufferCount =(uint32_t) mDPB.size();


	// The output texture is in native decode format.
	// The surface texture will be written to in the display function.
	mOutputTexture = mRenderPlatform->CreateTexture("OutputTexture");
	mOutputTexture->ensureTexture2DSizeAndFormat(mRenderPlatform, decParams.width, decParams.height, 1, platform::crossplatform::NV12, false, false, false);

#if TELEPORT_CLIENT_USE_D3D12
	mDecoder.reset(new platform::dx12::VideoDecoder());
	// Change to common state for use with D3D12 video decode command list.
	((platform::dx12::Texture*)mOutputTexture)->SetLayout(mRenderPlatform->GetImmediateContext(), D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON);
#else
	mDecoder.reset(new platform::vulkan::VideoDecoder());
	// Change to common state for use with D3D12 video decode command list.
//	((platform::vulkan::Texture*)mOutputTexture)->SetLayout(mRenderPlatform->GetImmediateContext(), D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON);
#endif

	// Pass true to perform a query to check if the decoder successfully decoded the frame.
	// Just use for debugging!
	if (DEC_FAILED(mDecoder->Initialize(mRenderPlatform, decParams, false)))
	{
		return Result::DecoderBackend_InitFailed;
	}

	mDeviceType = device.type;
	mParams = params;
	mFrameWidth = frameWidth;
	mFrameHeight = frameHeight;

	mCurrentFrame = 0;
	mStatusID = 0;

	return Result::OK;
}

Result VideoDecoder::reconfigure(int frameWidth, int frameHeight, const DecoderParams& params)
{
	if (!mDecoder)
	{
		TELEPORT_CERR << "VideoDecoder: Can't reconfigure because decoder not initialized";
		return Result::DecoderBackend_NotInitialized;
	}

	return Result::OK;
}

Result VideoDecoder::shutdown()
{
	mParams = {};
	mDeviceType = DeviceType::Invalid;
	mFrameWidth = 0;
	mFrameHeight = 0;
	mDisplayPictureIndex = -1;

	if (mDecoder)
	{
		if (DEC_FAILED(mDecoder->Shutdown()))
		{
			TELEPORT_CERR << "VideoDecoder: Failed to shut down the decoder";
			return Result::DecoderBackend_ShutdownFailed;
		}
	}

	mDecoder.reset();

	SAFE_DELETE(mOutputTexture);
	clearDecodeArguments();
	SAFE_DELETE(mTextureConversionEffect);

	return Result::OK;
}

Result VideoDecoder::registerSurface(const SurfaceBackendInterface* surface, const SurfaceBackendInterface* alphaSurface)
{
	if (!mDecoder)
	{
		TELEPORT_CERR << "VideoDecoder: Decoder not initialized";
		return Result::DecoderBackend_NotInitialized;
	}
	if (!surface || !surface->getResource())
	{
		TELEPORT_CERR << "VideoDecoder: Invalid surface handle";
		return Result::DecoderBackend_InvalidSurface;
	}
	if (surface->getWidth() != mFrameWidth || surface->getHeight() != mFrameHeight)
	{
		TELEPORT_CERR << "VideoDecoder: Output surface dimensions do not match video frame dimensions";
		return Result::DecoderBackend_InvalidSurface;
	}

	return Result::OK;
}

Result VideoDecoder::unregisterSurface()
{
	if (!mDecoder)
	{
		TELEPORT_CERR << "VideoDecoder: Decoder not initialized";
		return Result::DecoderBackend_NotInitialized;
	}

	return Result::OK;
}

Result VideoDecoder::decode(const void* buffer, size_t bufferSizeInBytes, const void* alphaBuffer, size_t alphaBufferSizeInBytes, VideoPayloadType payloadType, bool lastPayload)
{
	if (!mDecoder)
	{
		return Result::DecoderBackend_NotInitialized;
	}
	if (!buffer || bufferSizeInBytes == 0)
	{
		return Result::DecoderBackend_InvalidParam;
	}

	switch (payloadType)
	{
	case VideoPayloadType::VPS:
		// VPS not used by DXVA params for D3D12 Video Decoder.
		// TODO: Parse VPS if the picture parameters for the Vulkan decoder need it.
		return Result::OK;
	case VideoPayloadType::SPS:
	case VideoPayloadType::PPS:
	case VideoPayloadType::ALE:
	case VideoPayloadType::FirstVCL:
	case VideoPayloadType::VCL:
		break;
	default:
		return Result::DecoderBackend_InvalidPayload;
	}

	// The parse does not account for the ALU so skip it.
	size_t bytesParsed = mParser->parseNALUnit((uint8_t*)(buffer) + 3, bufferSizeInBytes - 3);

	if (!lastPayload)
	{
		return Result::OK;
	}

	// Include ALU size of 3 bytes.
	updateInputArguments(bytesParsed + 3);

	if (DEC_FAILED(mDecoder->Decode(mOutputTexture, buffer, bufferSizeInBytes, mDecodeArgs.data(),(uint32_t) mDecodeArgs.size())))
	{
		TELEPORT_CERR << "VideoDecoder: Error occurred while trying to decode the frame.";
		return Result::DecoderBackend_DecodeFailed;
	}

	return Result::DecoderBackend_ReadyToDisplay;
}

Result VideoDecoder::display(bool showAlphaAsColor)
{
	cp::GraphicsDeviceContext& deviceContext = mRenderPlatform->GetImmediateContext();
	
#if TELEPORT_CLIENT_USE_D3D12
	// Same texture. Two SRVs for two layers. D3D12 Texture class handles this.
	mTextureConversionEffect->SetTexture(deviceContext, "yTexture", mOutputTexture);
	mTextureConversionEffect->SetTexture(deviceContext, "uvTexture", mOutputTexture);
	mTextureConversionEffect->SetUnorderedAccessView(deviceContext, "rgbTexture", mSurfaceTexture);
	mTextureConversionEffect->Apply(deviceContext, "nv12_to_rgba", 0);
	mRenderPlatform->DispatchCompute(deviceContext, (mFrameWidth / 2) / 16, (mFrameHeight / 2) / 16, 1);
	mTextureConversionEffect->Unapply(deviceContext);
	mTextureConversionEffect->SetUnorderedAccessView(deviceContext, "rgbTexture", nullptr);
	mTextureConversionEffect->UnbindTextures(deviceContext);

	// Change back to common state for use with D3D12 video decode command list.
	((platform::dx12::Texture*)mOutputTexture)->SetLayout(deviceContext, D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON);
#endif

	return Result::OK;
}


void VideoDecoder::updateInputArguments(size_t sliceControlSize)
{
	switch (mParams.codec)
	{
	case VideoCodec::H264:
		updateInputArgumentsH264(sliceControlSize);
		break;
	case VideoCodec::HEVC:
		updateInputArgumentsHEVC(sliceControlSize);
		break;
	}

	mCurrentFrame = (mCurrentFrame + 1) % (uint32_t)mDPB.size();
}

void VideoDecoder::updateInputArgumentsH264(size_t sliceControlSize)
{
	cp::VideoDecodeArgument& picParams = mDecodeArgs[0];
	cp::VideoDecodeArgument& sliceControl = mDecodeArgs[1];

	if (!picParams.data)
	{
#if TELEPORT_CLIENT_USE_D3D12
		picParams.size = sizeof(DXVA_PicParams_H264);
		picParams.data = new DXVA_PicParams_H264;

		sliceControl.size = sizeof(DXVA_Slice_H264_Short);
		sliceControl.data = new DXVA_Slice_H264_Short;
#endif
	}

#if TELEPORT_CLIENT_USE_D3D12
	DXVA_PicParams_H264* pp = (DXVA_PicParams_H264*)picParams.data;
	DXVA_Slice_H264_Short* sc = (DXVA_Slice_H264_Short*)sliceControl.data;
	// TODO: Implement
#endif
}

void VideoDecoder::updateInputArgumentsHEVC(size_t sliceControlSize)
{
	cp::VideoDecodeArgument& picParams = mDecodeArgs[0];
	cp::VideoDecodeArgument& sliceControl = mDecodeArgs[1];

	// Commenting out as not used by DXVA.
	//const hevc::VPS* vps = (hevc::VPS*)mParser->getVPS();
	const hevc::SPS* sps = (hevc::SPS*)mParser->getSPS();
	const hevc::PPS* pps = (hevc::PPS*)mParser->getPPS();
	const hevc::Slice* slice = (hevc::Slice*)mParser->getLastSlice();
	const hevc::ExtraData* extraData = (hevc::ExtraData*)mParser->getExtraData();

	const hevc::ShortTermRefPicSet* strps = nullptr;

	if (slice->short_term_ref_pic_set_sps_flag)
	{
		strps = &sps->short_term_ref_pic_set[extraData->refRpsIdx];
	}
	else
	{
		strps = &slice->short_term_ref_pic_set;
	}


	if (!picParams.data)
	{
#if TELEPORT_CLIENT_USE_D3D12
		picParams.size = sizeof(DXVA_PicParams_HEVC);
		picParams.data = new DXVA_PicParams_HEVC;

		sliceControl.size = sizeof(DXVA_Slice_HEVC_Short);
		sliceControl.data = new DXVA_Slice_HEVC_Short;
#endif
	}
	else
	{
		if (IS_HEVC_IDR(slice)) 
		{
			resetFrames();
		}
		else
		{
			markFramesUnusedForReference();
		}
	}
	
#if TELEPORT_CLIENT_USE_D3D12
	memset(picParams.data, 0, sizeof(DXVA_PicParams_HEVC));
	#endif
	FrameCache& frame = mDPB[mCurrentFrame];
	frame.reset();
	frame.stRpsIdx = extraData->stRpsIdx;
	frame.refRpsIdx = extraData->refRpsIdx;
	frame.sliceType = slice->slice_type;
	frame.poc = extraData->poc;

#if TELEPORT_CLIENT_USE_D3D12
	DXVA_PicParams_HEVC* pp = (DXVA_PicParams_HEVC*)picParams.data;
	DXVA_Slice_HEVC_Short* sc = (DXVA_Slice_HEVC_Short*)sliceControl.data;

	pp->PicWidthInMinCbsY = sps->pic_width_in_luma_samples >> (sps->log2_min_luma_coding_block_size_minus3 + 3);
	pp->PicHeightInMinCbsY = sps->pic_height_in_luma_samples >> (sps->log2_min_luma_coding_block_size_minus3 + 3);

	pp->wFormatAndSequenceInfoFlags =
		(sps->chroma_format_idc << 0) |
		(sps->separate_colour_plane_flag << 2) |
		(sps->bit_depth_luma_minus8 << 3) |
		(sps->bit_depth_chroma_minus8 << 6) |
		(sps->log2_max_pic_order_cnt_lsb_minus4 << 9) |
		(0 << 13) |
		(0 << 14) |
		(0 << 15);

	pp->CurrPic.bPicEntry = mCurrentFrame | (0 << 7);

	pp->sps_max_dec_pic_buffering_minus1 = sps->sps_max_dec_pic_buffering_minus1[sps->sps_max_sub_layers_minus1];
	pp->log2_min_luma_coding_block_size_minus3 = sps->log2_min_luma_coding_block_size_minus3;
	pp->log2_diff_max_min_luma_coding_block_size = sps->log2_diff_max_min_luma_coding_block_size;
	pp->log2_min_transform_block_size_minus2 = sps->log2_min_transform_block_size_minus2;
	pp->log2_diff_max_min_transform_block_size = sps->log2_diff_max_min_transform_block_size;
	pp->max_transform_hierarchy_depth_inter = sps->max_transform_hierarchy_depth_inter;
	pp->max_transform_hierarchy_depth_intra = sps->max_transform_hierarchy_depth_intra;
	pp->num_short_term_ref_pic_sets = sps->num_short_term_ref_pic_sets;
	pp->num_long_term_ref_pics_sps = sps->num_long_term_ref_pics_sps;

	pp->num_ref_idx_l0_default_active_minus1 = pps->num_ref_idx_l0_default_active_minus1;
	pp->num_ref_idx_l1_default_active_minus1 = pps->num_ref_idx_l1_default_active_minus1;
	pp->init_qp_minus26 = pps->init_qp_minus26;

	if (slice->short_term_ref_pic_set_sps_flag == 0)
	{
		pp->ucNumDeltaPocsOfRefRpsIdx = extraData->numDeltaPocsOfRefRpsIdx;
		pp->wNumBitsForShortTermRPSInSlice = extraData->short_term_ref_pic_set_size;
	}
	else
	{
		pp->ucNumDeltaPocsOfRefRpsIdx = 0;
		pp->wNumBitsForShortTermRPSInSlice = 0;
	}

	pp->dwCodingParamToolFlags = (sps->scaling_list_enabled_flag << 0) |
		(sps->amp_enabled_flag << 1) |
		(sps->sample_adaptive_offset_enabled_flag << 2) |
		(sps->pcm_enabled_flag << 3) |
		((sps->pcm_enabled_flag ? sps->pcm_sample_bit_depth_luma_minus1 : 0) << 4) |
		((sps->pcm_enabled_flag ? sps->pcm_sample_bit_depth_chroma_minus1 : 0) << 8) |
		((sps->pcm_enabled_flag ? sps->log2_min_pcm_luma_coding_block_size_minus3 : 0) << 12) |
		((sps->pcm_enabled_flag ? sps->log2_diff_max_min_pcm_luma_coding_block_size : 0) << 14) |
		(sps->pcm_loop_filter_disabled_flag << 16) |
		(sps->long_term_ref_pics_present_flag << 17) |
		(sps->sps_temporal_mvp_enabled_flag << 18) |
		(sps->strong_intra_smoothing_enabled_flag << 19) |
		(pps->dependent_slice_segments_enabled_flag << 20) |
		(pps->output_flag_present_flag << 21) |
		(pps->num_extra_slice_header_bits << 22) |
		(pps->sign_data_hiding_flag << 25) |
		(pps->cabac_init_present_flag << 26) |
		(0 << 27);

	pp->dwCodingSettingPicturePropertyFlags = (pps->constrained_intra_pred_flag << 0) |
		(pps->transform_skip_enabled_flag << 1) |
		(pps->cu_qp_delta_enabled_flag << 2) |
		(pps->pps_slice_chroma_qp_offsets_present_flag << 3) |
		(pps->weighted_pred_flag << 4) |
		(pps->weighted_bipred_flag << 5) |
		(pps->transquant_bypass_enabled_flag << 6) |
		(pps->tiles_enabled_flag << 7) |
		(pps->entropy_coding_sync_enabled_flag << 8) |
		(pps->uniform_spacing_flag << 9) |
		((pps->tiles_enabled_flag ? pps->loop_filter_across_tiles_enabled_flag : 0) << 10) |
		(pps->pps_loop_filter_across_slices_enabled_flag << 11) |
		(pps->deblocking_filter_override_enabled_flag << 12) |
		(pps->pps_deblocking_filter_disabled_flag << 13) |
		(pps->lists_modification_present_flag << 14) |
		(pps->slice_segment_header_extension_present_flag << 15) |
		(IS_HEVC_IRAP(slice) << 16) |
		(IS_HEVC_IDR(slice) << 17) |
		// IntraPicFlag 
		(IS_HEVC_IRAP(slice) << 18) |
		(0 << 19);

	pp->pps_cb_qp_offset = pps->pps_cb_qp_offset;
	pp->pps_cr_qp_offset = pps->pps_cr_qp_offset;
	if (pps->tiles_enabled_flag)
	{
		pp->num_tile_columns_minus1 = pps->num_tile_columns_minus1;
		pp->num_tile_rows_minus1 = pps->num_tile_rows_minus1;

		if (!pps->uniform_spacing_flag)
		{
			for (uint32_t i = 0; i <= pps->num_tile_columns_minus1; ++i)
			{
				pp->column_width_minus1[i] = pps->column_width_minus1[i];
			}

			for (uint32_t i = 0; i <= pps->num_tile_rows_minus1; ++i)
			{
				pp->row_height_minus1[i] = pps->row_height_minus1[i];
			}
		}
	}

	pp->diff_cu_qp_delta_depth = pps->diff_cu_qp_delta_depth;
	pp->pps_beta_offset_div2 = pps->pps_beta_offset_div2;
	pp->pps_tc_offset_div2 = pps->pps_tc_offset_div2;
	pp->log2_parallel_merge_level_minus2 = pps->log2_parallel_merge_level_minus2;

	pp->CurrPicOrderCntVal = frame.poc;

	// Fill short term lists.
	int picSetIndex = 0;
	uint32_t prevPocDiff = 0;
	for (uint32_t i = 0; i < strps->num_negative_pics; ++i)
	{
		uint32_t pocDiff = prevPocDiff + strps->delta_poc_s0_minus1[i] + 1;
		uint32_t poc = frame.poc - pocDiff;
		if (strps->used_by_curr_pic_s0_flag[i] && (mPocFrameIndexMap.find(poc) != mPocFrameIndexMap.end()))
		{	
			mDPB[mPocFrameIndexMap[poc]].usedForShortTermRef = true;
			uint32_t refListindex = mPocFrameIndexMap[poc];
			if (refListindex > mCurrentFrame)
			{
				refListindex--;
			}
			pp->RefPicSetStCurrBefore[picSetIndex++] = refListindex;
		}	
		prevPocDiff = pocDiff;
	}

	for (; picSetIndex < 8; ++picSetIndex)
	{
		pp->RefPicSetStCurrBefore[picSetIndex] = 0xff;
	}

	picSetIndex = 0;
	prevPocDiff = 0;
	for (uint32_t i = 0; i < strps->num_positive_pics; ++i)
	{
		uint32_t pocDiff = prevPocDiff + strps->delta_poc_s1_minus1[i] + 1;
		uint32_t poc = frame.poc + pocDiff;
		if (strps->used_by_curr_pic_s1_flag[i] && (mPocFrameIndexMap.find(poc) != mPocFrameIndexMap.end()))
		{
			mDPB[mPocFrameIndexMap[poc]].usedForShortTermRef = true;
			uint32_t refListindex = mPocFrameIndexMap[poc];
			if (refListindex > mCurrentFrame)
			{
				refListindex--;
			}
			pp->RefPicSetStCurrAfter[picSetIndex++] = refListindex;
		}
		prevPocDiff = pocDiff;
	}

	for (; picSetIndex < 8; ++picSetIndex)
	{
		pp->RefPicSetStCurrAfter[picSetIndex] = 0xff;
	}
	
	// Fill long term lists.
	picSetIndex = 0;
	for (int i = 0; i < slice->used_by_curr_pic_lt_flag.size(); ++i)
	{
		if (slice->used_by_curr_pic_lt_flag[i])
		{
			uint32_t poc = extraData->longTermRefPicPocs[i];
			mDPB[mPocFrameIndexMap[poc]].usedForLongTermRef = true;
			uint32_t refListindex = mPocFrameIndexMap[poc];
			if (refListindex > mCurrentFrame)
			{
				refListindex--;
			}
			pp->RefPicSetLtCurr[picSetIndex++] = refListindex;
		}
	}
	for (; picSetIndex < 8; ++picSetIndex)
	{
		pp->RefPicSetLtCurr[picSetIndex] = 0xff;
	}
	
	for (uint32_t i = 0, j = 0; i < mDPB.size(); ++i)
	{
		if (i == mCurrentFrame)
		{
			continue;
		}
		if (mDPB[i].usedForShortTermRef || mDPB[i].usedForLongTermRef)
		{
			pp->RefPicList[j].Index7Bits = i;
			pp->RefPicList[j].AssociatedFlag = mDPB[i].usedForLongTermRef;
			pp->PicOrderCntValList[j] = mDPB[i].poc;
		}
		else
		{
			pp->RefPicList[j].bPicEntry = 0xff;
			pp->PicOrderCntValList[j] = 0;
		}

		++j;
	}

	pp->StatusReportFeedbackNumber = mStatusID++;


	// Slice control
	sc->BSNALunitDataLocation = 0;
	sc->SliceBytesInBuffer = static_cast<uint32_t>(sliceControlSize);
	sc->wBadSliceChopping = 0;

	mPocFrameIndexMap[frame.poc] = mCurrentFrame;

#endif
}

void VideoDecoder::resetFrames()
{
	mPocFrameIndexMap.clear();
	for (auto& frame : mDPB)
	{
		frame.reset();
	}
	mCurrentFrame = 0;
}

void VideoDecoder::markFramesUnusedForReference()
{
	for (auto& frame : mDPB)
	{
		frame.markUnusedForReference();
	}
}

void VideoDecoder::clearDecodeArguments()
{
	for (auto& arg : mDecodeArgs)
	{
		delete[] arg.data;
		arg.data=nullptr;
	}
	mDecodeArgs.clear();
}

void VideoDecoder::recompileShaders()
{
	SAFE_DELETE(mTextureConversionEffect);
	mTextureConversionEffect = mRenderPlatform->CreateEffect("texture_conversion");
}


