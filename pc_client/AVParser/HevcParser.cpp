// (C) Copyright 2018-2022 Simul Software Ltd

#include "HevcParser.h"

#include <iostream>
#include <string>

#include <sstream>

#include <assert.h>
#include "ErrorHandling.h"

namespace avparser
{
	namespace hevc
	{
		HevcParser::HevcParser()
			: mPrevPocTid0(0)
		{
			mReader.reset(new BitReader());
		}

		
		size_t HevcParser::parseNALUnit(const uint8_t* data, size_t size)
		{
			mReader->start(data, size);

			NALHeader header;
			parseNALUnitHeader(header);

			switch (header.type)
			{
			case NALUnitType::VPS:
			{
				mVPS = {};
				mVPS.header = header;
				parseVPS(mVPS);
				break;
			}

			case NALUnitType::SPS:
			{
				mSPS = {};
				mSPS.header = header;
				parseSPS(mSPS);
				break;
			}

			case NALUnitType::PPS:
			{
				mPPS = {};
				mPPS.header = header;
				parsePPS(mPPS);
				break;
			}

			case NALUnitType::TRAIL_R:
			case NALUnitType::TRAIL_N:
			case NALUnitType::TSA_N:
			case NALUnitType::TSA_R:
			case NALUnitType::STSA_N:
			case NALUnitType::STSA_R:
			case NALUnitType::BLA_W_LP:
			case NALUnitType::BLA_W_RADL:
			case NALUnitType::BLA_N_LP:
			case NALUnitType::IDR_W_RADL:
			case NALUnitType::IDR_N_LP:
			case NALUnitType::CRA_NUT:
			case NALUnitType::RADL_N:
			case NALUnitType::RADL_R:
			case NALUnitType::RASL_N:
			case NALUnitType::RASL_R:
			{
				mLastSlice = Slice();
				mLastSlice.header = header;
				mExtraData = {};
				parseSliceHeader(mLastSlice);
				break;
			}
			default:
				return 0;
			};

			return mReader->getBytesRead();
		}


		void HevcParser::parseNALUnitHeader(NALHeader& header)
		{
			// forbidden_zero_bit
			mReader->getBit();

			header.type = (NALUnitType)mReader->getBits(6);

			// nuh_layer_id
			header.layer_id = mReader->getBits(6);

			// nuh_temporal_id_plus1
			header.temporal_id_plus1 = mReader->getBits(3);
		}

		void HevcParser::parseVPS(VPS& vps)
		{
			vps.vps_video_parameter_set_id = mReader->getBits(4);
			mReader->getBits(2);
			vps.vps_max_layers_minus1 = mReader->getBits(6);
			vps.vps_max_sub_layers_minus1 = mReader->getBits(3);
			vps.vps_temporal_id_nesting_flag = mReader->getBits(1);
			mReader->getBits(16);

			vps.profile_tier_level = parseProfileTierLevel(vps.vps_max_sub_layers_minus1);

			vps.vps_sub_layer_ordering_info_present_flag = mReader->getBits(1);

			vps.vps_max_dec_pic_buffering_minus1.resize(vps.vps_max_sub_layers_minus1 + 1, 0);
			vps.vps_max_num_reorder_pics.resize(vps.vps_max_sub_layers_minus1 + 1, 0);
			vps.vps_max_latency_increase_plus1.resize(vps.vps_max_sub_layers_minus1 + 1, 0);


			for (uint32_t i = (vps.vps_sub_layer_ordering_info_present_flag ? 0 : vps.vps_max_sub_layers_minus1); i <= vps.vps_max_sub_layers_minus1; ++i)
			{
				vps.vps_max_dec_pic_buffering_minus1[i] = mReader->getGolombU();
				vps.vps_max_num_reorder_pics[i] = mReader->getGolombU();
				vps.vps_max_latency_increase_plus1[i] = mReader->getGolombU();
			}

			vps.vps_max_layer_id = mReader->getBits(6);
			vps.vps_num_layer_sets_minus1 = mReader->getGolombU();

			vps.layer_id_included_flag.resize(vps.vps_num_layer_sets_minus1 + 1);

			for (uint32_t i = 1; i <= vps.vps_num_layer_sets_minus1; ++i)
			{
				vps.layer_id_included_flag[i].resize(vps.vps_max_layer_id + 1);
				for (uint32_t j = 0; j <= vps.vps_max_layer_id; ++j)
				{
					(vps.layer_id_included_flag[i])[j] = mReader->getBits(1);
				}
			}

			vps.vps_timing_info_present_flag = mReader->getBits(1);
			if (vps.vps_timing_info_present_flag)
			{
				vps.vps_num_units_in_tick = mReader->getBits(32);
				vps.vps_time_scale = mReader->getBits(32);
				vps.vps_poc_proportional_to_timing_flag = mReader->getBits(1);

				if (vps.vps_poc_proportional_to_timing_flag)
				{
					vps.vps_num_ticks_poc_diff_one_minus1 = mReader->getGolombU();
				}
				vps.vps_num_hrd_parameters = mReader->getGolombU();

				if (vps.vps_num_hrd_parameters > 0)
				{
					vps.hrd_layer_set_idx.resize(vps.vps_num_hrd_parameters);
					vps.cprms_present_flag.resize(vps.vps_num_hrd_parameters);
					vps.cprms_present_flag[0] = 1;

					for (uint32_t i = 0; i < vps.vps_num_hrd_parameters; ++i)
					{
						vps.hrd_layer_set_idx[i] = mReader->getGolombU();

						if (i)
						{
							vps.cprms_present_flag[i] = mReader->getBits(1);
						}
						HrdParameters hrdParams = parseHrdParameters(vps.cprms_present_flag[i], vps.vps_max_sub_layers_minus1);
						vps.hrd_parameters.emplace_back(hrdParams);
					}
				}
			}

			vps.vps_extension_flag = mReader->getBits(1);
		}


