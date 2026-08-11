cmake_minimum_required(VERSION 3.8)
project(TeleportPCClient)

option(TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER "Use Platform video decoder? Currently requires TELEPORT_CLIENT_USE_D3D12" OFF)

if(${TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER})
	if(NOT ${TELEPORT_CLIENT_USE_D3D12})
		message("Warning: Setting TELEPORT_CLIENT_USE_D3D12 true because TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER is true.")
		set(TELEPORT_CLIENT_USE_D3D12 ON)
	endif()
endif()

#configure_file(client_default.ini assets/client.ini) 
file(GLOB inifiles "../client/client_default.ini" assets/client.ini)
set(srcs
	Main.cpp
	MemoryUtil.cpp
	MemoryUtil.h
	ProcessHandler.h
	ProcessHandler.cpp
	UseOpenXR.cpp
	UseOpenXR.h
	UnixDebugOutput.h
	Resource.h
	targetver.h
	../ClientApp/ClientApp.cpp
	../ClientApp/ClientApp.h
	CPackOptions.cmake
) 
set(shaders "${CMAKE_SOURCE_DIR}/client/Shaders/canvas_text.sfx"
			"${CMAKE_SOURCE_DIR}/client/Shaders/cubemap_clear.sfx"
			"${CMAKE_SOURCE_DIR}/client/Shaders/texture_conversion.sfx")
file(GLOB shader_includes "${CMAKE_SOURCE_DIR}/client/Shaders/*.sl" )

source_group("Shaders" src FILES ${shaders} )
source_group("Shader Includes" src FILES ${shader_includes} )

if(WIN32)
	set(def_platform PLATFORM_WINDOWS UNICODE _WIN32_WINNT=0x0601)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /DPLATFORM_DIR=${TELEPORT_PLATFORM_DIR}")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /DPLATFORM_DIR=${TELEPORT_PLATFORM_DIR}")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /wd4711")
	add_static_executable( TeleportPCClient WIN32 SOURCES ${srcs} ${shaders} ${shader_includes} ${inifiles} Icon.rc)
elseif(PLATFORM_LINUX)
	set(def_platform PLATFORM_LINUX)
	add_static_executable( TeleportPCClient CONSOLE SOURCES ${srcs} ${shaders} ${shader_includes} ${inifiles})
