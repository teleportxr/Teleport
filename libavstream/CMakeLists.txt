cmake_minimum_required(VERSION 3.8)
project(libavstream )
set(LIBAV_CUDA_SAMPLES_DIR "C:/ProgramData/NVIDIA Corporation/CUDA Samples/v11.5" CACHE STRING "")
set(LIBAV_CUDA_DIR "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.5" CACHE STRING "")
set(CMAKE_CUDA_COMPILER "${LIBAV_CUDA_DIR}/bin/nvcc.exe" )
# Build options
option(LIBAV_USE_DYNAMIC_RUNTIME "Use dynamic (MD) runtime?" OFF)
option(LIBAV_BUILD_SHARED_LIBS "Build shared library" OFF)
option(LIBAV_USE_EFP "Use EFP?" ON)
option(LIBAV_USE_CUDA "Use CUDA for encoding?" ON)
option(TELEPORT_LIBAV_MEASURE_PIPELINE_BANDWIDTH "Measure bandwidth of pipeline nodes?" OFF)
set(LIBAVS)
if(NOT ANDROID)
	option(LIBAV_BUILD_TESTS "Build tests" OFF)
endif()

# Private source
set(src_private_root
	src/audiodecoder.cpp
	src/audioencoder.cpp
	src/context.cpp
	src/decoder.cpp
	src/encoder.cpp
	src/file.cpp
	src/genericdecoder.cpp
	src/genericencoder.cpp
	src/geometrydecoder.cpp
	src/geometryencoder.cpp
	src/lock_free_queue.cpp
	src/logger.cpp
	src/surface.cpp
	src/mesh.cpp
	src/buffer.cpp
	src/node.cpp
	src/nullsink.cpp
	src/pipeline.cpp
	src/queue.cpp
	src/memory.cpp
	src/forwarder.cpp
	src/packetizer.cpp
	src/singlequeue.cpp
	src/tagdatadecoder.cpp
	src/libraryloader.cpp
	src/timer.cpp
)

set(src_private_network
	src/network/networksink.cpp
	src/network/webrtc_networksink.cpp
	src/network/networksource.cpp
	src/network/webrtc_networksource.cpp
	src/network/webrtc_common.cpp
	src/network/webrtc_common.h
)
if(LIBAV_USE_CUDA)
	set(src_private_api
		src/api/cuda.cpp
		src/api/cuda_dx12.cpp
	)
endif()
set(src_private_stream
	src/stream/parser_avc.cpp
	src/stream/parser_geo.cpp
	src/stream/parser.cpp
)
if(LIBAV_USE_CUDA)
	set(src_private_stream
		${src_private_stream}
		src/decoders/dec_nvidia.cpp
		src/encoders/enc_nvidia.cpp
		src/decoders/dec_nvidia.cu
		src/encoders/enc_nvidia.cu
	)
endif()
set(hdr_private_root
	src/common_p.hpp
	src/abi_p.hpp
    src/platform.hpp
	src/logger.hpp
	src/context_p.hpp
	src/pipeline_p.hpp
	src/node_p.hpp
	src/decoder_p.hpp
	src/geometrydecoder_p.hpp
	src/encoder_p.hpp
	src/geometryencoder_p.hpp
	src/audioencoder_p.h
	src/audiodecoder_p.h
	src/surface_p.hpp
	src/mesh_p.hpp
	src/buffer_p.hpp
	src/queue_p.hpp
	src/file_p.hpp
	src/forwarder_p.hpp
	src/packetizer_p.hpp
	src/profiling.h
	src/libraryloader.hpp)
if(LIBAV_USE_CUDA)
	set(hdr_private_api 
		src/api/cuda.hpp
		src/api/cuda_dx12.hpp)
endif()
set(hdr_private_stream
	src/stream/parser_avc.hpp
	src/stream/parser_geo.hpp)
set(hdr_private_parsers
	src/parsers/nalu_parser_interface.hpp
	src/parsers/nalu_parser_h264.hpp
	src/parsers/nalu_parser_h265.hpp)
set(hdr_private_network
	src/network/packetformat.hpp
	src/network/serial.h)
	
