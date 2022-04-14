// (C) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <memory>
#include <vector>
#include <array>
#include <cstdint>
#include <cstddef>

namespace avparser
{
	namespace hevc
	{
		enum class NALUnitType : uint32_t
		{
			TRAIL_N = 0,
			TRAIL_R = 1,
			TSA_N = 2,
			TSA_R = 3,
			STSA_N = 4,
			STSA_R = 5,
			RADL_N = 6,
			RADL_R = 7,
			RASL_N = 8,
			RASL_R = 9,
			BLA_W_LP = 16,
			BLA_W_RADL = 17,
			BLA_N_LP = 18,
			IDR_W_RADL = 19,
			IDR_N_LP = 20,
			CRA_NUT = 21,
			IRAP_VCL23 = 23,
			VPS = 32,
			SPS = 33,
			PPS = 34,
			AUD = 35,
			EOS_NUT = 36,
			EOB_NUT = 37,
			FD_NUT = 38,
			SEI_PREFIX = 39,
			SEI_SUFFIX = 40,
			RSV_NVCL41 = 41,
			RSV_NVCL42 = 42,
			RSV_NVCL43 = 43,
			RSV_NVCL44 = 44,
			RSV_NVCL45 = 45,
			RSV_NVCL46 = 46,
			RSV_NVCL47 = 47,
			UNSPEC48 = 48,
			UNSPEC49 = 49,
			UNSPEC50 = 50,
			UNSPEC51 = 51,
			UNSPEC52 = 52,
			UNSPEC53 = 53,
			UNSPEC54 = 54,
			UNSPEC55 = 55,
			UNSPEC56 = 56,
			UNSPEC57 = 57,
			UNSPEC58 = 58,
			UNSPEC59 = 59,
			UNSPEC60 = 60,
			UNSPEC61 = 61,
			UNSPEC62 = 62,
			UNSPEC63 = 63
		};

		struct NALHeader
		{
			NALUnitType           type;
			uint8_t               layer_id;
			uint8_t               temporal_id_plus1;
		};

		struct ProfileTierLevel
		{
			uint8_t general_profile_space = 0;
			uint8_t general_tier_flag = 0;
			uint8_t general_profile_idc = 0;
			uint8_t general_profile_compatibility_flag[32];
			uint8_t general_progressive_source_flag = 0;
			uint8_t general_interlaced_source_flag = 0;
			uint8_t general_non_packed_constraint_flag = 0;
			uint8_t general_frame_only_constraint_flag = 0;
			uint8_t general_level_idc = 0;
			std::vector<uint8_t>   sub_layer_profile_present_flag;
			std::vector<uint8_t>   sub_layer_level_present_flag;
			std::vector<uint8_t>   sub_layer_profile_space;
			std::vector<uint8_t>   sub_layer_tier_flag;
			std::vector<uint8_t>   sub_layer_profile_idc;
			std::vector< std::vector< uint8_t>> sub_layer_profile_compatibility_flag;
			std::vector<uint8_t>   sub_layer_progressive_source_flag;
			std::vector<uint8_t>   sub_layer_interlaced_source_flag;
			std::vector<uint8_t>   sub_layer_non_packed_constraint_flag;
			std::vector<uint8_t>   sub_layer_frame_only_constraint_flag;
			std::vector<uint8_t>   sub_layer_level_idc;
		};

		struct SubLayerHrdParameters
		{
			std::vector<uint32_t> bit_rate_value_minus1;
			std::vector<uint32_t> cpb_size_value_minus1;
			std::vector<uint32_t> cpb_size_du_value_minus1;
			std::vector<uint32_t> bit_rate_du_value_minus1;
			std::vector<uint8_t>  cbr_flag;
		};


		struct ScalingListData
		{
			std::vector< std::vector< uint8_t>> scaling_list_pred_mode_flag;
			std::vector< std::vector< uint32_t>> scaling_list_pred_matrix_id_delta;
			std::vector< std::vector< uint32_t>> scaling_list_dc_coef_minus8;
			std::vector<std::vector< std::vector< uint32_t>>> scaling_list_delta_coef;
		};

