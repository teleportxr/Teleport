cmake_minimum_required(VERSION 3.8)
project(teleport_terminal)

set(srcs
	Main.cpp
	HeadlessClient.h HeadlessClient.cpp
	HeadlessSessionCommandInterface.h HeadlessSessionCommandInterface.cpp
	HeadlessGeometryCacheBackend.h HeadlessGeometryCacheBackend.cpp
	HeadlessGeometryTarget.h HeadlessGeometryTarget.cpp
	HeadlessGeometryDecoder.h HeadlessGeometryDecoder.cpp
	HeadlessInputState.h HeadlessInputState.cpp
	ReplCommandParser.h ReplCommandParser.cpp
	Repl.h Repl.cpp
)

if(WIN32)
	set(def_platform PLATFORM_WINDOWS UNICODE _WIN32_WINNT=0x0601)
	add_static_executable(teleport_terminal CONSOLE SOURCES ${srcs})
elseif(TELEPORT_MACOS)
	set(def_platform PLATFORM_MACOS)
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
	PlatformCrossPlatform_MT
	PlatformMath_MT
	LibDataChannel::LibDataChannelStatic
	usrsctp
	juice-static
)

# The Platform submodule only adds its Vulkan subdirectory for Windows and Linux, so the
# target does not exist on macOS. The headless client draws nothing and its sole Platform
# include is Platform/Core/FileLoader.h, so this is a link-order convenience, not a
# real dependency.
if(TARGET PlatformVulkan_MT)
	target_link_libraries(teleport_terminal PlatformVulkan_MT)
endif()

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
elseif(TELEPORT_MACOS)
	# No -ldl on macOS: dlopen and friends live in libSystem.
	# Homebrew's openssl@3 is keg-only, so its lib dir is not on the default linker
	# search path; OPENSSL_ROOT_DIR is passed on the cmake command line.
	if(OPENSSL_ROOT_DIR)
		target_link_directories(teleport_terminal PRIVATE ${OPENSSL_ROOT_DIR}/lib)
	endif()
	target_link_libraries(teleport_terminal ssl crypto pthread)
elseif(TELEPORT_UNIX)
	target_link_libraries(teleport_terminal ssl crypto pthread dl)
endif()

target_compile_definitions(teleport_terminal PRIVATE ${def_platform})

SetTeleportDefaults(teleport_terminal)
set_target_properties(teleport_terminal PROPERTIES FOLDER Client)

if(TELEPORT_UNIX)
	install(TARGETS teleport_terminal RUNTIME DESTINATION bin COMPONENT client)
elseif(TELEPORT_WINDOWS)
	install(TARGETS teleport_terminal RUNTIME DESTINATION bin COMPONENT client)
endif()

if(TELEPORT_MACOS)
	# Unlike libktx/libcurl/libopus/libdraco/libozz_*, which are FetchContent-built as
	# real targets in this project and install(TARGETS ...) themselves into <prefix>/lib,
	# Homebrew's libssl/libcrypto are prebuilt dylibs found by find_package(OpenSSL) —
	# nothing else copies them into the package. Bundle them by hand, same destination.
	# Homebrew dylibs carry an absolute install_name (e.g.
	# /opt/homebrew/opt/openssl@3/lib/libssl.3.dylib) rather than an @rpath-relative one,
	# so copying the files above is not enough on its own: teleport_terminal, and
	# libssl.3.dylib's own reference to libcrypto, still point at the Homebrew path baked
	# in at link time. On a Mac without that exact keg installed this fails at launch with
	# "Library not loaded: .../openssl@3/lib/libssl.3.dylib". Rewrite the load commands to
	# point at the copies bundled next to the binary instead.
	#
	# Staging path below: "\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}" evaluated at install
	# time. CPack leaves DESTDIR empty and overrides CMAKE_INSTALL_PREFIX with the
	# staging prefix; a manual install keeps the configured prefix and honours DESTDIR.
	# $ENV{DESTDIR} alone resolves to bare /opt/teleportxr under CPack, which made
	# install_name_tool fail with "can't open file" and silently left the Homebrew
	# paths in the package.
	if(OPENSSL_ROOT_DIR)
		install(FILES
			${OPENSSL_ROOT_DIR}/lib/libssl.3.dylib
			${OPENSSL_ROOT_DIR}/lib/libcrypto.3.dylib
			DESTINATION lib COMPONENT client
		)
		install(CODE "
			set(_stage \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\")
			execute_process(COMMAND install_name_tool
				-change \"${OPENSSL_ROOT_DIR}/lib/libssl.3.dylib\" \"@loader_path/../lib/libssl.3.dylib\"
				-change \"${OPENSSL_ROOT_DIR}/lib/libcrypto.3.dylib\" \"@loader_path/../lib/libcrypto.3.dylib\"
				\"\${_stage}/bin/teleport_terminal\"
				RESULT_VARIABLE _rc
			)
			if(NOT _rc EQUAL 0)
				message(FATAL_ERROR \"install_name_tool failed on \${_stage}/bin/teleport_terminal\")
			endif()
			execute_process(COMMAND install_name_tool
				-id \"@loader_path/libssl.3.dylib\"
				-change \"${OPENSSL_ROOT_DIR}/lib/libcrypto.3.dylib\" \"@loader_path/libcrypto.3.dylib\"
				\"\${_stage}/lib/libssl.3.dylib\"
				RESULT_VARIABLE _rc
			)
			if(NOT _rc EQUAL 0)
				message(FATAL_ERROR \"install_name_tool failed on \${_stage}/lib/libssl.3.dylib\")
			endif()
			execute_process(COMMAND install_name_tool
				-id \"@loader_path/libcrypto.3.dylib\"
				\"\${_stage}/lib/libcrypto.3.dylib\"
				RESULT_VARIABLE _rc
			)
			if(NOT _rc EQUAL 0)
				message(FATAL_ERROR \"install_name_tool failed on \${_stage}/lib/libcrypto.3.dylib\")
			endif()
		" COMPONENT client)
	endif()

	# Re-sign every staged binary after the rewriting above: CMake's install-time RPATH
	# fixup and install_name_tool both invalidate any signature applied to the
	# build-tree binary beforehand, so signing the build output is not enough - the
	# copy inside the .pkg would fail Gatekeeper. This covers not just teleport_terminal
	# but every bundled third-party tool and dylib in bin/ and lib/ (draco_decoder etc.,
	# libssl/libcrypto/libcurl/libdraco/libopus) - notarisation rejects the whole .pkg if
	# any single Mach-O inside it is unsigned, lacks the hardened runtime, or lacks a
	# secure timestamp, and those third-party binaries only ever carry the ad-hoc
	# signature the linker applies on arm64, not a Developer ID one. Only when CI passes
	# an identity; local builds stay unsigned. Declared after install(TARGETS) and the
	# install_name_tool block so it runs last in this directory's install script.
	set(TELEPORT_MACOS_APP_IDENTITY "" CACHE STRING "Developer ID Application identity used to re-sign staged binaries after install-time rewriting")
	if(TELEPORT_MACOS_APP_IDENTITY)
		install(CODE "
			set(_stage \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\")
			file(GLOB _to_sign \"\${_stage}/bin/*\" \"\${_stage}/lib/*.dylib\")
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
		" COMPONENT client)
	endif()
endif()