if(LIBAV_USE_CUDA)
	set(hdr_private_decoders
		src/decoders/dec_nvidia.hpp)
	set(hdr_private_encoders
		src/encoders/enc_nvidia.hpp)
endif()
set(hdr_private_util
	src/util/binaryio.hpp
	src/util/bytebuffer.hpp
	src/util/jitterbuffer.hpp
	src/util/ringbuffer.hpp
	src/util/misc.hpp)
set(src_private_audio
	src/audio/audiotarget.cpp)

source_group("Source Files\\api" src FILES ${src_private_api} )
source_group("Source Files\\network" src FILES ${src_private_network} )
source_group("Source Files\\stream" src FILES ${src_private_stream} )
source_group("Source Files\\util" src FILES ${src_private_util} )
source_group("Source Files\\audio" src FILES ${src_private_audio} )

set(hdr_private ${hdr_private_root} ${hdr_private_api} ${hdr_private_stream} ${hdr_private_parsers} ${hdr_private_network} ${hdr_private_decoders} ${hdr_private_encoders} ${hdr_private_util} )

source_group("Private Headers" src FILES ${hdr_private_root} )
source_group("Private Headers\\api" src FILES ${hdr_private_api} )
source_group("Private Headers\\stream" src FILES ${hdr_private_stream} )
source_group("Private Headers\\parsers" src FILES ${hdr_private_parsers} )
source_group("Private Headers\\network" src FILES ${hdr_private_network} )
source_group("Private Headers\\decoders" src FILES ${hdr_private_decoders} )
source_group("Private Headers\\encoders" src FILES ${hdr_private_encoders} )
source_group("Private Headers\\util" src FILES ${hdr_private_util} )

# Public source
set(hdr_public_root
	include/libavstream/audiodecoder.h
	include/libavstream/audioencoder.h
	include/libavstream/buffer.hpp
	include/libavstream/common.hpp
	include/libavstream/context.hpp
	include/libavstream/decoder.hpp
	include/libavstream/tagdatadecoder.hpp
	include/libavstream/encoder.hpp
	include/libavstream/file.hpp
	include/libavstream/forwarder.hpp
	include/libavstream/genericdecoder.h
	include/libavstream/genericencoder.h
	include/libavstream/geometrydecoder.hpp
	include/libavstream/geometryencoder.hpp
	include/libavstream/interfaces.hpp
	include/libavstream/libavstream.hpp
	include/libavstream/lock_free_queue.h
	include/libavstream/memory.hpp
	include/libavstream/mesh.hpp
	include/libavstream/node.hpp
	include/libavstream/nullsink.hpp
	include/libavstream/packetizer.hpp
	include/libavstream/pipeline.hpp
	include/libavstream/queue.hpp
	include/libavstream/singlequeue.h
	include/libavstream/surface.hpp
	include/libavstream/timer.hpp
	)
set(hdr_public_network
	include/libavstream/network/networksink.h
	include/libavstream/network/webrtc_networksink.h
	include/libavstream/network/webrtc_observers.h
	include/libavstream/network/networksource.h
	include/libavstream/network/webrtc_networksource.h)
set(src_public_stream
	include/libavstream/stream/parser_interface.hpp)
set(src_public_decoders
	include/libavstream/decoders/dec_interface.h)
set(src_public_encoders
	include/libavstream/encoders/enc_interface.h)
set(src_public_surfaces
	include/libavstream/surfaces/surface_interface.hpp
	include/libavstream/surfaces/surface_dx11.hpp
	include/libavstream/surfaces/surface_dx12.hpp
)
if(TELEPORT_CLIENT_USE_VULKAN)
	set(src_public_surfaces ${src_public_surfaces} include/libavstream/surfaces/surface_vulkan.hpp)
endif()
set(src_public_platforms
	include/libavstream/platforms/platform_common.hpp
	include/libavstream/platforms/platform_windows.hpp
)
set(hdr_public_geometry
	include/libavstream/geometry/mesh_interface.hpp
	include/libavstream/geometry/GeometryParserInterface.h
	include/libavstream/geometry/material_interface.hpp
	include/libavstream/geometry/material_extensions.h
)