		void HevcParser::parseSPS(SPS& sps)
		{
			sps.sps_video_parameter_set_id = mReader->getBits(4);
			sps.sps_max_sub_layers_minus1 = mReader->getBits(3);
			sps.sps_temporal_id_nesting_flag = mReader->getBits(1);
			sps.profile_tier_level = parseProfileTierLevel(sps.sps_max_sub_layers_minus1);

			sps.sps_seq_parameter_set_id = mReader->getGolombU();
			//  sps.sps_seq_parameter_set_id = 0;
			sps.chroma_format_idc = mReader->getGolombU();

			if (sps.chroma_format_idc == 3)
			{
				sps.separate_colour_plane_flag = mReader->getBits(1);
			}
			else
			{
				sps.separate_colour_plane_flag = 0;
			}

			sps.pic_width_in_luma_samples = mReader->getGolombU();
			sps.pic_height_in_luma_samples = mReader->getGolombU();
			sps.conformance_window_flag = mReader->getBits(1);

			if (sps.conformance_window_flag)
			{
				sps.conf_win_left_offset = mReader->getGolombU();
				sps.conf_win_right_offset = mReader->getGolombU();
				sps.conf_win_top_offset = mReader->getGolombU();
				sps.conf_win_bottom_offset = mReader->getGolombU();
			}

			sps.bit_depth_luma_minus8 = mReader->getGolombU();
			sps.bit_depth_chroma_minus8 = mReader->getGolombU();
			sps.log2_max_pic_order_cnt_lsb_minus4 = mReader->getGolombU();
			sps.sps_sub_layer_ordering_info_present_flag = mReader->getBits(1);

			sps.sps_max_dec_pic_buffering_minus1.resize(sps.sps_max_sub_layers_minus1 + 1, 0);
			sps.sps_max_num_reorder_pics.resize(sps.sps_max_sub_layers_minus1 + 1, 0);
			sps.sps_max_latency_increase_plus1.resize(sps.sps_max_sub_layers_minus1 + 1, 0);

			for (size_t i = (sps.sps_sub_layer_ordering_info_present_flag ? 0 : sps.sps_max_sub_layers_minus1); i <= sps.sps_max_sub_layers_minus1;++i)
			{
				sps.sps_max_dec_pic_buffering_minus1[i] = mReader->getGolombU();
				sps.sps_max_num_reorder_pics[i] = mReader->getGolombU();
				sps.sps_max_latency_increase_plus1[i] = mReader->getGolombU();
			}

			sps.log2_min_luma_coding_block_size_minus3 = mReader->getGolombU();
			sps.log2_diff_max_min_luma_coding_block_size = mReader->getGolombU();
			sps.log2_min_transform_block_size_minus2 = mReader->getGolombU();
			sps.log2_diff_max_min_transform_block_size = mReader->getGolombU();
			sps.max_transform_hierarchy_depth_inter = mReader->getGolombU();
			sps.max_transform_hierarchy_depth_intra = mReader->getGolombU();

			sps.scaling_list_enabled_flag = mReader->getBits(1);
			if (sps.scaling_list_enabled_flag)
			{
				sps.sps_scaling_list_data_present_flag = mReader->getBits(1);
				if (sps.sps_scaling_list_data_present_flag)
				{
					sps.scaling_list_data = parseScalingListData();
				}
			}

			sps.amp_enabled_flag = mReader->getBits(1);
			sps.sample_adaptive_offset_enabled_flag = mReader->getBits(1);
			sps.pcm_enabled_flag = mReader->getBits(1);

			if (sps.pcm_enabled_flag)
			{
				sps.pcm_sample_bit_depth_luma_minus1 = mReader->getBits(4);
				sps.pcm_sample_bit_depth_chroma_minus1 = mReader->getBits(4);
				sps.log2_min_pcm_luma_coding_block_size_minus3 = mReader->getGolombU();
				sps.log2_diff_max_min_pcm_luma_coding_block_size = mReader->getGolombU();
				sps.pcm_loop_filter_disabled_flag = mReader->getBits(1);
			}

			sps.num_short_term_ref_pic_sets = mReader->getGolombU();

			sps.short_term_ref_pic_set.resize(sps.num_short_term_ref_pic_sets);
			uint32_t refRpsIdx = 0;
			for (uint32_t i = 0; i < sps.num_short_term_ref_pic_sets; ++i)
			{
				sps.short_term_ref_pic_set[i] = parseShortTermRefPicSet(i, sps.num_short_term_ref_pic_sets, sps.short_term_ref_pic_set, sps, refRpsIdx);
			}

			sps.long_term_ref_pics_present_flag = mReader->getBits(1);
			if (sps.long_term_ref_pics_present_flag)
			{
				sps.num_long_term_ref_pics_sps = mReader->getGolombU();
				sps.lt_ref_pic_poc_lsb_sps.resize(sps.num_long_term_ref_pics_sps);
				sps.used_by_curr_pic_lt_sps_flag.resize(sps.num_long_term_ref_pics_sps);

				for (size_t i = 0; i < sps.num_long_term_ref_pics_sps; ++i)
				{
					sps.lt_ref_pic_poc_lsb_sps[i] = mReader->getBits(sps.log2_max_pic_order_cnt_lsb_minus4 + 4);
					sps.used_by_curr_pic_lt_sps_flag[i] = mReader->getBits(1);
				}
			}

			sps.sps_temporal_mvp_enabled_flag = mReader->getBits(1);
			sps.strong_intra_smoothing_enabled_flag = mReader->getBits(1);
			sps.vui_parameters_present_flag = mReader->getBits(1);

			if (sps.vui_parameters_present_flag)
			{
				sps.vui_parameters = parseVuiParameters(sps.sps_max_sub_layers_minus1);
			}

			sps.sps_extension_flag = mReader->getBits(1);
		}

		ProfileTierLevel HevcParser::parseProfileTierLevel(size_t max_sub_layers_minus1)
		{
			ProfileTierLevel ptl;

			ptl.general_profile_space = mReader->getBits(2);
			ptl.general_tier_flag = mReader->getBits(1);
			ptl.general_profile_idc = mReader->getBits(5);

			for (size_t i = 0; i < 32; ++i)
			{
				ptl.general_profile_compatibility_flag[i] = mReader->getBits(1);
			}

			ptl.general_progressive_source_flag = mReader->getBits(1);
			ptl.general_interlaced_source_flag = mReader->getBits(1);
			ptl.general_non_packed_constraint_flag = mReader->getBits(1);
			ptl.general_frame_only_constraint_flag = mReader->getBits(1);
			mReader->getBits(32);
			mReader->getBits(12);
			ptl.general_level_idc = mReader->getBits(8);

			ptl.sub_layer_profile_present_flag.resize(max_sub_layers_minus1);
			ptl.sub_layer_level_present_flag.resize(max_sub_layers_minus1);

			for (size_t i = 0; i < max_sub_layers_minus1; ++i)
			{
				ptl.sub_layer_profile_present_flag[i] = mReader->getBits(1);
				ptl.sub_layer_level_present_flag[i] = mReader->getBits(1);
			}


			if (max_sub_layers_minus1 > 0)
			{
				for (size_t i = max_sub_layers_minus1; i < 8; ++i)
				{
					mReader->getBits(2);
				}
			}

			ptl.sub_layer_profile_space.resize(max_sub_layers_minus1);
			ptl.sub_layer_tier_flag.resize(max_sub_layers_minus1);
			ptl.sub_layer_profile_idc.resize(max_sub_layers_minus1);
			ptl.sub_layer_profile_compatibility_flag.resize(max_sub_layers_minus1);
			ptl.sub_layer_progressive_source_flag.resize(max_sub_layers_minus1);
			ptl.sub_layer_interlaced_source_flag.resize(max_sub_layers_minus1);
			ptl.sub_layer_non_packed_constraint_flag.resize(max_sub_layers_minus1);
			ptl.sub_layer_frame_only_constraint_flag.resize(max_sub_layers_minus1);
			ptl.sub_layer_level_idc.resize(max_sub_layers_minus1);

			for (size_t i = 0; i < max_sub_layers_minus1; ++i)
			{
				if (ptl.sub_layer_profile_present_flag[i])
				{
					ptl.sub_layer_profile_space[i] = mReader->getBits(2);
					ptl.sub_layer_tier_flag[i] = mReader->getBits(1);
					ptl.sub_layer_profile_idc[i] = mReader->getBits(5);
					ptl.sub_layer_profile_compatibility_flag[i].resize(32);

					for (size_t j = 0; j < 32; ++j)
					{
						ptl.sub_layer_profile_compatibility_flag[i][j] = mReader->getBits(1);
					}

					ptl.sub_layer_progressive_source_flag[i] = mReader->getBits(1);
					ptl.sub_layer_interlaced_source_flag[i] = mReader->getBits(1);
					ptl.sub_layer_non_packed_constraint_flag[i] = mReader->getBits(1);
					ptl.sub_layer_frame_only_constraint_flag[i] = mReader->getBits(1);
					mReader->getBits(32);
					mReader->getBits(12);

				}

				if (ptl.sub_layer_level_present_flag[i])
				{
					ptl.sub_layer_level_idc[i] = mReader->getBits(8);
				}
				else
				{
					ptl.sub_layer_level_idc[i] = 1;
				}

			}

			return ptl;
		}


