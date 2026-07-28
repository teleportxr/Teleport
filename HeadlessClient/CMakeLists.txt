cmake_minimum_required(VERSION 3.8)
project(teleport_terminal)

set(srcs
	Main.cpp
	HeadlessClient.h HeadlessClient.cpp
	HeadlessSessionCommandInterface.h HeadlessSessionCommandInterface.cpp
	HeadlessGeometryCacheBackend.h HeadlessGeometryCacheBackend.cpp
	HeadlessGeometryTarget.h HeadlessGeometryTarget.cpp
	HeadlessInputState.h HeadlessInputState.cpp
	ReplCommandParser.h ReplCommandParser.cpp
	Repl.h Repl.cpp
)

if(WIN32)
	set(def_platform PLATFORM_WINDOWS UNICODE _WIN32_WINNT=0x0601)
	add_static_executable(teleport_terminal CONSOLE SOURCES ${srcs})
elseif(TELEPORT_UNIX)
	set(def_platform PLATFORM_LINUX)
	add_static_executable(teleport_terminal CONSOLE SOURCES ${srcs})
else()
	message(FATAL_ERROR "Unsupported platform for teleport_terminal")
endif()

target_include_directories(teleport_terminal PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../libavstream/include
	${CMAKE_CURRENT_SOURCE_DIR}/../libavstream/export_include
)

target_link_directories(teleport_terminal PRIVATE
	${CMAKE_BINARY_DIR}/lib
)

target_compile_features(teleport_terminal PRIVATE cxx_std_17)

# Core libraries: TeleportClient, TeleportCore, libavstream
# Also need Core_MT and platform libs for FileLoader and utilities
# Deliberately NOT linked: ClientRender, ktx, OpenXR loader, GLFW, D3D libs, ImGui
target_link_libraries(teleport_terminal
	libavstream
	TeleportClient
	TeleportCore
	Core_MT
	PlatformVulkan_MT
	PlatformCrossPlatform_MT
	PlatformMath_MT
	LibDataChannel::LibDataChannelStatic
	usrsctp
	juice-static
)

if(WIN32)
	target_link_directories(teleport_terminal PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/openssl/x64/lib)
	target_link_libraries(teleport_terminal
		ws2_32.lib
		libssl.lib
		libcrypto.lib
		crypt32.lib
		iphlpapi.lib
		bcrypt.lib
	)
elseif(TELEPORT_UNIX)
	target_link_libraries(teleport_terminal ssl crypto pthread dl)
endif()

target_compile_definitions(teleport_terminal PRIVATE ${def_platform})

SetTeleportDefaults(teleport_terminal)
set_target_properties(teleport_terminal PROPERTIES FOLDER Client)

if(TELEPORT_UNIX)
	install(TARGETS teleport_terminal RUNTIME DESTINATION bin COMPONENT client)
elseif(TELEPORT_WINDOWS)
	install(TARGETS teleport_terminal RUNTIME DESTINATION build/bin/Release COMPONENT client)
endif()