set(hdr_public_audio
	include/libavstream/audio/audio_interface.h
	include/libavstream/audio/audiotarget.h
)
	

set(hdr_export
	export_include/libavstream/common_exports.h
	export_include/libavstream/common_maths.h
	export_include/libavstream/common_networking.h
	export_include/libavstream/common_packing.h
	export_include/libavstream/common_packing.h
	export_include/libavstream/material_exports.h
	export_include/libavstream/node.h
	export_include/libavstream/abi.h
)
if(NOT TELEPORT_SERVER)
	set(hdr_public_root ${hdr_public_root} include/libavstream/httputil.hpp)
	set(src_private_root ${src_private_root} src/httputil.cpp)
endif()

set(hdr_public ${hdr_public_root} ${hdr_public_network} ${src_public_stream} ${src_public_decoders} ${src_public_encoders} ${src_public_surfaces} ${src_public_platforms} ${hdr_public_geometry} ${hdr_public_audio} ${hdr_export})
source_group("Public Headers" src FILES ${hdr_public_root} )
source_group("Public Headers\\stream" src FILES ${src_public_stream} )
source_group("Public Headers\\decoders" src FILES ${src_public_decoders} )
source_group("Public Headers\\encoders" src FILES ${src_public_encoders} )
source_group("Public Headers\\surfaces" src FILES ${src_public_surfaces} )
source_group("Public Headers\\platforms" src FILES ${src_public_platforms} )
source_group("Public Headers\\geometry" src FILES ${hdr_public_geometry} )
source_group("Public Headers\\audio" src FILES ${hdr_public_audio} )
source_group("Public Headers\\network" src FILES ${hdr_public_network} )
source_group("Public Headers\\export" src FILES ${hdr_export} )

set(src_private ${src_private_root} ${src_private_api} ${src_private_network} ${src_private_stream} ${src_private_util} ${src_private_audio})

# Windows platform
if(WIN32)
	set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Ob0 /Od /D_DEBUG /DDEBUG /RTC1")
	set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2 /DNDEBUG /Zi")
	if(${LIBAV_USE_DYNAMIC_RUNTIME})
		set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
		set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
	else()
		set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
		set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	endif()
	set( EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/eop/ )
	set( LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lop/ )
	set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/ )
	set( CMAKE_CACHEFILE_DIR ${CMAKE_BINARY_DIR}/cachefiles/ )
	set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lod/ )
	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/ )
	set( CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/ )
	set(src_platform 
		src/platforms/platform_windows.cpp
		include/libavstream/platforms/platform_windows.hpp
		src/surfaces/surface_dx11.cpp
		include/libavstream/surfaces/surface_dx11.hpp
		src/surfaces/surface_dx12.cpp
		include/libavstream/surfaces/surface_dx12.hpp
	)
if(TELEPORT_CLIENT_USE_VULKAN)
	set(src_platform ${src_platform} src/surfaces/surface_vulkan.cpp include/libavstream/surfaces/surface_vulkan.hpp)
endif()
	set(def_platform PLATFORM_WINDOWS UNICODE _WIN32_WINNT=0x0601 INIT_CUDA_D3D11=1)
	find_program(xxd NAMES xxd.exe HINTS tools/win32)
	
# Android platform
elseif(ANDROID)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Og -DNDEBUG")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Og -D_DEBUG -DDEBUG")
	set(src_platform
		src/platforms/platform_posix.cpp
		src/platforms/platform_posix.hpp
	)
	set(def_platform PLATFORM_ANDROID)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Og -DNDEBUG")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Og -D_DEBUG -DDEBUG")
	set(src_platform src/platforms/platform_posix.cpp)
	set(def_platform PLATFORM_LINUX)
	find_program(xxd NAMES xxd)
else()
message("Unsupported platform for libavstream!")
return()
endif() 