		void HevcParser::parsePPS(PPS& pps)
		{
			pps.pps_pic_parameter_set_id = mReader->getGolombU();
			pps.pps_seq_parameter_set_id = mReader->getGolombU();
			pps.dependent_slice_segments_enabled_flag = mReader->getBits(1);

			pps.output_flag_present_flag = mReader->getBits(1);
			pps.num_extra_slice_header_bits = mReader->getBits(3);
			pps.sign_data_hiding_flag = mReader->getBits(1);
			pps.cabac_init_present_flag = mReader->getBits(1);
			pps.num_ref_idx_l0_default_active_minus1 = mReader->getGolombU();
			pps.num_ref_idx_l1_default_active_minus1 = mReader->getGolombU();
			pps.init_qp_minus26 = mReader->getGolombS();
			pps.constrained_intra_pred_flag = mReader->getBits(1);
			pps.transform_skip_enabled_flag = mReader->getBits(1);
			pps.cu_qp_delta_enabled_flag = mReader->getBits(1);

			if (pps.cu_qp_delta_enabled_flag)
			{
				pps.diff_cu_qp_delta_depth = mReader->getGolombU();
			}
			else
			{
				pps.diff_cu_qp_delta_depth = 0;
			}

			pps.pps_cb_qp_offset = mReader->getGolombS();
			pps.pps_cr_qp_offset = mReader->getGolombS();
			pps.pps_slice_chroma_qp_offsets_present_flag = mReader->getBits(1);
			pps.weighted_pred_flag = mReader->getBits(1);
			pps.weighted_bipred_flag = mReader->getBits(1);
			pps.transquant_bypass_enabled_flag = mReader->getBits(1);
			pps.tiles_enabled_flag = mReader->getBits(1);
			pps.entropy_coding_sync_enabled_flag = mReader->getBits(1);

			if (pps.tiles_enabled_flag)
			{
				pps.num_tile_columns_minus1 = mReader->getGolombU();
				pps.num_tile_rows_minus1 = mReader->getGolombU();
				pps.uniform_spacing_flag = mReader->getBits(1);

				if (!pps.uniform_spacing_flag)
				{
					pps.column_width_minus1.resize(pps.num_tile_columns_minus1);
					for (size_t i = 0; i < pps.num_tile_columns_minus1; ++i)
						pps.column_width_minus1[i] = mReader->getGolombU();

					pps.row_height_minus1.resize(pps.num_tile_rows_minus1);
					for (size_t i = 0; i < pps.num_tile_rows_minus1; ++i)
						pps.row_height_minus1[i] = mReader->getGolombU();
				}
				pps.loop_filter_across_tiles_enabled_flag = mReader->getBits(1);
			}
			else
			{
				pps.num_tile_columns_minus1 = 0;
				pps.num_tile_rows_minus1 = 0;
				pps.uniform_spacing_flag = 1;
				pps.loop_filter_across_tiles_enabled_flag = 1;
			}

			pps.pps_loop_filter_across_slices_enabled_flag = mReader->getBits(1);
			pps.deblocking_filter_control_present_flag = mReader->getBits(1);

			if (pps.deblocking_filter_control_present_flag)
			{
				pps.deblocking_filter_override_enabled_flag = mReader->getBits(1);
				pps.pps_deblocking_filter_disabled_flag = mReader->getBits(1);

				if (!pps.pps_deblocking_filter_disabled_flag)
				{
					pps.pps_beta_offset_div2 = mReader->getGolombS();
					pps.pps_tc_offset_div2 = mReader->getGolombS();
				}
				else
				{
					pps.pps_beta_offset_div2 = 0;
					pps.pps_tc_offset_div2 = 0;
				}
			}
			else
			{
				pps.deblocking_filter_override_enabled_flag = 0;
				pps.pps_deblocking_filter_disabled_flag = 0;
			}

			pps.pps_scaling_list_data_present_flag = mReader->getBits(1);
			if (pps.pps_scaling_list_data_present_flag)
			{
				pps.scaling_list_data = parseScalingListData();
			}

			pps.lists_modification_present_flag = mReader->getBits(1);
			pps.log2_parallel_merge_level_minus2 = mReader->getGolombU();
			pps.slice_segment_header_extension_present_flag = mReader->getBits(1);
			pps.pps_extension_flag = mReader->getBits(1);
		}



		HrdParameters HevcParser::parseHrdParameters(uint8_t commonInfPresentFlag, size_t maxNumSubLayersMinus1)
		{
			HrdParameters hrd;

			hrd.nal_hrd_parameters_present_flag = 0;
			hrd.vcl_hrd_parameters_present_flag = 0;
			hrd.sub_pic_hrd_params_present_flag = 0;
			hrd.sub_pic_cpb_params_in_pic_timing_sei_flag = 0;
			if (commonInfPresentFlag)
			{
				hrd.nal_hrd_parameters_present_flag = mReader->getBits(1);
				hrd.vcl_hrd_parameters_present_flag = mReader->getBits(1);

				if (hrd.nal_hrd_parameters_present_flag || hrd.vcl_hrd_parameters_present_flag)
				{
					hrd.sub_pic_hrd_params_present_flag = mReader->getBits(1);
					if (hrd.sub_pic_hrd_params_present_flag)
					{
						hrd.tick_divisor_minus2 = mReader->getBits(8);
						hrd.du_cpb_removal_delay_increment_length_minus1 = mReader->getBits(5);
						hrd.sub_pic_cpb_params_in_pic_timing_sei_flag = mReader->getBits(1);
						hrd.dpb_output_delay_du_length_minus1 = mReader->getBits(5);
					}
					hrd.bit_rate_scale = mReader->getBits(4);
					hrd.cpb_size_scale = mReader->getBits(4);

					if (hrd.sub_pic_hrd_params_present_flag)
						hrd.cpb_size_du_scale = mReader->getBits(4);

					hrd.initial_cpb_removal_delay_length_minus1 = mReader->getBits(5);
					hrd.au_cpb_removal_delay_length_minus1 = mReader->getBits(5);
					hrd.dpb_output_delay_length_minus1 = mReader->getBits(5);
				}
			}

			hrd.fixed_pic_rate_general_flag.resize(maxNumSubLayersMinus1 + 1);
			hrd.fixed_pic_rate_within_cvs_flag.resize(maxNumSubLayersMinus1 + 1);
			hrd.elemental_duration_in_tc_minus1.resize(maxNumSubLayersMinus1 + 1);
			hrd.low_delay_hrd_flag.resize(maxNumSubLayersMinus1 + 1, 0);
			hrd.cpb_cnt_minus1.resize(maxNumSubLayersMinus1 + 1, 0);

			if (hrd.nal_hrd_parameters_present_flag)
			{
				hrd.nal_sub_layer_hrd_parameters.resize(maxNumSubLayersMinus1 + 1);
			}
			if (hrd.vcl_hrd_parameters_present_flag)
			{
				hrd.vcl_sub_layer_hrd_parameters.resize(maxNumSubLayersMinus1 + 1);
			}

			for (size_t i = 0; i <= maxNumSubLayersMinus1; ++i)
			{
				hrd.fixed_pic_rate_general_flag[i] = mReader->getBits(1);

				if (hrd.fixed_pic_rate_general_flag[i])
					hrd.fixed_pic_rate_within_cvs_flag[i] = 1;

				if (!hrd.fixed_pic_rate_general_flag[i])
					hrd.fixed_pic_rate_within_cvs_flag[i] = mReader->getBits(1);

				if (hrd.fixed_pic_rate_within_cvs_flag[i])
					hrd.elemental_duration_in_tc_minus1[i] = mReader->getGolombU();
				else
					hrd.low_delay_hrd_flag[i] = mReader->getBits(1);

				if (!hrd.low_delay_hrd_flag[i])
					hrd.cpb_cnt_minus1[i] = mReader->getGolombU();

				if (hrd.nal_hrd_parameters_present_flag)
					hrd.nal_sub_layer_hrd_parameters[i] = parseSubLayerHrdParameters(hrd.sub_pic_hrd_params_present_flag, hrd.cpb_cnt_minus1[i]);
				if (hrd.vcl_hrd_parameters_present_flag)
					hrd.vcl_sub_layer_hrd_parameters[i] = parseSubLayerHrdParameters(hrd.sub_pic_hrd_params_present_flag, hrd.cpb_cnt_minus1[i]);
			}

			return hrd;
		}

