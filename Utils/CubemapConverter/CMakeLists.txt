cmake_minimum_required(VERSION 3.16)
project(CubemapConverter)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Vulkan REQUIRED)

# Get TeleportServer library location
get_target_property(TELEPORT_SERVER_INCLUDE_DIR TeleportServer INTERFACE_INCLUDE_DIRECTORIES)

# Main executable
add_executable(CubemapConverter
	CubemapConverter.cpp
)

# Link against TeleportServer library and its dependencies
target_link_libraries(CubemapConverter
	TeleportServer
	TeleportCore
	ktx
	Vulkan::Vulkan
)

# Include directories
target_include_directories(CubemapConverter PRIVATE
	${CMAKE_SOURCE_DIR}
	${TELEPORT_SERVER_INCLUDE_DIR}
)

# Installation
install(TARGETS CubemapConverter
	RUNTIME DESTINATION bin
)