		struct HrdParameters
		{
			uint8_t nal_hrd_parameters_present_flag = 0;
			uint8_t vcl_hrd_parameters_present_flag = 0;
			uint8_t sub_pic_hrd_params_present_flag = 0;
			uint8_t tick_divisor_minus2 = 0;
			uint8_t du_cpb_removal_delay_increment_length_minus1 = 0;
			uint8_t sub_pic_cpb_params_in_pic_timing_sei_flag = 0;
			uint8_t dpb_output_delay_du_length_minus1 = 0;
			uint8_t bit_rate_scale = 0;
			uint8_t cpb_size_scale = 0;
			uint8_t cpb_size_du_scale = 0;
			uint8_t initial_cpb_removal_delay_length_minus1 = 23;
			uint8_t au_cpb_removal_delay_length_minus1 = 23;
			uint8_t dpb_output_delay_length_minus1 = 23;
			std::vector<uint8_t>  fixed_pic_rate_general_flag;
			std::vector<uint8_t>  fixed_pic_rate_within_cvs_flag;
			std::vector<uint32_t> elemental_duration_in_tc_minus1;
			std::vector<uint8_t>  low_delay_hrd_flag;
			std::vector<uint32_t> cpb_cnt_minus1;
			std::vector<SubLayerHrdParameters> nal_sub_layer_hrd_parameters;
			std::vector<SubLayerHrdParameters> vcl_sub_layer_hrd_parameters;
		};

		struct ShortTermRefPicSet
		{
			uint8_t inter_ref_pic_set_prediction_flag = 0;
			uint32_t delta_idx_minus1 = 0;
			uint8_t delta_rps_sign = 0;
			uint32_t abs_delta_rps_minus1 = 0;
			std::vector<uint8_t> used_by_curr_pic_flag;
			std::vector<uint8_t> use_delta_flag;
			uint32_t num_negative_pics = 0;
			uint32_t num_positive_pics = 0;
			std::vector<uint32_t> delta_poc_s0_minus1;
			std::vector<uint8_t> used_by_curr_pic_s0_flag;
			std::vector<uint32_t> delta_poc_s1_minus1;
			std::vector<uint8_t> used_by_curr_pic_s1_flag;
		};

		struct RefPicListModification
		{
			uint8_t ref_pic_list_modification_flag_l0 = 0;
			std::vector<uint32_t> list_entry_l0;
			uint8_t ref_pic_list_modification_flag_l1 = 0;
			std::vector<uint32_t> list_entry_l1;
		};

		struct VuiParameters
		{
			uint8_t aspect_ratio_info_present_flag = 0;
			uint8_t aspect_ratio_idc = 0;
			uint16_t sar_width = 0;
			uint16_t sar_height = 0;
			uint8_t overscan_info_present_flag = 0;
			uint8_t overscan_appropriate_flag = 0;
			uint8_t video_signal_type_present_flag = 0;
			uint8_t video_format = 5;
			uint8_t video_full_range_flag = 0;
			uint8_t colour_description_present_flag = 0;
			uint8_t colour_primaries = 2;
			uint8_t transfer_characteristics = 2;
			uint8_t matrix_coeffs = 2;
			uint8_t chroma_loc_info_present_flag = 0;
			uint32_t chroma_sample_loc_type_top_field = 0;
			uint32_t chroma_sample_loc_type_bottom_field = 0;
			uint8_t neutral_chroma_indication_flag = 0;
			uint8_t field_seq_flag = 0;
			uint8_t frame_field_info_present_flag = 0;
			uint8_t default_display_window_flag = 0;
			uint32_t def_disp_win_left_offset = 0;
			uint32_t def_disp_win_right_offset = 0;
			uint32_t def_disp_win_top_offset = 0;
			uint32_t def_disp_win_bottom_offset = 0;
			uint8_t  vui_timing_info_present_flag = 0;
			uint32_t vui_num_units_in_tick = 0;
			uint32_t vui_time_scale = 0;
			uint8_t  vui_poc_proportional_to_timing_flag = 0;
			uint32_t vui_num_ticks_poc_diff_one_minus1 = 0;
			uint8_t  vui_hrd_parameters_present_flag = 0;
			HrdParameters hrd_parameters;
			uint8_t bitstream_restriction_flag = 0;
			uint8_t tiles_fixed_structure_flag = 0;
			uint8_t motion_vectors_over_pic_boundaries_flag = 0;
			uint8_t restricted_ref_pic_lists_flag = 0;
			uint32_t min_spatial_segmentation_idc = 0;
			uint32_t max_bytes_per_pic_denom = 2;
			uint32_t max_bits_per_min_cu_denom = 1;
			uint32_t log2_max_mv_length_horizontal = 15;
			uint32_t log2_max_mv_length_vertical = 15;
		};