		SubLayerHrdParameters HevcParser::parseSubLayerHrdParameters(uint8_t sub_pic_hrd_params_present_flag, size_t cpb_cnt_minus1)
		{
			SubLayerHrdParameters slhrd;

			slhrd.bit_rate_value_minus1.resize(cpb_cnt_minus1 + 1);
			slhrd.cpb_size_value_minus1.resize(cpb_cnt_minus1 + 1);
			slhrd.cpb_size_du_value_minus1.resize(cpb_cnt_minus1 + 1);
			slhrd.bit_rate_du_value_minus1.resize(cpb_cnt_minus1 + 1);
			slhrd.cbr_flag.resize(cpb_cnt_minus1 + 1);

			for (size_t i = 0; i <= cpb_cnt_minus1; ++i)
			{
				slhrd.bit_rate_value_minus1[i] = mReader->getGolombU();
				slhrd.cpb_size_value_minus1[i] = mReader->getGolombU();

				if (sub_pic_hrd_params_present_flag)
				{
					slhrd.cpb_size_du_value_minus1[i] = mReader->getGolombU();
					slhrd.bit_rate_du_value_minus1[i] = mReader->getGolombU();
				}

				slhrd.cbr_flag[i] = mReader->getBits(1);
			}

			return slhrd;
		}



		ShortTermRefPicSet HevcParser::parseShortTermRefPicSet(uint32_t stRpsIdx, uint32_t num_short_term_ref_pic_sets, const std::vector<ShortTermRefPicSet>& refPicSets, SPS& sps, uint32_t& refRpsIdx)
		{
			ShortTermRefPicSet rps;

			rps.inter_ref_pic_set_prediction_flag = 0;
			rps.delta_idx_minus1 = 0;
			if (stRpsIdx)
			{
				rps.inter_ref_pic_set_prediction_flag = mReader->getBits(1);
			}

			if (rps.inter_ref_pic_set_prediction_flag)
			{
				// Only set if this ref pic set belongs to a slice.
				if (stRpsIdx == num_short_term_ref_pic_sets)
				{
					rps.delta_idx_minus1 = mReader->getGolombU();
				}

				rps.delta_rps_sign = mReader->getBits(1);
				rps.abs_delta_rps_minus1 = mReader->getGolombU();

				refRpsIdx = stRpsIdx - (rps.delta_idx_minus1 + 1);

				/*uint32_t numDeltaPocs = 0;

				if (refPicSets[refRpsIdx].inter_ref_pic_set_prediction_flag)
				{
					for (size_t i = 0; i < refPicSets[refRpsIdx].used_by_curr_pic_flag.size(); ++i)
					{
						if (refPicSets[refRpsIdx].used_by_curr_pic_flag[i] || refPicSets[refRpsIdx].use_delta_flag[i])
						{
							numDeltaPocs++;
						}
					}
				}
				else
				{
					numDeltaPocs = refPicSets[refRpsIdx].num_negative_pics + refPicSets[refRpsIdx].num_positive_pics;
				}*/

				uint32_t refRPSNumDelataPocs = refPicSets[refRpsIdx].num_negative_pics + refPicSets[refRpsIdx].num_positive_pics;
				
				// Only set if this ref pic set belongs to a slice.
				if (stRpsIdx == num_short_term_ref_pic_sets)
				{
					mExtraData.numDeltaPocsOfRefRpsIdx = refRPSNumDelataPocs;
				}

				uint32_t numDeltaPocs = refRPSNumDelataPocs + 1;

				rps.used_by_curr_pic_flag.resize(numDeltaPocs);
				rps.use_delta_flag.resize(numDeltaPocs, 1);

				for (uint32_t i = 0; i < numDeltaPocs; ++i)
				{
					// Determines if the ith entry in the source candidate RPS is referenced by the current picture.
					rps.used_by_curr_pic_flag[i] = mReader->getBits(1);
					if (!rps.used_by_curr_pic_flag[i])
					{
						// Determines if the ith entry in the source candidate RPS is included in this RPS.
						rps.use_delta_flag[i] = mReader->getBits(1);
					}
				}

				int deltaRps = (1 - (2 * rps.delta_rps_sign)) * (rps.abs_delta_rps_minus1 + 1);


				// Pictures with poc values less than the current picture's.
				for (int j = refPicSets[refRpsIdx].num_positive_pics - 1; j >= 0; --j)
				{
					int dPoc = (refPicSets[refRpsIdx].delta_poc_s1_minus1[j] + 1) + deltaRps;
					if (dPoc < 0 && rps.use_delta_flag[refPicSets[refRpsIdx].num_negative_pics + j])
					{
						uint32_t dPocMinus1 = (uint32_t)(dPoc * -1) - 1;
						rps.delta_poc_s0_minus1.push_back(dPocMinus1);
						rps.used_by_curr_pic_s0_flag.push_back(rps.used_by_curr_pic_flag[refPicSets[refRpsIdx].num_negative_pics + j]);
					}
				}

				if (deltaRps < 0 && rps.use_delta_flag[refRPSNumDelataPocs])
				{
					uint32_t dPocMinus1 = (uint32_t)(deltaRps * -1) - 1;
					rps.delta_poc_s0_minus1.push_back(dPocMinus1);
					rps.used_by_curr_pic_s0_flag.push_back(rps.used_by_curr_pic_flag[refRPSNumDelataPocs]);
				} 

				for (int j = 0; j < refPicSets[refRpsIdx].num_negative_pics; ++j)
				{
					int dPoc = (refPicSets[refRpsIdx].delta_poc_s0_minus1[j] + 1) + deltaRps;
					if (dPoc < 0 && rps.use_delta_flag[j])
					{
						uint32_t dPocMinus1 = (uint32_t)(dPoc * -1) - 1;
						rps.delta_poc_s0_minus1.push_back(dPocMinus1);
						rps.used_by_curr_pic_s0_flag.push_back(rps.used_by_curr_pic_flag[j]);
					}
				}
				rps.num_negative_pics = rps.used_by_curr_pic_s0_flag.size();


				// Pictures with poc values greater than the current picture's.	
				for (int j = refPicSets[refRpsIdx].num_negative_pics - 1; j >= 0; --j)
				{
					int dPoc = (refPicSets[refRpsIdx].delta_poc_s0_minus1[j] + 1) + deltaRps;
					if (dPoc > 0 && rps.use_delta_flag[j])
					{
						uint32_t dPocMinus1 = (uint32_t)(dPoc * -1) - 1;
						rps.delta_poc_s1_minus1.push_back(dPocMinus1);
						rps.used_by_curr_pic_s1_flag.push_back(rps.used_by_curr_pic_flag[j]);
					}
				}

				if (deltaRps > 0 && rps.use_delta_flag[refRPSNumDelataPocs])
				{
					uint32_t dPocMinus1 = (uint32_t)(deltaRps * -1) - 1;
					rps.delta_poc_s1_minus1.push_back(dPocMinus1);
					rps.used_by_curr_pic_s1_flag.push_back(rps.used_by_curr_pic_flag[refRPSNumDelataPocs]);
				}

				for (int j = 0; j < refPicSets[refRpsIdx].num_positive_pics; ++j)
				{
					int dPoc = (refPicSets[refRpsIdx].delta_poc_s1_minus1[j] + 1) + deltaRps;
					if (dPoc > 0 && rps.use_delta_flag[refPicSets[refRpsIdx].num_negative_pics + j])
					{
						uint32_t dPocMinus1 = (uint32_t)(dPoc * -1) - 1;
						rps.delta_poc_s1_minus1.push_back(dPocMinus1);
						rps.used_by_curr_pic_s1_flag.push_back(rps.used_by_curr_pic_flag[refPicSets[refRpsIdx].num_negative_pics + j]);
					}
				}
				rps.num_positive_pics = rps.used_by_curr_pic_s1_flag.size();
			}
			else
			{
				rps.num_negative_pics = mReader->getGolombU();
				rps.num_positive_pics = mReader->getGolombU();

				if (rps.num_negative_pics > sps.sps_max_dec_pic_buffering_minus1[sps.sps_max_sub_layers_minus1])
				{
					TELEPORT_CERR << "Error in parseShortTermRefPicSet: num_negative_pics > sps_max_dec_pic_buffering_minus1" << std::endl;
					return rps;
				}

				if (rps.num_positive_pics > sps.sps_max_dec_pic_buffering_minus1[sps.sps_max_sub_layers_minus1])
				{
					TELEPORT_CERR << " Error in parseShortTermRefPicSet: num_positive_pics > sps_max_dec_pic_buffering_minus1" << std::endl;
					return rps;
				}

				rps.delta_poc_s0_minus1.resize(rps.num_negative_pics);
				rps.used_by_curr_pic_s0_flag.resize(rps.num_negative_pics);

				for (uint32_t i = 0; i < rps.num_negative_pics; ++i)
				{
					rps.delta_poc_s0_minus1[i] = mReader->getGolombU();
					rps.used_by_curr_pic_s0_flag[i] = mReader->getBits(1);
				}

				rps.delta_poc_s1_minus1.resize(rps.num_positive_pics);
				rps.used_by_curr_pic_s1_flag.resize(rps.num_positive_pics);
				for (uint32_t i = 0; i < rps.num_positive_pics; ++i)
				{
					rps.delta_poc_s1_minus1[i] = mReader->getGolombU();
					rps.used_by_curr_pic_s1_flag[i] = mReader->getBits(1);
				}

			}

			return rps;
		}


