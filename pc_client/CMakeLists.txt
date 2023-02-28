cmake_minimum_required(VERSION 3.8)
project(pc_client)

option(TELEPORT_CLIENT_USE_D3D12 "Use D3D12?" OFF)
if( ${TELEPORT_CLIENT_USE_D3D12})
	set(TELEPORT_CLIENT_USE_D3D11 OFF)
else()
	set(TELEPORT_CLIENT_USE_D3D11 ON)
endif()
option(TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER "Use Platform video decoder? Currently requires TELEPORT_CLIENT_USE_D3D12" OFF)

if(${TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER})
	if(NOT ${TELEPORT_CLIENT_USE_D3D12})
		message("Warning: Setting TELEPORT_CLIENT_USE_D3D12 true because TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER is true.")
		set(TELEPORT_CLIENT_USE_D3D12 ON)
	endif()
endif()

file(GLOB inifiles "*.ini")
set(srcs
	Main.cpp
	MemoryUtil.cpp
	MemoryUtil.h
	UseOpenXR.cpp
	UseOpenXR.h
	Resource.h
	targetver.h
	../ClientApp/ClientApp.cpp
	../ClientApp/ClientApp.h
)

if(WIN32)
	set(def_platform PLATFORM_WINDOWS UNICODE _WIN32_WINNT=0x0601)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /DPLATFORM_DIR=${TELEPORT_PLATFORM_DIR}")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /DPLATFORM_DIR=${TELEPORT_PLATFORM_DIR}")
else()
	message("Unsupported platform for pc_client!")
	return()
endif()

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /wd4711")
source_group("Shaders" src FILES ${shaders} )

add_static_executable( pc_client WIN32 SOURCES ${srcs} ${shaders} ${inifiles} Icon.rc)


target_include_directories(pc_client PRIVATE 
	${CMAKE_CURRENT_SOURCE_DIR} ${TELEPORT_SIMUL}/.. ${CMAKE_SOURCE_DIR}/../thirdparty/enet/include ../libavstream/include ../TeleportAudio/src ../ClientRender/src ../thirdparty/basis_universal ${TELEPORT_PLATFORM_DIR}/Windows)

if(TELEPORT_CLIENT_USE_D3D12)
	target_link_libraries(pc_client SimulDirectX12_MT)
else()
	target_link_libraries(pc_client SimulDirectX11_MT)
	target_link_libraries(pc_client d3d12)
endif()
target_compile_definitions(pc_client PUBLIC TELEPORT_CLIENT_USE_D3D12=$<BOOL:${TELEPORT_CLIENT_USE_D3D12}> TELEPORT_CLIENT_USE_D3D11=$<BOOL:${TELEPORT_CLIENT_USE_D3D11}> TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER=$<BOOL:${TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER}>) 	

