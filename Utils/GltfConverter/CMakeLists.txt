cmake_minimum_required(VERSION 3.16)
project(GltfConverter)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Main executable
add_executable(GltfConverter
	GltfConverter.cpp
)

# TeleportCore publicly exposes tiny_gltf.h (thirdparty/draco/third_party/tinygltf) and
# <nlohmann/json.hpp>, which is all this tool needs - no server/graphics dependencies required.
target_link_libraries(GltfConverter
	TeleportCore
)

# Installation
install(TARGETS GltfConverter
	RUNTIME DESTINATION bin
)