		VuiParameters HevcParser::parseVuiParameters(size_t sps_max_sub_layers_minus1)
		{
			VuiParameters vui;

			vui.aspect_ratio_idc = 0;
			vui.sar_width = 0;
			vui.sar_height = 0;

			vui.aspect_ratio_info_present_flag = mReader->getBits(1);

			if (vui.aspect_ratio_info_present_flag)
			{
				vui.aspect_ratio_idc = mReader->getBits(8);

				if (vui.aspect_ratio_idc == 255) //EXTENDED_SAR
				{
					vui.sar_width = mReader->getBits(16);
					vui.sar_height = mReader->getBits(16);
				}
			}


			vui.overscan_info_present_flag = mReader->getBits(1);
			if (vui.overscan_info_present_flag)
			{
				vui.overscan_appropriate_flag = mReader->getBits(1);
			}

			vui.video_format = 5;
			vui.video_full_range_flag = 0;
			vui.colour_primaries = 2;
			vui.transfer_characteristics = 2;
			vui.matrix_coeffs = 2;

			vui.video_signal_type_present_flag = mReader->getBits(1);

			if (vui.video_signal_type_present_flag)
			{
				vui.video_format = mReader->getBits(3);
				vui.video_full_range_flag = mReader->getBits(1);
				vui.colour_description_present_flag = mReader->getBits(1);

				if (vui.colour_description_present_flag)
				{
					vui.colour_primaries = mReader->getBits(8);
					vui.transfer_characteristics = mReader->getBits(8);
					vui.matrix_coeffs = mReader->getBits(8);
				}

			}

			vui.chroma_sample_loc_type_top_field = 0;
			vui.chroma_sample_loc_type_bottom_field = 0;

			vui.chroma_loc_info_present_flag = mReader->getBits(1);
			if (vui.chroma_loc_info_present_flag)
			{
				vui.chroma_sample_loc_type_top_field = mReader->getGolombU();
				vui.chroma_sample_loc_type_bottom_field = mReader->getGolombU();
			}


			vui.neutral_chroma_indication_flag = mReader->getBits(1);
			vui.field_seq_flag = mReader->getBits(1);
			vui.frame_field_info_present_flag = mReader->getBits(1);
			vui.default_display_window_flag = mReader->getBits(1);

			vui.def_disp_win_left_offset = 0;
			vui.def_disp_win_right_offset = 0;
			vui.def_disp_win_right_offset = 0;
			vui.def_disp_win_bottom_offset = 0;

			if (vui.default_display_window_flag)
			{
				vui.def_disp_win_left_offset = mReader->getGolombU();
				vui.def_disp_win_right_offset = mReader->getGolombU();
				vui.def_disp_win_top_offset = mReader->getGolombU();
				vui.def_disp_win_bottom_offset = mReader->getGolombU();
			}

			vui.vui_timing_info_present_flag = mReader->getBits(1);

			if (vui.vui_timing_info_present_flag)
			{
				vui.vui_num_units_in_tick = mReader->getBits(32);
				vui.vui_time_scale = mReader->getBits(32);
				vui.vui_poc_proportional_to_timing_flag = mReader->getBits(1);

				if (vui.vui_poc_proportional_to_timing_flag)
				{
					vui.vui_num_ticks_poc_diff_one_minus1 = mReader->getGolombU();
				}

				vui.vui_hrd_parameters_present_flag = mReader->getBits(1);

				if (vui.vui_hrd_parameters_present_flag)
				{
					vui.hrd_parameters = parseHrdParameters(1, sps_max_sub_layers_minus1);
				}
			}

			vui.bitstream_restriction_flag = mReader->getBits(1);

			if (vui.bitstream_restriction_flag)
			{
				vui.tiles_fixed_structure_flag = mReader->getBits(1);
				vui.motion_vectors_over_pic_boundaries_flag = mReader->getBits(1);
				vui.restricted_ref_pic_lists_flag = mReader->getBits(1);

				vui.min_spatial_segmentation_idc = mReader->getGolombU();
				vui.max_bytes_per_pic_denom = mReader->getGolombU();
				vui.max_bits_per_min_cu_denom = mReader->getGolombU();
				vui.log2_max_mv_length_horizontal = mReader->getGolombU();
				vui.log2_max_mv_length_vertical = mReader->getGolombU();
			}

			return vui;
		}


		ScalingListData HevcParser::parseScalingListData()
		{
			ScalingListData sc;

			sc.scaling_list_pred_mode_flag.resize(4);
			sc.scaling_list_pred_matrix_id_delta.resize(4);
			sc.scaling_list_dc_coef_minus8.resize(2);
			sc.scaling_list_delta_coef.resize(4);

			for (size_t sizeId = 0; sizeId < 4; sizeId++)
			{
				if (sizeId == 3)
				{
					sc.scaling_list_pred_mode_flag[sizeId].resize(2);
					sc.scaling_list_pred_matrix_id_delta[sizeId].resize(2);
					sc.scaling_list_dc_coef_minus8[sizeId - 2].resize(2);
					sc.scaling_list_delta_coef[sizeId].resize(2);
				}
				else
				{
					sc.scaling_list_pred_mode_flag[sizeId].resize(6);
					sc.scaling_list_pred_matrix_id_delta[sizeId].resize(6);
					sc.scaling_list_delta_coef[sizeId].resize(6);
					if (sizeId >= 2)
					{
						sc.scaling_list_dc_coef_minus8[sizeId - 2].resize(6);
					}
				}

				for (size_t matrixId = 0; matrixId < ((sizeId == 3) ? 2 : 6); matrixId++)
				{
					sc.scaling_list_pred_mode_flag[sizeId][matrixId] = mReader->getBits(1);
					if (!sc.scaling_list_pred_mode_flag[sizeId][matrixId])
					{
						sc.scaling_list_pred_matrix_id_delta[sizeId][matrixId] = mReader->getGolombU();
					}
					else
					{
						size_t nextCoef = 8;
						size_t coefNum = std::min(64, (1 << (4 + (sizeId << 1))));
						if (sizeId > 1)
						{
							sc.scaling_list_dc_coef_minus8[sizeId - 2][matrixId] = mReader->getGolombS();
						}

						sc.scaling_list_delta_coef[sizeId][matrixId].resize(coefNum);
						for (size_t i = 0; i < coefNum; ++i)
						{
							sc.scaling_list_delta_coef[sizeId][matrixId][i] = mReader->getGolombS();
						}
					}
				}
			}

			return sc;
		}