SetTeleportDefaults(pc_client)
set_target_properties( pc_client PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:windows")
set_target_properties( pc_client PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties( pc_client PROPERTIES FOLDER Client)
set_target_properties( pc_client PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=C:/Program Files (x86)/Windows Kits/10/bin/10.0.19041.0/x86;$(PATH)"	 )		
target_include_directories(pc_client PUBLIC ../thirdparty/enet/Include ${CMAKE_SOURCE_DIR}/thirdparty/openxr-sdk/include ${SIMUL_PLATFORM_DIR}/External/fmt/include ${SIMUL_PLATFORM_DIR}/External/imgui)
target_compile_features(pc_client PRIVATE cxx_std_17)
target_link_directories(pc_client PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/openxr-sdk/lib)
#target_link_direc tories(pc_client PUBLIC "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.19041.0/um/x64")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	target_compile_definitions(pc_client PRIVATE PLATFORM_64BIT)
endif()

target_link_libraries(pc_client debug openxr_loaderd optimized openxr_loader) 
target_link_libraries(pc_client libavstream enet TeleportAudio ClientRender TeleportClient TeleportCore basisu winmm)
target_link_libraries(pc_client Core_MT SimulCrossPlatform_MT SimulMath_MT directxtex_MT ImGui_MT fmt)
target_link_libraries(pc_client -ignore:4711)

target_link_options(pc_client PUBLIC /ignore:4711)
target_compile_options(pc_client PRIVATE /wd4711)

set(CPACK_PACKAGE_INSTALL_DIRECTORY "cpack_install")

LIST(APPEND CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$INSTDIR\\\\TeleportClient.lnk' '$INSTDIR\\\\bin\\\\Release\\\\pc_client..exe' '-log'"
)
LIST(APPEND CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$INSTDIR\\\\TeleportClient.lnk'"
)
install(TARGETS pc_client
		RUNTIME
		DESTINATION build_pc_client/bin/Release
		COMPONENT client)
file(GLOB INSTALL_TEXTURES "${CMAKE_CURRENT_SOURCE_DIR}/textures/*.*")
file(GLOB INSTALL_FONTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/*.*")
install(FILES "${CMAKE_BINARY_DIR}\\\\bin\\\\Release\\\\pc_client.exe" DESTINATION build_pc_client/bin/Release COMPONENT client )
install(FILES "${CMAKE_BINARY_DIR}\\\\bin\\\\Release\\\\pc_client.pdb" DESTINATION build_pc_client/bin/Release COMPONENT client )
install(FILES ${INSTALL_TEXTURES} DESTINATION pc_client/textures COMPONENT client )
install(FILES ${INSTALL_FONTS} DESTINATION pc_client/assets/fonts COMPONENT client )
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}\\\\assets\\\\localGeometryCache" DESTINATION pc_client/assets COMPONENT client)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}\\\\client_default.ini" DESTINATION pc_client/assets RENAME client.ini COMPONENT client OPTIONAL)
install(DIRECTORY "${CMAKE_BINARY_DIR}\\\\firstparty\\\\Platform\\\\DirectX11\\\\shaderbin\\\\" DESTINATION pc_client/assets/shaders/directx11 COMPONENT client)
install(DIRECTORY "${CMAKE_BINARY_DIR}\\\\firstparty\\\\Platform\\\\DirectX12\\\\shaderbin\\\\" DESTINATION pc_client/assets/shaders/directx12 COMPONENT client)
install(DIRECTORY "${CMAKE_BINARY_DIR}\\\\firstparty\\\\Platform\\\\Vulkan\\\\shaderbin\\\\" DESTINATION pc_client/assets/shaders/vulkan COMPONENT client)

install(DIRECTORY "${CMAKE_BINARY_DIR}\\\\firstparty\\\\Platform\\\\DirectX11\\\\shaderbin\\\\" DESTINATION pc_client/assets/shaders/directx11 COMPONENT client)

message("FILES ${shaders} DESTINATION pc_client\\\\Shaders COMPONENT client")


install(FILES ${shaders} DESTINATION pc_client\\\\Shaders COMPONENT client)


# Create an installer for the client:
#set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
#include(InstallRequiredSystemLibraries)
#install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
#        DESTINATION programs
#        COMPONENT applications)
set(CPACK_PACKAGE_NAME "TeleportClientInstaller")
set(CPACK_PACKAGE_VENDOR "Simul")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Installer for the TeleportVR PC CLient")

# get commit id from a file:
file(TOUCH "teleport_commit.properties")
file(READ "teleport_commit.properties" ver)
if(NOT( "${ver}" STREQUAL ""))
	message(${ver})
	string(REGEX MATCH "TELEPORT_COMMIT= ([a-zA-Z0-9]+)" _ ${ver})
endif()
set(TELEPORT_COMMIT ${CMAKE_MATCH_1})
set(CPACK_PACKAGE_VERSION_MAJOR "${TELEPORT_COMMIT}")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
message("TELEPORT_COMMIT ${TELEPORT_COMMIT}")
if("${TELEPORT_COMMIT}" STREQUAL "")
	message("Test installer build.")
	message("CPACK_PACKAGING_INSTALL_PREFIX ${CPACK_PACKAGING_INSTALL_PREFIX}")
	#C:/TestInstallTeleport)
endif()
set(CPACK_PACKAGE_INSTALL_DIRECTORY "TeleportVR")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${TELEPORT_COMMIT}-x64")
message("CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_FILE_NAME}")
file(WRITE "build_pc_client/installer_filename.properties" "TELEPORT_CLIENT_INSTALLER_FILENAME=${CPACK_PACKAGE_FILE_NAME}.exe")
#
set(CPACK_GENERATOR "NSIS")
set(CPACK_COMPONENTS_ALL client )

#set(CPACK_COMPONENT_CTDATA_GROUP "data")
#set(CPACK_COMPONENT_INSTRUCTIONDATA_GROUP "data")
#
#set(CPACK_COMPONENT_APPLICATIONS_DISPLAY_NAME "applications (hello & world)")
#set(CPACK_COMPONENT_GROUP_DATA_DESCRIPTION "data (CT and instructions)")
#
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}\\\\Installers\\\\TeleportClientLicence.md")
##Create an icon which will be displayed inside the installation wizard, and an icon for the installer and uninstaller. Tell CPack about these icons.
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}\\\\Installers\\\\TeleportIcon.ico")
set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}\\\\Installers\\\\TeleportIcon.ico")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}\\\\Installers\\\\TeleportIcon.ico")
##Set the programs, directories and files, displayed under the Start menu shortcut.
set(CPACK_NSIS_MENU_LINKS "build_pc_client\\\\bin\\\\Release\\\\pc_client" "Teleport VR Client")

include(CPack)