if(LIBAV_USE_CUDA)
	# CUDA kernels
	# _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH because newer MS compiler expects CUDA 12. 
	add_custom_command(
		OUTPUT enc_nvidia.cubin enc_nvidia.cubin.inl
		COMMAND ${CMAKE_CUDA_COMPILER} -allow-unsupported-compiler -fatbin ${CMAKE_CURRENT_SOURCE_DIR}/src/encoders/enc_nvidia.cu -o enc_nvidia.cubin -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH=1
		COMMAND ${xxd} -i enc_nvidia.cubin enc_nvidia.cubin.inl
		DEPENDS src/encoders/enc_nvidia.cu
	)

	add_custom_command(
		OUTPUT dec_nvidia.cubin dec_nvidia.cubin.inl
		COMMAND ${CMAKE_CUDA_COMPILER} -allow-unsupported-compiler -fatbin ${CMAKE_CURRENT_SOURCE_DIR}/src/decoders/dec_nvidia.cu -o dec_nvidia.cubin -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH=1
		COMMAND ${xxd} -i dec_nvidia.cubin dec_nvidia.cubin.inl
		DEPENDS src/decoders/dec_nvidia.cu
	)
	add_custom_target(
		cuda_kernels
		DEPENDS enc_nvidia.cubin enc_nvidia.cubin.inl dec_nvidia.cubin dec_nvidia.cubin.inl SOURCES src/encoders/enc_nvidia.cu src/decoders/dec_nvidia.cu

	)
	set_target_properties(cuda_kernels PROPERTIES FOLDER AVStream)
endif()
set(CMAKE_CXX_STANDARD 17 )
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_library(libavstream STATIC ${hdr_private} ${src_private} ${hdr_public} ${src_platform})

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	target_compile_options(libavstream PRIVATE -fPIC)
endif()
#set_target_runtime(libavstream static)
set_target_properties(libavstream PROPERTIES PREFIX "")
set_target_properties(libavstream PROPERTIES FOLDER AVStream)