elseif(TELEPORT_MACOS)
	set(def_platform PLATFORM_MACOS)
	# A real .app bundle, not a bare CONSOLE binary (unlike teleport_terminal): the icon and
	# Info.plist below only take effect on Finder/Dock/Launchpad for a proper bundle, and
	# MACOSX_BUNDLE (added to add_static_executable in firstparty/Platform/CMake/Include.cmake)
	# makes CMake stage one in the build tree too, so the icon works pre-install as well as
	# post-install/post-DMG-drag.
	set(TELEPORT_PCCLIENT_ICNS "${CMAKE_SOURCE_DIR}/Installers/TeleportPCClient.icns")
	set_source_files_properties(${TELEPORT_PCCLIENT_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	add_static_executable( TeleportPCClient MACOSX_BUNDLE SOURCES ${srcs} ${shaders} ${shader_includes} ${inifiles} ${TELEPORT_PCCLIENT_ICNS})
	set_target_properties(TeleportPCClient PROPERTIES
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
		MACOSX_BUNDLE_ICON_FILE "TeleportPCClient.icns"
		MACOSX_BUNDLE_GUI_IDENTIFIER "co.simul.teleportxr.pcclient"
		MACOSX_BUNDLE_BUNDLE_NAME "TeleportPCClient"
	)
else()
	message("Unsupported platform for TeleportPCClient!")
	return()
endif()

if(LIBAV_USE_CUDA)
	add_custom_command(
        TARGET TeleportPCClient POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${LIBAV_CUDA_DIR}/bin/cudart64_110.dll
               ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/cudart64_110.dll)
endif()
if(TELEPORT_CLIENT_SUPPORT_IPSME)
	add_custom_command(
        TARGET TeleportPCClient POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${TELEPORT_CLIENT_MOSQUITTO_LIB_DIR}/${CMAKE_CFG_INTDIR}/mosquitto.dll
               ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/mosquitto.dll)
endif()

add_multiplatform_sfx_shader_project(TeleportPCClient INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../client/Shaders" "${SIMUL_PLATFORM_DIR}/CrossPlatform/Shaders" "${CMAKE_SOURCE_DIR}/.." "${SIMUL_PLATFORM_DIR}/.." SOURCES ${shaders} ${shader_includes} ${global_shader_includes} OPTIONS OUTPUT "${CMAKE_BINARY_DIR}/shaderbin/$PLATFORM_NAME" DEFINES FOLDER Shaders )

target_include_directories(TeleportPCClient PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR} ${TELEPORT_SIMUL}/.. ../libavstream/include ../libavstream/export_include ../TeleportAudio/src ../ClientRender/src)
if(WIN32)
	target_include_directories(TeleportPCClient PRIVATE ${TELEPORT_PLATFORM_DIR}/Windows)
endif()
 

if(TELEPORT_CLIENT_USE_D3D12)
	target_link_libraries(TeleportPCClient SimulDirectX12_MT)
	target_link_libraries(TeleportPCClient d3d12)
endif()
if(TELEPORT_CLIENT_USE_D3D11)
message("TELEPORT_CLIENT_USE_D3D11")
	target_link_libraries(TeleportPCClient SimulDirectX11_MT)
endif()
if(TELEPORT_CLIENT_USE_VULKAN)
	target_link_libraries(TeleportPCClient PlatformVulkan_MT)
    if (Vulkan_FOUND)
        target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})
        target_compile_definitions(${PROJECT_NAME} PUBLIC XR_TUTORIAL_USE_VULKAN)
    endif()
endif()
#target_link_directories(TeleportPCClient PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/webrtc/lib/Release)
target_link_libraries(TeleportPCClient )
target_compile_definitions(TeleportPCClient PUBLIC TELEPORT_CLIENT_USE_VULKAN=$<BOOL:${TELEPORT_CLIENT_USE_VULKAN}> TELEPORT_CLIENT_USE_D3D12=$<BOOL:${TELEPORT_CLIENT_USE_D3D12}> TELEPORT_CLIENT_USE_D3D11=$<BOOL:${TELEPORT_CLIENT_USE_D3D11}> TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER=$<BOOL:${TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER}>  )	