		void HevcParser::parseSliceHeader(Slice& slice)
		{
			slice.first_slice_segment_in_pic_flag = mReader->getBits(1);

			if (slice.header.type >= NALUnitType::BLA_W_LP && slice.header.type <= NALUnitType::IRAP_VCL23)
			{
				slice.no_output_of_prior_pics_flag = mReader->getBits(1);
			}

			slice.slice_pic_parameter_set_id = mReader->getGolombU();

			int32_t spsId = mPPS.pps_seq_parameter_set_id;

			slice.dependent_slice_segment_flag = 0;
			if (!slice.first_slice_segment_in_pic_flag)
			{
				if (mPPS.dependent_slice_segments_enabled_flag)
				{
					slice.dependent_slice_segment_flag = mReader->getBits(1);
				}
				else
				{
					slice.dependent_slice_segment_flag = 0;
				}
				int ctbLog2SizeY = mSPS.log2_min_luma_coding_block_size_minus3 + 3 + mSPS.log2_diff_max_min_luma_coding_block_size;
				uint32_t ctbSizeY = 1 << ctbLog2SizeY;
				uint32_t picWidthInCtbsY = mSPS.pic_width_in_luma_samples / ctbSizeY;
				if (mSPS.pic_width_in_luma_samples % ctbSizeY)
				{
					picWidthInCtbsY++;
				}

				uint32_t picHeightInCtbsY = mSPS.pic_height_in_luma_samples / ctbSizeY;
				if (mSPS.pic_height_in_luma_samples % ctbSizeY)
				{
					picHeightInCtbsY++;
				}

				int sliceAddrLength = log2(picHeightInCtbsY * picWidthInCtbsY);
				if ((1 << sliceAddrLength) < picHeightInCtbsY * picWidthInCtbsY)
				{
					sliceAddrLength++;
				}

				slice.slice_segment_address = mReader->getBits(sliceAddrLength);
			}

			if (!slice.dependent_slice_segment_flag)
			{
				uint32_t num_extra_slice_header_bits = mPPS.num_extra_slice_header_bits;
				slice.slice_reserved_undetermined_flag.resize(num_extra_slice_header_bits, 0);
				for (size_t i = 0; i < num_extra_slice_header_bits; ++i)
				{
					slice.slice_reserved_undetermined_flag[i] = mReader->getBits(1);
				}

				slice.slice_type = (SliceType)mReader->getGolombU();

				if (mPPS.output_flag_present_flag)
				{
					slice.pic_output_flag = mReader->getBits(1);
				}

				if (mSPS.separate_colour_plane_flag)
				{
					slice.colour_plane_id = mReader->getBits(2);
				}

				bool idrPicFlag = slice.header.type == NALUnitType::IDR_W_RADL || slice.header.type == NALUnitType::IDR_N_LP;
				if (idrPicFlag)
				{
					mPrevPocTid0 = 0;
				}
				else
				{
					if (mSPS.log2_max_pic_order_cnt_lsb_minus4 + 4 >= 32)
					{
						TELEPORT_CERR << "Error in parseSliceHeader: slice_pic_order_cnt_lsb size is greater then 32 bits!" << std::endl;
						return;
					}

					slice.slice_pic_order_cnt_lsb = mReader->getBits(mSPS.log2_max_pic_order_cnt_lsb_minus4 + 4);
					mExtraData.poc = computePoc(mSPS, mPrevPocTid0, slice.slice_pic_order_cnt_lsb, (uint32_t)slice.header.type);

					slice.short_term_ref_pic_set_sps_flag = mReader->getBits(1);

					size_t remainingBits = mReader->getBitsRemaining();

					mExtraData.numDeltaPocsOfRefRpsIdx = 0;

					if (!slice.short_term_ref_pic_set_sps_flag)
					{
						slice.short_term_ref_pic_set = parseShortTermRefPicSet(mSPS.num_short_term_ref_pic_sets, mSPS.num_short_term_ref_pic_sets, mSPS.short_term_ref_pic_set, mSPS, mExtraData.refRpsIdx);
						mExtraData.stRpsIdx = mSPS.num_short_term_ref_pic_sets;
					}
					else if (mSPS.num_short_term_ref_pic_sets > 1)
					{
						size_t numBits = log2(mSPS.num_short_term_ref_pic_sets);
						if (1 << numBits < mSPS.num_short_term_ref_pic_sets)
						{
							numBits++;
						}

						if (numBits > 0)
						{
							slice.short_term_ref_pic_set_idx = mReader->getBits(numBits);
						}
						else
						{
							slice.short_term_ref_pic_set_idx = 0;
						}
						mExtraData.stRpsIdx = slice.short_term_ref_pic_set_idx;
						mExtraData.refRpsIdx = mExtraData.stRpsIdx;
					}

					mExtraData.short_term_ref_pic_set_size = remainingBits - mReader->getBitsRemaining();

					// Long term reference frames may not be enabled. This is an option that can be enabled in the video encoder.
					if (mSPS.long_term_ref_pics_present_flag)
					{
						slice.num_long_term_sps = 0;
						if (mSPS.num_long_term_ref_pics_sps > 0)
						{
							slice.num_long_term_sps = mReader->getGolombU();
						}

						slice.num_long_term_pics = mReader->getGolombU();

						size_t num_long_term = slice.num_long_term_sps + slice.num_long_term_pics;

						slice.lt_idx_sps.resize(num_long_term, 0);
						slice.poc_lsb_lt.resize(num_long_term);
						slice.used_by_curr_pic_lt_flag.resize(num_long_term);
						slice.delta_poc_msb_present_flag.resize(num_long_term);
						slice.delta_poc_msb_cycle_lt.resize(num_long_term);

						mExtraData.longTermRefPicPocs.resize(num_long_term, 0);

						uint32_t prevPocMsb = 0;

						for (size_t i = 0; i < num_long_term; ++i)
						{
							if (i < slice.num_long_term_sps)
							{
								if (mSPS.num_long_term_ref_pics_sps > 1)
								{
									uint32_t size = log2(mSPS.num_long_term_ref_pics_sps);
									slice.lt_idx_sps[i] = mReader->getBits(size);
									slice.poc_lsb_lt[i] = mSPS.lt_ref_pic_poc_lsb_sps[slice.lt_idx_sps[i]];
									slice.used_by_curr_pic_lt_flag[i] = mSPS.used_by_curr_pic_lt_sps_flag[slice.lt_idx_sps[i]];
								}
							}
							else
							{
								slice.poc_lsb_lt[i] = mReader->getBits(mSPS.log2_max_pic_order_cnt_lsb_minus4 + 4);
								slice.used_by_curr_pic_lt_flag[i] = mReader->getBits(1);
							}

							slice.delta_poc_msb_present_flag[i] = mReader->getBits(1);
							if (slice.delta_poc_msb_present_flag[i])
							{
								slice.delta_poc_msb_cycle_lt[i] = mReader->getGolombU();
							}
							else
							{
								slice.delta_poc_msb_cycle_lt[i] = 0;
							}

							uint32_t pocMsb;
							if (i == 0 || i == slice.num_long_term_sps)
							{
								pocMsb = slice.delta_poc_msb_cycle_lt[i];
							}
							else 
							{
								pocMsb = slice.delta_poc_msb_cycle_lt[i] + prevPocMsb;
							}

							prevPocMsb = pocMsb;
							
							mExtraData.longTermRefPicPocs[i] = (pocMsb << (mSPS.log2_max_pic_order_cnt_lsb_minus4 + 4)) + slice.poc_lsb_lt[i];
						}
					}

					if (mSPS.sps_temporal_mvp_enabled_flag)
					{
						slice.slice_temporal_mvp_enabled_flag = mReader->getBits(1); 
					}
				}

				if ((slice.header.temporal_id_plus1 - 1) == 0 &&
					slice.header.type != NALUnitType::TRAIL_N &&
					slice.header.type != NALUnitType::TSA_N &&
					slice.header.type != NALUnitType::STSA_N &&
					slice.header.type != NALUnitType::RADL_N &&
					slice.header.type != NALUnitType::RASL_N &&
					slice.header.type != NALUnitType::RADL_R &&
					slice.header.type != NALUnitType::RASL_R)
				{
					mPrevPocTid0 = mExtraData.poc;
				}

				if (mSPS.sample_adaptive_offset_enabled_flag)
				{
					slice.slice_sao_luma_flag = mReader->getBits(1);
					slice.slice_sao_chroma_flag = mReader->getBits(1);
				}

				slice.num_ref_idx_l0_active_minus1 = mPPS.num_ref_idx_l0_default_active_minus1;
				slice.num_ref_idx_l1_active_minus1 = mPPS.num_ref_idx_l1_default_active_minus1;

				if (slice.slice_type == SliceType::B || slice.slice_type == SliceType::P)
				{
					slice.num_ref_idx_active_override_flag = mReader->getBits(1);
					if (slice.num_ref_idx_active_override_flag)
					{
						slice.num_ref_idx_l0_active_minus1 = mReader->getGolombU();

						if (slice.slice_type == SliceType::B)
						{
							slice.num_ref_idx_l1_active_minus1 = mReader->getGolombU();
						}
					}

					if (mPPS.lists_modification_present_flag)
					{
						uint32_t numPocTotal = computeNumPocTotal(slice, mSPS);
						if (numPocTotal > 1)
						{
							slice.ref_pic_lists_modification = parseRefPicListModification(slice);
						}
					}

					if (slice.slice_type == SliceType::B)
					{
						slice.mvd_l1_zero_flag = mReader->getBits(1);
					}

					if (mPPS.cabac_init_present_flag)
					{
						slice.cabac_init_flag = mReader->getBits(1);
					}

					if (slice.slice_temporal_mvp_enabled_flag)
					{
						if (slice.slice_type == SliceType::B)
						{
							slice.collocated_from_l0_flag = mReader->getBits(1);
						}

						if (slice.collocated_from_l0_flag && slice.num_ref_idx_l0_active_minus1 ||
							!slice.collocated_from_l0_flag && slice.num_ref_idx_l1_active_minus1)
						{
							slice.collocated_ref_idx = mReader->getGolombU();
						}
					}

					if (mPPS.weighted_pred_flag && slice.slice_type == SliceType::P ||
						mPPS.weighted_bipred_flag && slice.slice_type == SliceType::B)
					{
						slice.pred_weight_table = parsePredWeightTable(slice);

						if (slice.pred_weight_table.luma_log2_weight_denom > 7)
						{
							TELEPORT_CERR << "pred_weight_table.luma_log2_weight_denom must be in range (0-7) but equals " 
								<< slice.pred_weight_table.luma_log2_weight_denom << std::endl;
						}
					}

					slice.five_minus_max_num_merge_cand = mReader->getGolombU();
				}
				slice.slice_qp_delta = mReader->getGolombS();

				if (mPPS.pps_slice_chroma_qp_offsets_present_flag)
				{
					slice.slice_cb_qp_offset = mReader->getGolombS();
					slice.slice_cr_qp_offset = mReader->getGolombS();
				}

				if (mPPS.deblocking_filter_override_enabled_flag)
				{
					slice.deblocking_filter_override_flag = mReader->getBits(1);
				}

				if (slice.deblocking_filter_override_flag)
				{
					slice.slice_deblocking_filter_disabled_flag = mReader->getBits(1);
					if (!slice.slice_deblocking_filter_disabled_flag)
					{
						slice.slice_beta_offset_div2 = mReader->getGolombS();
						slice.slice_tc_offset_div2 = mReader->getGolombS();
					}
				}
				else
				{
					slice.slice_deblocking_filter_disabled_flag = mPPS.pps_deblocking_filter_disabled_flag;
				}

				if (mPPS.pps_loop_filter_across_slices_enabled_flag &&
					(slice.slice_sao_luma_flag || slice.slice_sao_chroma_flag || !slice.slice_deblocking_filter_disabled_flag))
				{
					slice.slice_loop_filter_across_slices_enabled_flag = mReader->getBits(1);
				}
			}

			if (mPPS.tiles_enabled_flag || mPPS.entropy_coding_sync_enabled_flag)
			{
				slice.num_entry_point_offsets = mReader->getGolombU();
				if (slice.num_entry_point_offsets > 0)
				{
					slice.offset_len_minus1 = mReader->getGolombU();
					slice.entry_point_offset_minus1.resize(slice.num_entry_point_offsets);

					if (slice.offset_len_minus1 > 31)
					{
						TELEPORT_CERR << "offset_len_minus1 must be in range (0-31) but equals "
							<< slice.offset_len_minus1 << std::endl;

						return;
					}
					for (size_t i = 0; i < slice.num_entry_point_offsets; ++i)
					{
						slice.entry_point_offset_minus1[i] = mReader->getBits(slice.offset_len_minus1 + 1);
					}
				}
			}

			if (mPPS.slice_segment_header_extension_present_flag)
			{
				slice.slice_segment_header_extension_length = mReader->getGolombU();
				slice.slice_segment_header_extension_data_byte.resize(slice.slice_segment_header_extension_length);
				for (size_t i = 0; i < slice.slice_segment_header_extension_length; ++i)
				{
					slice.slice_segment_header_extension_data_byte[i] = mReader->getBits(8);
				}
			}
		}

