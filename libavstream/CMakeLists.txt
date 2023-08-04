cmake_minimum_required(VERSION 3.8)
project(libavstream )
set(LIBAV_CUDA_SAMPLES_DIR "C:/ProgramData/NVIDIA Corporation/CUDA Samples/v11.5" CACHE STRING "")
set(LIBAV_CUDA_DIR "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.5" CACHE STRING "")
set(CMAKE_CUDA_COMPILER "${LIBAV_CUDA_DIR}/bin/nvcc.exe" CACHE STRING "")
# Build options
option(LIBAV_USE_DYNAMIC_RUNTIME "Use dynamic (MD) runtime?" OFF)
option(LIBAV_BUILD_SHARED_LIBS "Build shared library" OFF)
option(LIBAV_USE_SRT "Use SRT?" ON)
option(LIBAV_USE_EFP "Use EFP?" ON)
option(LIBAV_USE_CUDA "Use CUDA for encoding?" ON)
set(LIBAVS)
if(NOT ANDROID)
	option(LIBAV_BUILD_TESTS "Build tests" OFF)
endif()

# Private source
set(src_private_root
	src/context.cpp
	src/pipeline.cpp
	src/node.cpp
	src/decoder.cpp
	src/tagdatadecoder.cpp
	src/genericdecoder.cpp
	src/genericencoder.cpp
	src/geometrydecoder.cpp
	src/encoder.cpp
	src/geometryencoder.cpp
	src/audioencoder.cpp
	src/audiodecoder.cpp
	src/surface.cpp
	src/mesh.cpp
	src/buffer.cpp
	src/queue.cpp
	src/file.cpp
	src/memory.cpp
	src/forwarder.cpp
	src/packetizer.cpp
	src/singlequeue.cpp
	src/nullsink.cpp
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

if(LIBAV_USE_SRT)

set(src_private_network
	${src_private_network}
	src/network/srt_efp_networksink.cpp
	src/network/srt_efp_networksource.cpp
	)
endif()

set(src_private_api
	src/api/cuda.cpp
	src/api/cuda_dx12.cpp
)
set(src_private_stream
	src/stream/parser_avc.cpp
	src/stream/parser_geo.cpp
	src/stream/parser.cpp
	src/decoders/dec_nvidia.cpp
	src/encoders/enc_nvidia.cpp
	src/decoders/dec_nvidia.cu
	src/encoders/enc_nvidia.cu
)
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
	src/libraryloader.hpp)

set(hdr_private_api 
	src/api/cuda.hpp
    src/api/cuda_dx12.hpp)
set(hdr_private_stream
	src/stream/parser_avc.hpp
	src/stream/parser_geo.hpp)
set(hdr_private_parsers
	src/parsers/nalu_parser_interface.hpp
	src/parsers/nalu_parser_h264.hpp
	src/parsers/nalu_parser_h265.hpp)
set(hdr_private_network
	src/network/packetformat.hpp
	src/network/serial.hpp)

if(LIBAV_SRT)
	set(hdr_private_network
		${hdr_private_network}
		src/network/networksink_p.hpp
		src/network/networksource_p.hpp
		)
endif()

set(hdr_private_decoders
	src/decoders/dec_nvidia.hpp)
set(hdr_private_encoders
	src/encoders/enc_nvidia.hpp)
set(hdr_private_util
	src/util/binaryio.hpp
	src/util/bytebuffer.hpp
	src/util/jitterbuffer.hpp
	src/util/ringbuffer.hpp
	src/util/misc.hpp
	src/util/srtutil.h)
set(src_private_util
	src/util/srtutil.cpp)
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
	include/libavstream/abi.hpp
	include/libavstream/audiodecoder.h
	include/libavstream/audioencoder.h
	include/libavstream/buffer.hpp
	include/libavstream/common.hpp
	include/libavstream/common_input.h
	include/libavstream/common_maths.h
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
	include/libavstream/network/srt_efp_networksink.h
	include/libavstream/network/webrtc_networksink.h
	include/libavstream/network/webrtc_observers.h
	include/libavstream/network/networksource.h
	include/libavstream/network/srt_efp_networksource.h
	include/libavstream/network/webrtc_networksource.h)
set(src_public_stream
	include/libavstream/stream/parser_interface.hpp)
set(src_public_decoders
	include/libavstream/decoders/dec_interface.hpp)
set(src_public_encoders
	include/libavstream/encoders/enc_interface.hpp)
set(src_public_surfaces
	include/libavstream/surfaces/surface_interface.hpp
	include/libavstream/surfaces/surface_dx11.hpp
	include/libavstream/surfaces/surface_dx12.hpp
)
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
	
if(NOT TELEPORT_SERVER)
	set(hdr_public_root ${hdr_public_root} include/libavstream/httputil.hpp)
	set(src_private_root ${src_private_root} src/httputil.cpp)
endif()

