cmake_minimum_required(VERSION 3.8)
project(TeleportPCClient)

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

file(GLOB inifiles "*.ini" "assets/*.ini")
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
	CPackOptions.cmake
)
file(GLOB shaders "../client/Shaders/*.sfx" )
file(GLOB shader_includes "../client/Shaders/*.sl" )

if(WIN32)
	set(def_platform PLATFORM_WINDOWS UNICODE _WIN32_WINNT=0x0601)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /DPLATFORM_DIR=${TELEPORT_PLATFORM_DIR}")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /DPLATFORM_DIR=${TELEPORT_PLATFORM_DIR}")
else()
	message("Unsupported platform for TeleportPCClient!")
	return()
endif()

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /wd4711")
source_group("Shaders" src FILES ${shaders} )

add_static_executable( TeleportPCClient WIN32 SOURCES ${srcs} ${shaders} ${inifiles} Icon.rc)

add_multiplatform_sfx_shader_project(TeleportPCClient INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../client/Shaders" "${SIMUL_PLATFORM_DIR}/CrossPlatform/Shaders" "${CMAKE_SOURCE_DIR}/.." "${SIMUL_PLATFORM_DIR}/.." SOURCES ${shaders} ${shader_includes} ${global_shader_includes} OPTIONS  OUTPUT "${CMAKE_BINARY_DIR}/shaderbin/$PLATFORM_NAME" DEFINES FOLDER Shaders )

target_include_directories(TeleportPCClient PRIVATE 
	${CMAKE_CURRENT_SOURCE_DIR} ${TELEPORT_SIMUL}/.. ${CMAKE_SOURCE_DIR}/../thirdparty/enet/include ../libavstream/include ../TeleportAudio/src ../ClientRender/src ../thirdparty/basis_universal ${TELEPORT_PLATFORM_DIR}/Windows)
 

if(TELEPORT_CLIENT_USE_D3D12)
	target_link_libraries(TeleportPCClient SimulDirectX12_MT)
	target_link_libraries(TeleportPCClient d3d12)
else()
	target_link_libraries(TeleportPCClient SimulDirectX11_MT)
endif()
#target_link_directories(TeleportPCClient PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/webrtc/lib/Release)
target_link_libraries(TeleportPCClient )
target_compile_definitions(TeleportPCClient PUBLIC TELEPORT_CLIENT_USE_D3D12=$<BOOL:${TELEPORT_CLIENT_USE_D3D12}> TELEPORT_CLIENT_USE_D3D11=$<BOOL:${TELEPORT_CLIENT_USE_D3D11}> TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER=$<BOOL:${TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER}>) 	

SetTeleportDefaults(TeleportPCClient)
set_target_properties( TeleportPCClient PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:windows")
set_target_properties( TeleportPCClient PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties( TeleportPCClient PROPERTIES FOLDER Client)
set_target_properties( TeleportPCClient PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=$ENV{$WindowsSdkVerBinPath};$(PATH)"	 )		
target_include_directories(TeleportPCClient PUBLIC ../thirdparty/enet/Include ${CMAKE_SOURCE_DIR}/thirdparty/openxr-sdk/include ${SIMUL_PLATFORM_DIR}/External/fmt/include ${SIMUL_PLATFORM_DIR}/External/imgui)
target_compile_features(TeleportPCClient PRIVATE cxx_std_17)
target_link_directories(TeleportPCClient PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/openxr-sdk/lib)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	target_compile_definitions(TeleportPCClient PRIVATE PLATFORM_64BIT)
endif()

target_link_libraries(TeleportPCClient debug openxr_loaderd optimized openxr_loader) 
target_link_libraries(TeleportPCClient libavstream enet TeleportAudio ClientRender TeleportClient TeleportCore basisu winmm)
target_link_libraries(TeleportPCClient Core_MT SimulCrossPlatform_MT SimulMath_MT directxtex_MT ImGui_MT fmt)

target_link_directories(TeleportPCClient PUBLIC ../thirdparty/openssl/x64/lib)
target_link_libraries(TeleportPCClient LibDataChannel::LibDataChannelStatic usrsctp ws2_32.lib  libssl.lib juice-static iphlpapi.lib  bcrypt.lib)


target_link_libraries(TeleportPCClient -ignore:4711)

target_link_options(TeleportPCClient PUBLIC /ignore:4711)
target_compile_options(TeleportPCClient PRIVATE /wd4711)

cmake_minimum_required(VERSION 2.9)

file(GLOB global_shader_includes "${SIMUL_PLATFORM_DIR}/CrossPlatform/Shaders/*.sl")


set(CPACK_PACKAGE_INSTALL_DIRECTORY "cpack_install")

LIST(APPEND CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$INSTDIR\\\\TeleportClient.lnk' '$INSTDIR\\\\bin\\\\Release\\\\TeleportPCClient.exe' '-log'"
)
LIST(APPEND CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$INSTDIR\\\\TeleportClient.lnk'"
)
install(TARGETS TeleportPCClient
		RUNTIME
		DESTINATION build_pc_client/bin/Release
		COMPONENT client)
file(GLOB INSTALL_TEXTURES "${CMAKE_CURRENT_SOURCE_DIR}/textures/*.*")
file(GLOB INSTALL_FONTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/*.*")
install(FILES "${CMAKE_BINARY_DIR}\\\\bin\\\\Release\\\\TeleportPCClient.exe" DESTINATION build_pc_client/bin/Release COMPONENT client )
install(FILES "${CMAKE_BINARY_DIR}\\\\bin\\\\Release\\\\TeleportPCClient.pdb" DESTINATION build_pc_client/bin/Release COMPONENT client )
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

set(CPACK_PACKAGE_INSTALL_DIRECTORY "TeleportVR")
#
set(CPACK_GENERATOR "NSIS")
set (CPACK_PROJECT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/pc_client/CPackOptions.cmake")
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
set(CPACK_NSIS_MENU_LINKS "build_pc_client\\\\bin\\\\Release\\\\TeleportPCClient" "Teleport VR Client")

include(CPack)