		RefPicListModification HevcParser::parseRefPicListModification(Slice& slice)
		{
			RefPicListModification res;

			int32_t spsId = mPPS.pps_seq_parameter_set_id;

			size_t totalRefPics = computeNumPocTotal(slice, mSPS);
			int32_t listSize = log2(totalRefPics);

			if ((1 << listSize) < totalRefPics)
			{
				listSize++;
			}


			res.ref_pic_list_modification_flag_l0 = mReader->getBits(1);

			if (res.ref_pic_list_modification_flag_l0)
			{
				res.list_entry_l0.resize(slice.num_ref_idx_l0_active_minus1);

				for (size_t i = 0; i < slice.num_ref_idx_l0_active_minus1; ++i)
				{
					res.list_entry_l0[i] = mReader->getBits(listSize);
				}
			}

			res.ref_pic_list_modification_flag_l1 = mReader->getBits(1);

			if (res.ref_pic_list_modification_flag_l1)
			{
				res.list_entry_l1.resize(slice.num_ref_idx_l1_active_minus1);

				for (size_t i = 0; i < slice.num_ref_idx_l1_active_minus1; ++i)
				{
					res.list_entry_l1[i] = mReader->getBits(listSize);
				}
			}

			return res;
		}


		PredWeightTable HevcParser::parsePredWeightTable(Slice& slice)
		{
			PredWeightTable table;

			table.luma_log2_weight_denom = mReader->getGolombU();
			if (mSPS.chroma_format_idc != 0)
			{
				table.delta_chroma_log2_weight_denom = mReader->getGolombS();
			}

			table.luma_weight_l0_flag.resize(slice.num_ref_idx_l0_active_minus1 + 1);

			for (size_t i = 0; i <= slice.num_ref_idx_l0_active_minus1; ++i)
			{
				table.luma_weight_l0_flag[i] = mReader->getBits(1);
			}

			table.chroma_weight_l0_flag.resize(slice.num_ref_idx_l0_active_minus1 + 1, 0);

			if (mSPS.chroma_format_idc != 0)
			{
				for (size_t i = 0; i <= slice.num_ref_idx_l0_active_minus1; ++i)
				{
					table.chroma_weight_l0_flag[i] = mReader->getBits(1);
				}
			}

			table.delta_luma_weight_l0.resize(slice.num_ref_idx_l0_active_minus1 + 1);
			table.luma_offset_l0.resize(slice.num_ref_idx_l0_active_minus1 + 1);

			table.delta_chroma_weight_l0.resize(slice.num_ref_idx_l0_active_minus1 + 1);
			table.delta_chroma_offset_l0.resize(slice.num_ref_idx_l0_active_minus1 + 1);

			for (size_t i = 0; i <= slice.num_ref_idx_l0_active_minus1; ++i)
			{
				if (table.luma_weight_l0_flag[i])
				{
					table.delta_luma_weight_l0[i] = mReader->getGolombS();
					table.luma_offset_l0[i] = mReader->getGolombS();
				}
				if (table.chroma_weight_l0_flag[i])
				{
					for (size_t j = 0; j < 2; ++j)
					{
						table.delta_chroma_weight_l0[i][j] = mReader->getGolombS();
						table.delta_chroma_offset_l0[i][j] = mReader->getGolombS();
					}
				}
			}

			if (slice.slice_type == SliceType::B)
			{
				table.luma_weight_l1_flag.resize(slice.num_ref_idx_l1_active_minus1 + 1);

				for (size_t i = 0; i <= slice.num_ref_idx_l1_active_minus1; ++i)
				{
					table.luma_weight_l1_flag[i] = mReader->getBits(1);
				}

				table.chroma_weight_l1_flag.resize(slice.num_ref_idx_l1_active_minus1 + 1, 0);

				if (mSPS.chroma_format_idc != 0)
				{
					for (size_t i = 0; i <= slice.num_ref_idx_l1_active_minus1; ++i)
					{
						table.chroma_weight_l1_flag[i] = mReader->getBits(1);
					}
				}

				table.delta_luma_weight_l1.resize(slice.num_ref_idx_l1_active_minus1 + 1);
				table.luma_offset_l1.resize(slice.num_ref_idx_l1_active_minus1 + 1);
				table.delta_chroma_weight_l1.resize(slice.num_ref_idx_l1_active_minus1 + 1);
				table.delta_chroma_offset_l1.resize(slice.num_ref_idx_l1_active_minus1 + 1);

				for (size_t i = 0; i <= slice.num_ref_idx_l1_active_minus1; ++i)
				{
					if (table.luma_weight_l1_flag[i])
					{
						table.delta_luma_weight_l1[i] = mReader->getGolombS();
						table.luma_offset_l1[i] = mReader->getGolombS();
					}
					if (table.chroma_weight_l1_flag[i])
					{
						for (size_t j = 0; j < 2; ++j)
						{
							table.delta_chroma_weight_l1[i][j] = mReader->getGolombS();
							table.delta_chroma_offset_l1[i][j] = mReader->getGolombS();
						}
					}
				}
			}
			return table;
		}