set(def_webrtc			  WEBRTC_WIN _CRT_SECURE_NO_DEPRECATE _SCL_SECURE_NO_DEPRECATE _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
target_compile_definitions(libavstream PRIVATE ${def_platform} )
target_compile_definitions(libavstream PRIVATE ${def_webrtc})
set(webrtc_include_paths ${CMAKE_SOURCE_DIR}/thirdparty/webrtc/include
	${CMAKE_SOURCE_DIR}/thirdparty/webrtc/include/third_party/libyuv/include
	${CMAKE_SOURCE_DIR}/thirdparty/webrtc/include/third_party/abseil-cpp)

if(LIBAV_USE_EFP)
	# ElasticFrameProtocol
	add_subdirectory(thirdparty/efp)
	set_target_properties( efp PROPERTIES FOLDER AVStream/thirdparty/efp)
	set_target_properties( efp_shared PROPERTIES FOLDER AVStream/thirdparty/efp)
	set_target_properties( test_efp_c_api PROPERTIES FOLDER AVStream/thirdparty/efp)
	set_target_properties( efptests PROPERTIES FOLDER AVStream/thirdparty/efp)
	set(efp_include_paths ${efp_SOURCE_DIR} ${efp_BINARY_DIR})
	#message(WARNING efp_include_paths ${efp_include_paths})
endif()

if(TELEPORT_TRACY_PROFILING)
	target_include_directories(libavstream PUBLIC ${CMAKE_BINARY_DIR}/_deps/tracy-src/public)
	target_compile_definitions(libavstream PRIVATE TRACY_ENABLE=1 )
endif()

# don't want curl tests.
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
if(TELEPORT_CLIENT )
    set(BUILD_CURL_EXE OFF CACHE BOOL "")
    set(CURL_STATIC_CRT ON CACHE BOOL "")
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
	set(CURL_USE_SCHANNEL ON CACHE BOOL "")
    add_subdirectory(thirdparty/curl)
	target_compile_definitions(libcurl PRIVATE BUILDING_LIBCURL=1) 
	set(curl_include_paths "${CURL_SOURCE_DIR}/include" ${CURL_BINARY_DIR})
	set(curl_libraries libcurl)
	RecurseApplyFolder(${CMAKE_SOURCE_DIR}/libavstream/thirdparty/curl thirdparty/curl)
	target_compile_definitions(libavstream PUBLIC TELEPORT_CLIENT=1) 
endif()
target_include_directories(libavstream PRIVATE src PRIVATE ${CMAKE_SOURCE_DIR}
														${CMAKE_CURRENT_BINARY_DIR}
														${webrtc_include_paths}
														${efp_include_paths}
														include
														#${PTHREAD_INCLUDE_DIR}
														PUBLIC include include/libavstream export_include
														${curl_include_paths})
target_compile_features(libavstream PRIVATE cxx_std_17)

if(LIBAAV_BUILD_SHARED_LIBS)
	target_compile_definitions(libavstream
		PUBLIC LIBAVSTREAM_SHARED
		PRIVATE LIBAVSTREAM_EXPORTS
	)
endif()
target_compile_definitions(libavstream PRIVATE LIBAV_USE_CUDA=$<BOOL:${LIBAV_USE_CUDA}> )
target_compile_definitions(libavstream PRIVATE TELEPORT_LIBAV_MEASURE_PIPELINE_BANDWIDTH=$<BOOL:${TELEPORT_LIBAV_MEASURE_PIPELINE_BANDWIDTH}>)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	target_compile_definitions(libavstream PRIVATE PLATFORM_64BIT)
endif()

# Third party dependencies
	add_subdirectory(thirdparty/asio)
if(LIBAV_USE_CUDA)
	add_subdirectory(thirdparty/nv)
endif()

	# Tests
	if(LIBAV_BUILD_TESTS)
		add_subdirectory(thirdparty/cxxopts)
		add_subdirectory(tests)
	endif()

#Include CUDA library location.
if(LIBAV_USE_CUDA)
	target_link_directories(libavstream PUBLIC "${LIBAV_CUDA_DIR}/lib/x64")
	target_link_directories(libavstream PUBLIC "${LIBAV_CUDA_DIR}/lib64")
	target_link_libraries(libavstream cuda cudart nv)
endif()

#fmt
target_include_directories(libavstream PUBLIC ${CMAKE_SOURCE_DIR}/firstparty/Platform/External/fmt/include )
#Platform etc
target_include_directories(libavstream PUBLIC ${CMAKE_SOURCE_DIR}/firstparty/ )
target_include_directories(libavstream PUBLIC export_include )

target_link_libraries(libavstream asio efp ${curl_libraries})


# Build CUDA kernels on PC platforms only
if(NOT ANDROID)
	if(LIBAV_USE_CUDA)
		add_dependencies(libavstream cuda_kernels)
		target_include_directories(libavstream PRIVATE "${LIBAV_CUDA_SAMPLES_DIR}/common/inc")
		target_include_directories(libavstream PRIVATE "${LIBAV_CUDA_DIR}/include")
	endif()
    if (Vulkan_FOUND)
        target_include_directories(libavstream PUBLIC ${Vulkan_INCLUDE_DIRS})
    endif()
endif()

target_include_directories(libavstream PRIVATE "${CMAKE_SOURCE_DIR}/firstparty/Platform/External")
target_include_directories(libavstream PRIVATE "${CMAKE_SOURCE_DIR}/firstparty")

# linking with libdatachannel for webrtc support
target_compile_definitions(libavstream PRIVATE TELEPORT_DATACHANNEL_WEBRTC=$<BOOL:${TELEPORT_DATACHANNEL_WEBRTC}>)
if(TELEPORT_DATACHANNEL_WEBRTC)
	target_include_directories(libavstream PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/libdatachannel/include")
	target_link_directories(libavstream PUBLIC "${CMAKE_BINARY_DIR}/thirdparty/libdatachannel/${CONFIG_NAME}")
	target_link_libraries(libavstream datachannel-static)
	if(TELEPORT_WINDOWS)
		target_link_libraries(libavstream legacy_stdio_definitions.lib)
	endif()
endif()
target_include_directories(libavstream PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/libdatachannel/deps/json/single_include")
target_compile_definitions( libavstream PUBLIC FMT_HEADER_ONLY )

# Unit testing.
if(TELEPORT_TEST)
	find_package(Catch2 REQUIRED)
	add_executable(libavstream_test src/test/test.cpp)
	set_target_properties(libavstream_test PROPERTIES FOLDER AVStream)
	target_link_libraries(libavstream_test Catch2WithMain libavstream efp fmt LibDataChannel::LibDataChannelStatic )
endif()