set(hdr_public ${hdr_public_root} ${hdr_public_network} ${src_public_stream} ${src_public_decoders} ${src_public_encoders} ${src_public_surfaces} ${src_public_platforms} ${hdr_public_geometry} ${hdr_public_audio})
source_group("Public Headers" src FILES ${hdr_public_root} )
source_group("Public Headers\\stream" src FILES ${src_public_stream} )
source_group("Public Headers\\decoders" src FILES ${src_public_decoders} )
source_group("Public Headers\\encoders" src FILES ${src_public_encoders} )
source_group("Public Headers\\surfaces" src FILES ${src_public_surfaces} )
source_group("Public Headers\\platforms" src FILES ${src_public_platforms} )
source_group("Public Headers\\geometry" src FILES ${hdr_public_geometry} )
source_group("Public Headers\\audio" src FILES ${hdr_public_audio} )
source_group("Public Headers\\network" src FILES ${hdr_public_network} )

set(src_private ${src_private_root} ${src_private_api} ${src_private_network} ${src_private_stream} ${src_private_util} ${src_private_audio})

# Windows platform
if(WIN32)
	set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Ob0 /Od /D_DEBUG /DDEBUG /RTC1")
	set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Od /Ob0 /DNDEBUG /Zi")
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
	set(src_platform
		src/platforms/platform_posix.cpp
	)
	set(def_platform PLATFORM_LINUX)
	find_program(xxd NAMES xxd)
else()
message("Unsupported platform for libavstream!")
return()
endif() 

# CUDA kernels
add_custom_command(
	OUTPUT enc_nvidia.cubin enc_nvidia.cubin.inl
	COMMAND ${CMAKE_CUDA_COMPILER} -allow-unsupported-compiler -fatbin ${CMAKE_CURRENT_SOURCE_DIR}/src/encoders/enc_nvidia.cu -o enc_nvidia.cubin
	COMMAND ${xxd} -i enc_nvidia.cubin enc_nvidia.cubin.inl
	DEPENDS src/encoders/enc_nvidia.cu
)

add_custom_command(
	OUTPUT dec_nvidia.cubin dec_nvidia.cubin.inl
	COMMAND ${CMAKE_CUDA_COMPILER} -allow-unsupported-compiler -fatbin ${CMAKE_CURRENT_SOURCE_DIR}/src/decoders/dec_nvidia.cu -o dec_nvidia.cubin
	COMMAND ${xxd} -i dec_nvidia.cubin dec_nvidia.cubin.inl
	DEPENDS src/decoders/dec_nvidia.cu
)
add_custom_target(
	cuda_kernels
	DEPENDS enc_nvidia.cubin enc_nvidia.cubin.inl dec_nvidia.cubin dec_nvidia.cubin.inl SOURCES src/encoders/enc_nvidia.cu src/decoders/dec_nvidia.cu
)
set_target_properties(cuda_kernels PROPERTIES FOLDER AVStream)

set(CMAKE_CXX_STANDARD 17 )
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_library(libavstream STATIC ${hdr_private} ${src_private} ${hdr_public} ${src_platform})

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	target_compile_options(libavstream PRIVATE -fPIC)
endif()
#set_target_runtime(libavstream static)
set_target_properties(libavstream PROPERTIES PREFIX "")
set_target_properties(libavstream PROPERTIES FOLDER AVStream)

