LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# draco.a
#
# Crossplatform Texture Compression for SimulCasterClients (PC and Android)
#--------------------------------------------------------
include $(CLEAR_VARS)				# clean everything up to prepare for a module

LOCAL_MODULE    := draco	        # generate draco.a

include $(LOCAL_PATH)/../../../client/cflags.mk

LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/..
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../draco
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../draco/src
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_FILES  := 	../../draco/src/draco/animation/keyframe_animation.cc\
                        ../../draco/src/draco/animation/keyframe_animation_decoder.cc\
                        ../../draco/src/draco/animation/keyframe_animation_encoder.cc
LOCAL_SRC_FILES  += 	../../draco/src/draco/attributes/attribute_octahedron_transform.cc\
                        ../../draco/src/draco/attributes/attribute_quantization_transform.cc\
                        ../../draco/src/draco/attributes/attribute_transform.cc\
                        ../../draco/src/draco/attributes/geometry_attribute.cc\
                        ../../draco/src/draco/attributes/point_attribute.cc
LOCAL_SRC_FILES  += 	../../draco/src/draco/compression/decode.cc\
                        ../../draco/src/draco/compression/encode.cc\
                        ../../draco/src/draco/compression/expert_encode.cc\
						../../draco/src/draco/compression/attributes/attributes_decoder.cc\
                        ../../draco/src/draco/compression/attributes/attributes_encoder.cc\
                        ../../draco/src/draco/compression/attributes/kd_tree_attributes_decoder.cc\
                        ../../draco/src/draco/compression/attributes/kd_tree_attributes_encoder.cc\
                        ../../draco/src/draco/compression/attributes/sequential_attribute_decoder.cc\
                        ../../draco/src/draco/compression/attributes/sequential_attribute_decoders_controller.cc\
                        ../../draco/src/draco/compression/attributes/sequential_attribute_encoder.cc\
                        ../../draco/src/draco/compression/attributes/sequential_attribute_encoders_controller.cc\
                        ../../draco/src/draco/compression/attributes/sequential_integer_attribute_decoder.cc\
                        ../../draco/src/draco/compression/attributes/sequential_integer_attribute_encoder.cc\
                        ../../draco/src/draco/compression/attributes/sequential_normal_attribute_decoder.cc\
                        ../../draco/src/draco/compression/attributes/sequential_normal_attribute_encoder.cc\
                        ../../draco/src/draco/compression/attributes/sequential_quantization_attribute_decoder.cc\
                        ../../draco/src/draco/compression/attributes/sequential_quantization_attribute_encoder.cc\
                        ../../draco/src/draco/compression/attributes/prediction_schemes/prediction_scheme_encoder_factory.cc\
                        ../../draco/src/draco/compression/bit_coders/adaptive_rans_bit_decoder.cc\
                        ../../draco/src/draco/compression/bit_coders/adaptive_rans_bit_encoder.cc\
                        ../../draco/src/draco/compression/bit_coders/direct_bit_decoder.cc\
                        ../../draco/src/draco/compression/bit_coders/direct_bit_encoder.cc\
                        ../../draco/src/draco/compression/bit_coders/rans_bit_decoder.cc\
                        ../../draco/src/draco/compression/bit_coders/rans_bit_encoder.cc\
                        ../../draco/src/draco/compression/bit_coders/symbol_bit_decoder.cc\
                        ../../draco/src/draco/compression/bit_coders/symbol_bit_encoder.cc\
                        ../../draco/src/draco/compression/entropy/shannon_entropy.cc\
                        ../../draco/src/draco/compression/entropy/symbol_decoding.cc\
                        ../../draco/src/draco/compression/entropy/symbol_encoding.cc\
                        ../../draco/src/draco/compression/mesh/mesh_decoder.cc\
                        ../../draco/src/draco/compression/mesh/mesh_edgebreaker_decoder.cc\
                        ../../draco/src/draco/compression/mesh/mesh_edgebreaker_decoder_impl.cc\
                        ../../draco/src/draco/compression/mesh/mesh_edgebreaker_encoder.cc\
                        ../../draco/src/draco/compression/mesh/mesh_edgebreaker_encoder_impl.cc\
                        ../../draco/src/draco/compression/mesh/mesh_encoder.cc\
                        ../../draco/src/draco/compression/mesh/mesh_sequential_decoder.cc\
                        ../../draco/src/draco/compression/mesh/mesh_sequential_encoder.cc\
                        ../../draco/src/draco/compression/point_cloud/point_cloud_decoder.cc\
                        ../../draco/src/draco/compression/point_cloud/point_cloud_encoder.cc\
                        ../../draco/src/draco/compression/point_cloud/point_cloud_kd_tree_decoder.cc\
                        ../../draco/src/draco/compression/point_cloud/point_cloud_kd_tree_encoder.cc\
                        ../../draco/src/draco/compression/point_cloud/point_cloud_sequential_decoder.cc\
                        ../../draco/src/draco/compression/point_cloud/point_cloud_sequential_encoder.cc\
                        ../../draco/src/draco/compression/point_cloud/algorithms/dynamic_integer_points_kd_tree_decoder.cc\
                        ../../draco/src/draco/compression/point_cloud/algorithms/dynamic_integer_points_kd_tree_encoder.cc\
                        ../../draco/src/draco/compression/point_cloud/algorithms/float_points_tree_decoder.cc\
                        ../../draco/src/draco/compression/point_cloud/algorithms/float_points_tree_encoder.cc\
                        ../../draco/src/draco/compression/point_cloud/algorithms/integer_points_kd_tree_decoder.cc\
                        ../../draco/src/draco/compression/point_cloud/algorithms/integer_points_kd_tree_encoder.cc