SetTeleportDefaults(TeleportPCClient)
set_target_properties( TeleportPCClient PROPERTIES FOLDER Client)
if(WIN32)
	set_target_properties( TeleportPCClient PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:windows")
	set_target_properties( TeleportPCClient PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
	set_target_properties( TeleportPCClient PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=$ENV{$WindowsSdkVerBinPath};$(PATH)")
endif()
target_include_directories(TeleportPCClient PUBLIC ${SIMUL_PLATFORM_DIR}/External/imgui)
if(NOT TELEPORT_MACOS)
	# thirdparty/openxr-sdk only vendors prebuilt Windows .lib import libraries; macOS instead
	# fetches and builds the openxr_loader target from source (see the TELEPORT_MACOS branch
	# below), which carries its own generated openxr/openxr.h via its usual CMake usage
	# requirements, so it must not also pick up these vendored headers.
	target_include_directories(TeleportPCClient PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/openxr-sdk/include)
	target_link_directories(TeleportPCClient PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/openxr-sdk/lib)
endif()
target_compile_features(TeleportPCClient PRIVATE cxx_std_17)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	target_compile_definitions(TeleportPCClient PRIVATE PLATFORM_64BIT)
endif()

# Common libraries
target_link_libraries(TeleportPCClient libavstream TeleportAudio ClientRender TeleportClient TeleportCore ktx)
target_link_libraries(TeleportPCClient Core_MT PlatformCrossPlatform_MT PlatformMath_MT ImGui_MT)
target_link_libraries(TeleportPCClient LibDataChannel::LibDataChannelStatic usrsctp juice-static)

if(WIN32)
	# Windows-specific libraries
	target_link_libraries(TeleportPCClient debug openxr_loaderd optimized openxr_loader)
	target_link_libraries(TeleportPCClient winmm directxtex_MT)
	target_link_directories(TeleportPCClient PUBLIC ../thirdparty/openssl/x64/lib)
	# libssl depends on libcrypto (the WPACKET_* symbols live there); link both from the
	# bundled static OpenSSL so they resolve against a matched build. See the OpenSSL block
	# in the root CMakeLists.txt.
	target_link_libraries(TeleportPCClient ws2_32.lib libssl.lib libcrypto.lib crypt32.lib iphlpapi.lib bcrypt.lib)
	target_link_libraries(TeleportPCClient -ignore:4711)
	target_link_libraries(TeleportPCClient Shlwapi.lib)
	target_link_options(TeleportPCClient PUBLIC /ignore:4711)
	target_compile_options(TeleportPCClient PRIVATE /wd4711)
elseif(PLATFORM_LINUX)
	# Linux-specific libraries
	# Find OpenXR - try system installation first
	find_package(OpenXR QUIET)
	if(OpenXR_FOUND)
		target_link_libraries(TeleportPCClient OpenXR::openxr_loader)
	else()
		# Fall back to pkg-config
		find_package(PkgConfig QUIET)
		if(PKG_CONFIG_FOUND)
			pkg_check_modules(OPENXR openxr)
			if(OPENXR_FOUND)
				target_include_directories(TeleportPCClient PRIVATE ${OPENXR_INCLUDE_DIRS})
				target_link_libraries(TeleportPCClient ${OPENXR_LIBRARIES})
			else()
				message(WARNING "OpenXR not found. Please install libopenxr-dev or set OpenXR_DIR.")
			endif()
		else()
			message(WARNING "OpenXR not found. Please install libopenxr-dev.")
		endif()
	endif()
	target_link_libraries(TeleportPCClient ssl crypto)
	target_link_libraries(TeleportPCClient pthread dl)
	target_include_directories(TeleportPCClient PRIVATE ${SIMUL_PLATFORM_DIR}/External/glfw/include)
	target_link_libraries(TeleportPCClient glfw)
elseif(TELEPORT_MACOS)
	# macOS-specific libraries.
	# No system or vendored OpenXR loader exists for macOS: there's no Homebrew formula (no
	# macOS OpenXR runtime ecosystem exists to package a loader for), and the vendored
	# thirdparty/openxr-sdk/lib only ships prebuilt Windows .lib files. Build the loader from
	# source instead - it's portable C/C++ that builds and links fine with no runtime present,
	# which is all that's needed since ENABLE_VR defaults off on macOS (TeleportClient/Config.h)
	# and UseOpenXR never actually calls xrCreateInstance there.
	include(FetchContent)
	set(BUILD_LOADER ON CACHE BOOL "" FORCE)
	set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)
	set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
	set(DYNAMIC_LOADER ON CACHE BOOL "" FORCE)
	FetchContent_Declare(
		openxr_sdk_macos
		GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK-Source.git
		GIT_TAG release-1.1.42
	)
	FetchContent_MakeAvailable(openxr_sdk_macos)
	target_link_libraries(TeleportPCClient openxr_loader)
	# ssl/crypto are not linked bare here (unlike the Linux branch above) - TeleportClient
	# already links OpenSSL::SSL publicly (TeleportClient/CMakeLists.txt), which resolves to
	# Homebrew's keg-only openssl@3 .dylib by full path and propagates transitively. Bare
	# -lssl/-lcrypto would fail to link on macOS since Homebrew doesn't put openssl@3 on the
	# linker's default search path.
	target_link_libraries(TeleportPCClient pthread dl)
	target_include_directories(TeleportPCClient PRIVATE ${SIMUL_PLATFORM_DIR}/External/glfw/include)
	target_link_libraries(TeleportPCClient glfw)

	if(TELEPORT_CLIENT_USE_VULKAN)
		# Main.cpp calls CFBundleGetMainBundle()/CFBundleCopyResourcesDirectoryURL() (SetupMoltenVkIcd)
		# to locate the bundled MoltenVK ICD manifest before the first Vulkan call.
		target_link_libraries(TeleportPCClient "-framework CoreFoundation")
		# MoltenVK is never linked - the Vulkan loader dlopens it at runtime via the ICD manifest
		# bundled below (Installers/MoltenVK_icd.json, staged into Contents/Resources/vulkan/icd.d
		# by the install() rules further down). It's only needed here to find the .dylib to bundle.
		if(NOT MOLTENVK_LIBRARY)
			if(EXISTS /opt/homebrew/opt/molten-vk/lib/libMoltenVK.dylib)
				set(MOLTENVK_LIBRARY /opt/homebrew/opt/molten-vk/lib/libMoltenVK.dylib CACHE FILEPATH "Path to libMoltenVK.dylib, bundled as the Vulkan ICD")
			elseif(EXISTS /usr/local/opt/molten-vk/lib/libMoltenVK.dylib)
				set(MOLTENVK_LIBRARY /usr/local/opt/molten-vk/lib/libMoltenVK.dylib CACHE FILEPATH "Path to libMoltenVK.dylib, bundled as the Vulkan ICD")
			else()
				message(WARNING "libMoltenVK.dylib not found (expected under Homebrew's molten-vk keg). "
					"TeleportPCClient.app will not bundle a Vulkan ICD and will only run on a Mac with "
					"MoltenVK installed system-wide. Install with 'brew install molten-vk' or set MOLTENVK_LIBRARY.")
			endif()
		endif()
	endif()
endif()

if(TELEPORT_CLIENT_SUPPORT_IPSME)
	target_link_directories(TeleportPCClient PUBLIC "${TELEPORT_CLIENT_MOSQUITTO_LIB_DIR}")
	if(WIN32)
		target_link_libraries(TeleportPCClient mosquitto.lib)
	else()
		target_link_libraries(TeleportPCClient mosquitto)
	endif()
endif()

file(GLOB global_shader_includes "${SIMUL_PLATFORM_DIR}/CrossPlatform/Shaders/*.sl")

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
add_custom_command(
    TARGET TeleportPCClient
    PRE_BUILD
    COMMAND (robocopy ${CMAKE_SOURCE_DIR}/client/assets ${CMAKE_CURRENT_SOURCE_DIR}/assets /s /e /NFL /NDL /NJH /NJS /nc /ns /np) ^& IF %ERRORLEVEL% LSS 8 SET ERRORLEVEL = 0
)
endif()

# CPack configuration (generators, NSIS strings, Debian fields, include(CPack)) lives in
# CMake/TeleportPackaging.cmake, included from the root CMakeLists.txt after all
# subdirectories. Only this target's install() rules belong here.
# TELEPORT_CLIENT_DATA_DEST is the destination for the client's runtime data, relative to
# the install prefix. It must be set before the first install() rule that uses it: an unset
# value expands to nothing, turning "${TELEPORT_CLIENT_DATA_DEST}/assets/textures" into the
# absolute /assets/textures, which lands at the root of the target filesystem.
# TELEPORT_CLIENT_COMPONENT is "client" everywhere except macOS, where TeleportPCClient is
# packaged as its own "pcclient" CPack component (a DragNDrop .dmg, CMake/TeleportPackaging.cmake)
# separate from teleport_terminal's "client" component (a productbuild .pkg,
# HeadlessClient/CMakeLists.txt) - the two need different generators, so they can't share a
# component.
set(TELEPORT_CLIENT_COMPONENT client)
if(TELEPORT_WINDOWS)
	set(TELEPORT_CLIENT_DATA_DEST "pc_client")
	install(TARGETS TeleportPCClient RUNTIME DESTINATION build/bin/Release COMPONENT ${TELEPORT_CLIENT_COMPONENT})
elseif(TELEPORT_MACOS)
	set(TELEPORT_CLIENT_COMPONENT pcclient)
	# Everything below lands inside the bundle, so the .app is a single self-contained,
	# relocatable, drag-to-Applications unit:
	#   TeleportPCClient.app/Contents/MacOS/TeleportPCClient       (BUNDLE DESTINATION . below)
	#   TeleportPCClient.app/Contents/Resources/TeleportPCClient.icns  (MACOSX_PACKAGE_LOCATION, above)
	#   TeleportPCClient.app/Contents/Frameworks/*.dylib           (fixup_bundle, below)
	#   TeleportPCClient.app/Contents/Resources/vulkan/icd.d/       (MoltenVK, below)
	#   TeleportPCClient.app/Contents/share/teleportxr/...          (data dir - see below)
	#
	# "share/teleportxr" (not Resources) so teleport::client::ResolveClientDataDirectory finds it
	# unmodified: it walks up from the executable's directory looking for "share/teleportxr" or
	# "client" below each ancestor (TeleportClient/ClientBootstrap.cpp), and
	# Contents/MacOS/../share/teleportxr = Contents/share/teleportxr satisfies that walk exactly
	# as the Linux layout's bin/../share/teleportxr does, with no code changes needed.
	set(TELEPORT_CLIENT_DATA_DEST "TeleportPCClient.app/Contents/share/teleportxr")
	install(TARGETS TeleportPCClient BUNDLE DESTINATION . COMPONENT ${TELEPORT_CLIENT_COMPONENT})
	# Copies every non-system dylib TeleportPCClient links (Homebrew's Vulkan loader and OpenSSL,
	# plus this project's own FetchContent-built libktx/libdraco/libozz_*/libopus/libglfw/
	# libopenxr_loader/libcurl, all currently found via absolute paths or @rpath) into
	# Contents/Frameworks, and rewrites TeleportPCClient's load commands to match - so the .dmg
	# produces an app that runs on a Mac with none of this project's build dependencies installed.
	# BundleUtilities is a stock CMake module (not a new third-party dependency); fixup_bundle is
	# CMake's documented legacy-but-supported tool for exactly this. It only follows link-time
	# dependencies, so it never touches MoltenVK (dlopen'd via the ICD manifest, not linked) -
	# that's bundled separately below.
	install(CODE "
		set(BU_CHMOD_BUNDLE_ITEMS ON)
		include(BundleUtilities)
		# fixup_bundle resolves each @rpath/libX.dylib dependency by searching these directories
		# for a real file - it can't derive them from the binary's own (relative, @loader_path-
		# based) LC_RPATH, since nothing has been copied into the bundle yet on this first pass.
		# This project's own FetchContent-built dylibs (libktx/libdraco/libozz_*/libopus/libglfw/
		# libopenxr_loader/libcurl) land in a different build-tree directory per upstream project
		# (bin/, Release/, thirdparty/draco/, _deps/<name>-build/...), so search the whole build
		# tree for *.dylib rather than hardcode each one - new dependencies then need no change here.
		file(GLOB_RECURSE _teleport_dylibs \"${CMAKE_BINARY_DIR}/*.dylib\")
		set(_teleport_dylib_dirs \"\")
		foreach(_dylib \${_teleport_dylibs})
			get_filename_component(_dylib_dir \"\${_dylib}\" DIRECTORY)
			list(APPEND _teleport_dylib_dirs \"\${_dylib_dir}\")
		endforeach()
		list(REMOVE_DUPLICATES _teleport_dylib_dirs)
		fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/TeleportPCClient.app\" \"\" \"\${_teleport_dylib_dirs};/opt/homebrew/lib;/usr/local/lib\")
	" COMPONENT ${TELEPORT_CLIENT_COMPONENT})
	if(MOLTENVK_LIBRARY)
		# Kept alongside its own manifest, not in Contents/Frameworks with everything else: the
		# Vulkan loader resolves an ICD's "library_path" relative to the .json's own directory
		# (see Installers/MoltenVK_icd.json - "./libMoltenVK.dylib"), and VK_ICD_FILENAMES (set at
		# startup in pc_client/Main.cpp) points straight at this manifest.
		install(FILES "${MOLTENVK_LIBRARY}"
			DESTINATION TeleportPCClient.app/Contents/Resources/vulkan/icd.d COMPONENT ${TELEPORT_CLIENT_COMPONENT})
		install(FILES "${CMAKE_SOURCE_DIR}/Installers/MoltenVK_icd.json"
			DESTINATION TeleportPCClient.app/Contents/Resources/vulkan/icd.d COMPONENT ${TELEPORT_CLIENT_COMPONENT})
	endif()
	# Re-sign every embedded dylib plus MoltenVK now (fixup_bundle's install_name_tool rewriting
	# above invalidates any signature the linker applied). The bundle *as a whole* is NOT signed
	# here - it's signed once at the very end of this file, after every install() rule that adds
	# files under TeleportPCClient.app has run (textures/fonts/localGeometryCache/shaders/
	# client.ini, added later via the shared TELEPORT_CLIENT_DATA_DEST rules below). Signing the
	# bundle here would sign a manifest of what Contents/ contains *at this point*; every file
	# install()ed afterwards would then be unsealed, and `codesign --verify --deep --strict`
	# fails with "a sealed resource is missing or invalid" - caught by an actual CI run.
	# Mirrors HeadlessClient/CMakeLists.txt's TELEPORT_MACOS_APP_IDENTITY re-sign block. Only
	# runs when CI (or a locally-configured Developer ID) supplies an identity; local dev builds
	# stay unsigned, matching the .pkg flow.
	set(TELEPORT_MACOS_APP_IDENTITY "" CACHE STRING "Developer ID Application identity used to re-sign staged binaries after install-time rewriting")
	if(TELEPORT_MACOS_APP_IDENTITY)
		install(CODE "
			set(_bundle \"\${CMAKE_INSTALL_PREFIX}/TeleportPCClient.app\")
			file(GLOB _to_sign \"\${_bundle}/Contents/Frameworks/*\" \"\${_bundle}/Contents/Resources/vulkan/icd.d/*.dylib\")
			foreach(_bin \${_to_sign})
				if(NOT IS_DIRECTORY \${_bin})
					execute_process(COMMAND codesign --force --options runtime --timestamp
						--sign \"${TELEPORT_MACOS_APP_IDENTITY}\" \"\${_bin}\"
						RESULT_VARIABLE _rc
					)
					if(NOT _rc EQUAL 0)
						message(FATAL_ERROR \"codesign failed on \${_bin}\")
					endif()
				endif()
			endforeach()
		" COMPONENT ${TELEPORT_CLIENT_COMPONENT})
	endif()
else()
	# "share/<package>" is the FHS layout for read-only data inside /opt/<package>, and is the
	# name teleport::client::ResolveClientDataDirectory looks for first. Lower case to match the
	# /usr/share/teleportxr a distribution package would use, so that only the prefix would differ.
	set(TELEPORT_CLIENT_DATA_DEST "share/teleportxr")
	# Linux .deb layout (CPACK_PACKAGING_INSTALL_PREFIX=/opt/teleportxr):
	#   /opt/teleportxr/bin/TeleportPCClient
	#   /opt/teleportxr/share/teleportxr/...     (data directory, found by walking up from bin/)
	# Icon and desktop entry use absolute paths so they land under /usr/share regardless
	# of the package prefix, where the freedesktop tooling expects them.
	install(TARGETS TeleportPCClient RUNTIME DESTINATION bin COMPONENT ${TELEPORT_CLIENT_COMPONENT})
	install(FILES "${CMAKE_SOURCE_DIR}/client/textures/teleportxr.png"
		DESTINATION /usr/share/icons/hicolor/256x256/apps
		RENAME TeleportXR.png COMPONENT ${TELEPORT_CLIENT_COMPONENT})
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/TeleportXR.desktop"
		DESTINATION /usr/share/applications COMPONENT ${TELEPORT_CLIENT_COMPONENT})
	# Runtime window icon: Main.cpp loads this relative to the client working directory.
	install(FILES "${CMAKE_SOURCE_DIR}/client/textures/teleportxr.png"
		DESTINATION ${TELEPORT_CLIENT_DATA_DEST}/assets/textures COMPONENT ${TELEPORT_CLIENT_COMPONENT})
	# Convenience symlink so the binary can be invoked from a shell.
	install(CODE "
		file(MAKE_DIRECTORY \"\$ENV{DESTDIR}/usr/bin\")
		file(CREATE_LINK \"/opt/teleportxr/bin/TeleportPCClient\"
			\"\$ENV{DESTDIR}/usr/bin/teleportxr\" SYMBOLIC)
	" COMPONENT ${TELEPORT_CLIENT_COMPONENT})
endif()
file(GLOB INSTALL_TEXTURES "${CMAKE_CURRENT_SOURCE_DIR}/textures/*.*")
file(GLOB INSTALL_FONTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/*.*")
if(TELEPORT_WINDOWS AND LIBAV_USE_CUDA)
	install(FILES "${LIBAV_CUDA_DIR}/bin/cudart64_110.dll" DESTINATION build/bin/Release COMPONENT ${TELEPORT_CLIENT_COMPONENT})
endif()
install(FILES ${INSTALL_TEXTURES} DESTINATION ${TELEPORT_CLIENT_DATA_DEST}/textures COMPONENT ${TELEPORT_CLIENT_COMPONENT})
install(FILES ${INSTALL_FONTS} DESTINATION ${TELEPORT_CLIENT_DATA_DEST}/assets/fonts COMPONENT ${TELEPORT_CLIENT_COMPONENT})
install(DIRECTORY "${CMAKE_SOURCE_DIR}/client/assets/localGeometryCache" DESTINATION ${TELEPORT_CLIENT_DATA_DEST}/assets COMPONENT ${TELEPORT_CLIENT_COMPONENT})
install(FILES "${CMAKE_SOURCE_DIR}/client/client_default.ini" DESTINATION ${TELEPORT_CLIENT_DATA_DEST} COMPONENT ${TELEPORT_CLIENT_COMPONENT})
install(FILES "${CMAKE_SOURCE_DIR}/client/client_default.ini" DESTINATION ${TELEPORT_CLIENT_DATA_DEST}/assets RENAME client.ini COMPONENT ${TELEPORT_CLIENT_COMPONENT} OPTIONAL)
install(DIRECTORY "${CMAKE_BINARY_DIR}/shaderbin/" DESTINATION ${TELEPORT_CLIENT_DATA_DEST}/assets/shaders COMPONENT ${TELEPORT_CLIENT_COMPONENT})

install(FILES ${shaders} DESTINATION ${TELEPORT_CLIENT_DATA_DEST}/Shaders COMPONENT ${TELEPORT_CLIENT_COMPONENT})

if(TELEPORT_MACOS AND TELEPORT_MACOS_APP_IDENTITY)
	# Signs TeleportPCClient.app as a whole, sealing everything under Contents/ into its code
	# resource manifest. Deliberately the last install() rule in this file (declaration order is
	# execution order for a directory's cmake_install.cmake): every rule above that adds a file
	# under the bundle - fixup_bundle/MoltenVK earlier in this file, and the shared
	# textures/fonts/localGeometryCache/shaders/client.ini rules just above, which run for every
	# platform - must already have run, or codesign seals a manifest that's missing whatever
	# hasn't been installed yet, and `codesign --verify --deep --strict` on the result fails with
	# "a sealed resource is missing or invalid" (caught by an actual CI run - see the git history
	# of this block).
	install(CODE "
		execute_process(COMMAND codesign --force --options runtime --timestamp
			--sign \"${TELEPORT_MACOS_APP_IDENTITY}\" \"\${CMAKE_INSTALL_PREFIX}/TeleportPCClient.app\"
			RESULT_VARIABLE _rc
		)
		if(NOT _rc EQUAL 0)
			message(FATAL_ERROR \"codesign failed on \${CMAKE_INSTALL_PREFIX}/TeleportPCClient.app\")
		endif()
	" COMPONENT ${TELEPORT_CLIENT_COMPONENT})
endif()