		struct PredWeightTable
		{
			uint32_t luma_log2_weight_denom = 0;
			int32_t delta_chroma_log2_weight_denom = 0;
			std::vector<uint8_t> luma_weight_l0_flag;
			std::vector<uint8_t> chroma_weight_l0_flag;
			std::vector<int32_t> delta_luma_weight_l0;
			std::vector<int32_t> luma_offset_l0;
			std::vector<std::array<int32_t, 2>> delta_chroma_weight_l0;
			std::vector<std::array<int32_t, 2>> delta_chroma_offset_l0;
			std::vector<uint8_t> luma_weight_l1_flag;
			std::vector<uint8_t> chroma_weight_l1_flag;
			std::vector<int32_t> delta_luma_weight_l1;
			std::vector<int32_t> luma_offset_l1;
			std::vector<std::array<int32_t, 2>> delta_chroma_weight_l1;
			std::vector<std::array<int32_t, 2>> delta_chroma_offset_l1;
		};

		struct VPS 
		{
			NALHeader header = { NALUnitType::VPS, 0, 0 };
			uint8_t vps_video_parameter_set_id = 0;
			uint8_t vps_max_layers_minus1 = 0;
			uint8_t vps_max_sub_layers_minus1 = 0;
			uint8_t vps_temporal_id_nesting_flag = 0;
			ProfileTierLevel profile_tier_level;
			uint8_t vps_sub_layer_ordering_info_present_flag = 0;
			std::vector<uint32_t> vps_max_dec_pic_buffering_minus1;
			std::vector<uint32_t> vps_max_num_reorder_pics;
			std::vector<uint32_t> vps_max_latency_increase_plus1;
			uint8_t vps_max_layer_id = 0;
			uint32_t vps_num_layer_sets_minus1 = 0;
			std::vector<std::vector<uint8_t>> layer_id_included_flag;
			uint8_t vps_timing_info_present_flag = 0;
			uint32_t vps_num_units_in_tick = 0;
			uint32_t vps_time_scale = 0;
			uint8_t vps_poc_proportional_to_timing_flag = 0;
			uint32_t vps_num_ticks_poc_diff_one_minus1 = 0;
			uint32_t vps_num_hrd_parameters = 0;
			std::vector<uint32_t> hrd_layer_set_idx;
			std::vector<uint8_t> cprms_present_flag;
			std::vector<HrdParameters> hrd_parameters;
			uint8_t vps_extension_flag = 0;
		};