		uint32_t HevcParser::log2(uint32_t k)
		{
			uint32_t output = 0;

			if (k & 0xffff0000) 
			{
				k >>= 16;
				output += 16;
			}


			if (k & 0xff00) 
			{
				k >>= 8;
				output += 8;
			}

			output += mLog2Table[k];

			return output;
		}

		uint32_t HevcParser::computeNumPocTotal(Slice& slice, SPS& sps)
		{
			size_t numPocTotal = 0;
			size_t currStrpsIdx;

			bool usedByCurrPicLt[16];

			size_t numLongTermRefs = slice.num_long_term_sps + slice.num_long_term_pics;

			for (size_t i = 0; i < numLongTermRefs; ++i)
			{
				if (i < slice.num_long_term_sps)
				{
					usedByCurrPicLt[i] = sps.used_by_curr_pic_lt_sps_flag[slice.lt_idx_sps[i]];
				}
				else
				{
					usedByCurrPicLt[i] = slice.used_by_curr_pic_lt_flag[i];
				}
			}

			if (slice.short_term_ref_pic_set_sps_flag)
			{
				currStrpsIdx = slice.short_term_ref_pic_set_idx;
			}
			else
			{
				currStrpsIdx = sps.num_short_term_ref_pic_sets;
			}

			if (sps.short_term_ref_pic_set.size() <= currStrpsIdx)
			{
				if (currStrpsIdx != 0 || slice.short_term_ref_pic_set_sps_flag)
				{
					return 0;
				}
			}

			ShortTermRefPicSet strps;

			if (currStrpsIdx < sps.short_term_ref_pic_set.size())
			{
				strps = sps.short_term_ref_pic_set[currStrpsIdx];
			}
			else
			{
				strps = slice.short_term_ref_pic_set;
			}

			for (size_t i = 0; i < strps.num_negative_pics; ++i)
			{
				if (strps.used_by_curr_pic_s0_flag[i])
				{
					numPocTotal++;
				}
			}

			for (size_t i = 0; i < strps.num_positive_pics; ++i)
			{
				if (strps.used_by_curr_pic_s1_flag[i])
				{
					numPocTotal++;
				}
			}

			for (size_t i = 0; i < numLongTermRefs; ++i)
			{
				if (usedByCurrPicLt[i])
				{
					numPocTotal++;
				}
			}

			return numPocTotal;
		}

		uint32_t HevcParser::computePoc(const SPS& sps, uint32_t prevPocTid0, uint32_t pocLsb, uint32_t nalUnitType)
		{
			uint32_t maxPocLsb = 1 << (sps.log2_max_pic_order_cnt_lsb_minus4 + 4);
			uint32_t prevPocLsb = prevPocTid0 % maxPocLsb;
			uint32_t prevPocMsb = prevPocTid0 - prevPocLsb;
			uint32_t pocMsb;


			// POC msb must be set to 0 for BLA picture types.
			if ((hevc::NALUnitType)nalUnitType == hevc::NALUnitType::BLA_W_LP ||
				(hevc::NALUnitType)nalUnitType == hevc::NALUnitType::BLA_W_RADL ||
				(hevc::NALUnitType)nalUnitType == hevc::NALUnitType::BLA_N_LP)
			{
				pocMsb = 0;
			}
			else if (pocLsb < prevPocLsb && prevPocLsb - pocLsb >= maxPocLsb / 2)
			{
				pocMsb = prevPocMsb + maxPocLsb;
			}
			else if (pocLsb > prevPocLsb && pocLsb - prevPocLsb > maxPocLsb / 2)
			{
				pocMsb = prevPocMsb - maxPocLsb;
			}
			else
			{
				pocMsb = prevPocMsb;
			}

			return pocMsb + pocLsb;
		}
	}
}