LOCAL_SRC_FILES  += 	../../draco/src/draco/core/bit_utils.cc\
                        ../../draco/src/draco/core/bounding_box.cc\
                        ../../draco/src/draco/core/cycle_timer.cc\
                        ../../draco/src/draco/core/data_buffer.cc\
                        ../../draco/src/draco/core/decoder_buffer.cc\
                        ../../draco/src/draco/core/divide.cc\
                        ../../draco/src/draco/core/draco_types.cc\
                        ../../draco/src/draco/core/encoder_buffer.cc\
                        ../../draco/src/draco/core/hash_utils.cc\
                        ../../draco/src/draco/core/options.cc\
                        ../../draco/src/draco/core/quantization_utils.cc
LOCAL_SRC_FILES  += 	../../draco/src/draco/io/file_reader_factory.cc\
                        ../../draco/src/draco/io/file_utils.cc\
                        ../../draco/src/draco/io/file_writer_factory.cc\
                        ../../draco/src/draco/io/file_writer_utils.cc\
                        ../../draco/src/draco/io/mesh_io.cc\
                        ../../draco/src/draco/io/obj_decoder.cc\
                        ../../draco/src/draco/io/obj_encoder.cc\
                        ../../draco/src/draco/io/parser_utils.cc\
                        ../../draco/src/draco/io/ply_decoder.cc\
                        ../../draco/src/draco/io/ply_encoder.cc\
                        ../../draco/src/draco/io/ply_reader.cc\
                        ../../draco/src/draco/io/point_cloud_io.cc\
                        ../../draco/src/draco/io/stdio_file_reader.cc\
                        ../../draco/src/draco/io/stdio_file_writer.cc
LOCAL_SRC_FILES  += 	../../draco/src/draco/mesh/corner_table.cc\
                        ../../draco/src/draco/mesh/mesh.cc\
                        ../../draco/src/draco/mesh/mesh_are_equivalent.cc\
                        ../../draco/src/draco/mesh/mesh_attribute_corner_table.cc\
                        ../../draco/src/draco/mesh/mesh_cleanup.cc\
                        ../../draco/src/draco/mesh/mesh_misc_functions.cc\
                        ../../draco/src/draco/mesh/mesh_stripifier.cc\
                        ../../draco/src/draco/mesh/triangle_soup_mesh_builder.cc
LOCAL_SRC_FILES  += 	../../draco/src/draco/metadata/geometry_metadata.cc\
                        ../../draco/src/draco/metadata/metadata.cc\
                        ../../draco/src/draco/metadata/metadata_decoder.cc\
                        ../../draco/src/draco/metadata/metadata_encoder.cc
LOCAL_SRC_FILES  += 	../../draco/src/draco/point_cloud/point_cloud.cc\
                        ../../draco/src/draco/point_cloud/point_cloud_builder.cc

LOCAL_CFLAGS += -D__ANDROID__
LOCAL_CPPFLAGS += -Wall -Wextra -Wno-unused-local-typedefs -Wno-unused-value -Wno-unused-parameter -Wno-unused-variable -Wno-reorder
LOCAL_CPP_FEATURES += exceptions
include $(BUILD_STATIC_LIBRARY)		# start building based on everything since CLEAR_VARS