		struct SPS 
		{
			NALHeader header = { NALUnitType::SPS, 0, 0 };
			uint8_t sps_video_parameter_set_id = 0;
			uint8_t sps_max_sub_layers_minus1 = 0;
			uint8_t sps_temporal_id_nesting_flag = 0;
			ProfileTierLevel profile_tier_level;
			uint32_t sps_seq_parameter_set_id = 0;
			uint32_t chroma_format_idc = 0;
			uint8_t separate_colour_plane_flag = 0;
			uint32_t pic_width_in_luma_samples = 0;
			uint32_t pic_height_in_luma_samples = 0;
			uint8_t conformance_window_flag = 0;
			uint32_t conf_win_left_offset = 0;
			uint32_t conf_win_right_offset = 0;
			uint32_t conf_win_top_offset = 0;
			uint32_t conf_win_bottom_offset = 0;
			uint32_t bit_depth_luma_minus8 = 0;
			uint32_t bit_depth_chroma_minus8 = 0;
			uint32_t log2_max_pic_order_cnt_lsb_minus4 = 0;
			uint8_t sps_sub_layer_ordering_info_present_flag = 0;
			std::vector<uint32_t> sps_max_dec_pic_buffering_minus1;
			std::vector<uint32_t> sps_max_num_reorder_pics;
			std::vector<uint32_t> sps_max_latency_increase_plus1;
			uint32_t log2_min_luma_coding_block_size_minus3 = 0;
			uint32_t log2_diff_max_min_luma_coding_block_size = 0;
			uint32_t log2_min_transform_block_size_minus2 = 0;
			uint32_t log2_diff_max_min_transform_block_size = 0;
			uint32_t max_transform_hierarchy_depth_inter = 0;
			uint32_t max_transform_hierarchy_depth_intra = 0;
			uint8_t scaling_list_enabled_flag = 0;
			uint8_t sps_scaling_list_data_present_flag = 0;
			ScalingListData scaling_list_data;
			uint8_t amp_enabled_flag = 0;
			uint8_t sample_adaptive_offset_enabled_flag = 0;
			uint8_t pcm_enabled_flag = 0;
			uint8_t pcm_sample_bit_depth_luma_minus1 = 0;
			uint8_t pcm_sample_bit_depth_chroma_minus1 = 0;
			uint32_t log2_min_pcm_luma_coding_block_size_minus3 = 0;
			uint32_t log2_diff_max_min_pcm_luma_coding_block_size = 0;
			uint8_t pcm_loop_filter_disabled_flag = 0;
			uint32_t num_short_term_ref_pic_sets = 0;
			std::vector<ShortTermRefPicSet> short_term_ref_pic_set;
			uint8_t long_term_ref_pics_present_flag = 0;
			uint32_t num_long_term_ref_pics_sps = 0;
			std::vector<uint32_t> lt_ref_pic_poc_lsb_sps;
			std::vector<uint8_t> used_by_curr_pic_lt_sps_flag;
			uint8_t sps_temporal_mvp_enabled_flag = 0;
			uint8_t strong_intra_smoothing_enabled_flag = 0;
			uint8_t vui_parameters_present_flag = 0;
			VuiParameters vui_parameters;
			uint8_t sps_extension_flag = 0;
		};


		struct PPS 
		{
			NALHeader header = { NALUnitType::PPS, 0, 0 };
			uint32_t pps_pic_parameter_set_id = 0;
			uint32_t pps_seq_parameter_set_id = 0;
			uint8_t dependent_slice_segments_enabled_flag = 0;
			uint8_t output_flag_present_flag = 0;
			uint8_t num_extra_slice_header_bits = 0;
			uint8_t sign_data_hiding_flag = 0;
			uint8_t cabac_init_present_flag = 0;
			uint32_t num_ref_idx_l0_default_active_minus1 = 0;
			uint32_t num_ref_idx_l1_default_active_minus1 = 0;
			int32_t init_qp_minus26 = 0;
			uint8_t constrained_intra_pred_flag = 0;
			uint8_t transform_skip_enabled_flag = 0;
			uint8_t cu_qp_delta_enabled_flag = 0;
			uint32_t diff_cu_qp_delta_depth = 0;
			int32_t pps_cb_qp_offset = 0;
			int32_t pps_cr_qp_offset = 0;
			uint8_t pps_slice_chroma_qp_offsets_present_flag = 0;
			uint8_t weighted_pred_flag = 0;
			uint8_t weighted_bipred_flag = 0;
			uint8_t transquant_bypass_enabled_flag = 0;
			uint8_t tiles_enabled_flag = 0;
			uint8_t entropy_coding_sync_enabled_flag = 0;
			uint32_t num_tile_columns_minus1 = 0;
			uint32_t num_tile_rows_minus1 = 0;
			uint8_t uniform_spacing_flag = 1;
			std::vector<uint32_t> column_width_minus1;
			std::vector<uint32_t> row_height_minus1;
			uint8_t loop_filter_across_tiles_enabled_flag = 0;
			uint8_t pps_loop_filter_across_slices_enabled_flag = 0;
			uint8_t deblocking_filter_control_present_flag = 0;
			uint8_t deblocking_filter_override_enabled_flag = 0;
			uint8_t pps_deblocking_filter_disabled_flag = 0;
			uint32_t pps_beta_offset_div2 = 0;
			uint32_t pps_tc_offset_div2 = 0;
			uint8_t pps_scaling_list_data_present_flag = 0;
			ScalingListData scaling_list_data;
			uint8_t lists_modification_present_flag = 0;
			int32_t log2_parallel_merge_level_minus2 = 0;
			uint8_t slice_segment_header_extension_present_flag = 0;
			uint8_t pps_extension_flag = 0;
		};