# WebRTC is VERY SENSITIVE to these settings. Having the wrong settings can cause bizarre runtime memory-trashing.
##set(def_webrtc   
##						  _WEBSOCKETPP_CPP11_INTERNAL_
##                           #USE_AURA=1
##							_HAS_EXCEPTIONS=0 __STD_C _CRT_RAND_S
##							_CRT_SECURE_NO_DEPRECATE
##                           _SCL_SECURE_NO_DEPRECATE
##							_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
##							_ATL_NO_OPENGL _WINDOWS CERT_CHAIN_PARA_HAS_EXTRA_FIELDS
##                           PSAPI_VERSION=2 _SECURE_ATL  __WRL_NO_DEFAULT_LIB__
##                           WINAPI_FAMILY=WINAPI_FAMILY_PC_APP WIN10=_WIN32_WINNT_WIN10 WIN32_LEAN_AND_MEAN
##                           NOMINMAX NTDDI_VERSION=NTDDI_WIN10_RS2 _WIN32_WINNT=0x0A00 WINVER=0x0A00
##                           NDEBUG NVALGRIND DYNAMIC_ANNOTATIONS_ENABLED=0
##							WEBRTC_ENABLE_PROTOBUF=0
##                           WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE RTC_ENABLE_VP9 HAVE_SCTP
##                           WEBRTC_LIBRARY_IMPL WEBRTC_NON_STATIC_TRACE_EVENT_HANDLERS=0
##                           WEBRTC_WIN
##							ABSL_ALLOCATOR_NOTHROW=1
##							ABSL_USES_STD_OPTIONAL=0
##							HAVE_SCTP WEBRTC_VIDEO_CAPTURE_WINRT
##
set(def_webrtc			  WEBRTC_WIN _CRT_SECURE_NO_DEPRECATE _SCL_SECURE_NO_DEPRECATE _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
target_compile_definitions(libavstream PRIVATE ${def_platform} )
target_compile_definitions(libavstream PRIVATE ${def_webrtc})
set(webrtc_include_paths ${CMAKE_SOURCE_DIR}/thirdparty/webrtc/include
	${CMAKE_SOURCE_DIR}/thirdparty/webrtc/include/third_party/libyuv/include
	${CMAKE_SOURCE_DIR}/thirdparty/webrtc/include/third_party/abseil-cpp)
if(LIBAV_USE_SRT)
	set(srt_include_paths "${SRT_SOURCE_DIR}/common" "${SRT_SOURCE_DIR}/srtcore" ${SRT_BINARY_DIR})
	#message(WARNING srt_include_paths ${srt_include_paths})
endif()
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

if(NOT TELEPORT_SERVER)
    set(BUILD_CURL_EXE OFF CACHE BOOL "")
    set(CURL_STATIC_CRT ON CACHE BOOL "")
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
	set(BUILD_TESTING OFF CACHE BOOL "")
	# SCHANNEL is for native windows SSL.
	set(CURL_USE_SCHANNEL ON CACHE BOOL "")
    add_subdirectory(thirdparty/curl)
	set_target_properties(libcurl PROPERTIES FOLDER AVStream/thirdparty/curl)
	target_compile_definitions(libcurl PRIVATE BUILDING_LIBCURL=1) 
	set(curl_include_paths "${CURL_SOURCE_DIR}/include" ${CURL_BINARY_DIR})
	set(curl_libraries libcurl)
	target_compile_definitions(libavstream PUBLIC TELEPORT_CLIENT=1) 
endif()
target_include_directories(libavstream PRIVATE src PRIVATE ${CMAKE_SOURCE_DIR}
														${CMAKE_CURRENT_BINARY_DIR}
														${webrtc_include_paths}
														${srt_include_paths}
														${efp_include_paths}
														${curl_include_paths}
														include
														${PTHREAD_INCLUDE_DIR}
														PUBLIC include/libavstream)
target_compile_features(libavstream PRIVATE cxx_std_17)

if(LIBAAV_BUILD_SHARED_LIBS)
	target_compile_definitions(libavstream
		PUBLIC LIBAVSTREAM_SHARED
		PRIVATE LIBAVSTREAM_EXPORTS
	)
endif()
if(LIBAV_USE_SRT)
	target_compile_definitions(libavstream PUBLIC LIBAV_USE_SRT=1)
else()
	target_compile_definitions(libavstream PUBLIC LIBAV_USE_SRT=0)
endif()
target_compile_definitions(libavstream PRIVATE LIBAV_USE_CUDA=$<BOOL:${LIBAV_USE_CUDA}>)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	target_compile_definitions(libavstream PRIVATE PLATFORM_64BIT)
endif()

# Third party dependencies
add_subdirectory(thirdparty/asio)
add_subdirectory(thirdparty/nv)

# Tests
if(LIBAV_BUILD_TESTS)
	add_subdirectory(thirdparty/cxxopts)
	add_subdirectory(tests)
endif()

#Include CUDA library location.
target_link_directories(libavstream PUBLIC "${LIBAV_CUDA_DIR}/lib/x64")
target_link_directories(libavstream PUBLIC "${LIBAV_CUDA_DIR}/lib64")

target_link_libraries(libavstream cuda cudart asio nv efp ${curl_libraries})

if(LIBAV_USE_SRT)
target_link_libraries(libavstream srt_static)
endif()
# Build CUDA kernels on PC platforms only
if(NOT ANDROID)
	add_dependencies(libavstream cuda_kernels)
	target_include_directories(libavstream PRIVATE "${LIBAV_CUDA_SAMPLES_DIR}/common/inc")
	target_include_directories(libavstream PRIVATE "${LIBAV_CUDA_DIR}/include")
endif()

# linking with libdatachannel for alternative webrtc support
target_compile_definitions(libavstream PRIVATE TELEPORT_DATACHANNEL_WEBRTC=$<BOOL:${TELEPORT_DATACHANNEL_WEBRTC}>)
if(TELEPORT_DATACHANNEL_WEBRTC)
	target_include_directories(libavstream PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/libdatachannel/include")
	target_link_directories(libavstream PUBLIC "${CMAKE_BINARY_DIR}/thirdparty/libdatachannel/${CONFIG_NAME}")
	target_link_libraries(libavstream datachannel-static )
	if(TELEPORT_WINDOWS)
		target_link_libraries(libavstream legacy_stdio_definitions.lib)
	endif()
endif()
target_include_directories(libavstream PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/libdatachannel/deps/json/single_include")