		enum class SliceType : uint32_t
		{
			B = 0,
			P = 1,
			I = 2,
			None = 3
		};

		struct Slice 
		{
			NALHeader header;
			uint8_t first_slice_segment_in_pic_flag = 0;
			uint8_t no_output_of_prior_pics_flag = 0;
			uint32_t slice_pic_parameter_set_id = 0;
			uint8_t dependent_slice_segment_flag = 0;
			uint32_t slice_segment_address = 0;
			std::vector<uint32_t> slice_reserved_undetermined_flag;
			SliceType slice_type = SliceType::None;
			uint8_t pic_output_flag = 1;
			uint8_t colour_plane_id = 0;
			uint32_t slice_pic_order_cnt_lsb = 0;
			uint8_t short_term_ref_pic_set_sps_flag = 0;
			ShortTermRefPicSet short_term_ref_pic_set;
			uint8_t short_term_ref_pic_set_idx = 0;
			uint32_t num_long_term_sps = 0;
			uint32_t num_long_term_pics = 0;
			std::vector<uint32_t> lt_idx_sps;
			std::vector<uint32_t> poc_lsb_lt;
			std::vector<uint8_t> used_by_curr_pic_lt_flag;
			std::vector<uint8_t> delta_poc_msb_present_flag;
			std::vector<uint32_t> delta_poc_msb_cycle_lt;
			uint8_t slice_temporal_mvp_enabled_flag = 0;
			uint8_t slice_sao_luma_flag = 1;
			uint8_t slice_sao_chroma_flag = 0;
			uint8_t num_ref_idx_active_override_flag = 0;
			uint32_t num_ref_idx_l0_active_minus1 = 0;
			uint32_t num_ref_idx_l1_active_minus1 = 0;
			RefPicListModification ref_pic_lists_modification;
			uint8_t mvd_l1_zero_flag = 0;
			uint8_t cabac_init_flag = 0;
			uint8_t collocated_from_l0_flag = 0;
			uint32_t collocated_ref_idx = 0;
			PredWeightTable pred_weight_table;
			uint32_t five_minus_max_num_merge_cand = 0;
			int32_t slice_qp_delta = 0;
			int32_t slice_cb_qp_offset = 0;
			int32_t slice_cr_qp_offset = 0;
			uint8_t deblocking_filter_override_flag = 0;
			uint8_t slice_deblocking_filter_disabled_flag = 0;
			int32_t slice_beta_offset_div2 = 0;
			int32_t slice_tc_offset_div2 = 0;
			int32_t slice_loop_filter_across_slices_enabled_flag = 0;
			uint32_t num_entry_point_offsets = 0;
			uint32_t offset_len_minus1 = 0;
			std::vector<uint32_t> entry_point_offset_minus1;
			uint32_t slice_segment_header_extension_length = 0;
			std::vector<uint8_t> slice_segment_header_extension_data_byte;
		};
	}
}

