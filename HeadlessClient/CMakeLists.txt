cmake_minimum_required(VERSION 3.8)
project(teleportd)

Include(FetchContent)
FetchContent_Declare(
	  cli
	  GIT_REPOSITORY https://github.com/daniele77/cli.git
	  GIT_SHALLOW 1
	  GIT_TAG v2.2.0
	  SOURCE_DIR cli
	)
FetchContent_MakeAvailable(cli)

# The headless client is split into two processes:
#   teleportd - the service: owns live connections, ticks them at 20 Hz,
#                       and takes one-line commands over a loopback TCP socket.
#   teleport_cli      - a small POSIX-style front end that forwards commands from
#                       a terminal, pipe, or -e argument to the service.
# Everything they share lives in the teleport_headless_core static library.

set(core_srcs
	HeadlessConnection.h HeadlessConnection.cpp
	ConnectionManager.h ConnectionManager.cpp
	CommandProcessor.h CommandProcessor.cpp
	ControlServer.h ControlServer.cpp
	ControlProtocol.h
	SocketUtil.h SocketUtil.cpp
	HeadlessSessionCommandInterface.h HeadlessSessionCommandInterface.cpp
	HeadlessGeometryCacheBackend.h HeadlessGeometryCacheBackend.cpp
	HeadlessGeometryTarget.h HeadlessGeometryTarget.cpp
	HeadlessGeometryDecoder.h HeadlessGeometryDecoder.cpp
	HeadlessInputState.h HeadlessInputState.cpp
	ReplCommandParser.h ReplCommandParser.cpp
)

if(WIN32)
	set(def_platform PLATFORM_WINDOWS UNICODE _WIN32_WINNT=0x0601)
elseif(TELEPORT_MACOS)
	set(def_platform PLATFORM_MACOS)
elseif(TELEPORT_UNIX)
	set(def_platform PLATFORM_LINUX)
else()
	message(FATAL_ERROR "Unsupported platform for teleportd")
endif()

# --- Shared core -------------------------------------------------------------

add_library(teleport_headless_core STATIC ${core_srcs})

target_include_directories(teleport_headless_core PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../libavstream/include
	${CMAKE_CURRENT_SOURCE_DIR}/../libavstream/export_include
)

target_link_directories(teleport_headless_core PUBLIC
	${CMAKE_BINARY_DIR}/lib
)

target_compile_features(teleport_headless_core PUBLIC cxx_std_17)
target_compile_definitions(teleport_headless_core PUBLIC ${def_platform})

# Deliberately NOT linked: ClientRender, ktx, OpenXR loader, GLFW, D3D libs, ImGui
target_link_libraries(teleport_headless_core PUBLIC
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

set_target_properties(teleport_headless_core PROPERTIES FOLDER Client)

# --- Service: teleportd ----------------------------------------------

add_static_executable(teleportd CONSOLE SOURCES Main.cpp)
target_link_libraries(teleportd teleport_headless_core)
target_compile_features(teleportd PRIVATE cxx_std_17)

# Historically the Platform submodule only added its Vulkan subdirectory for Windows and
# Linux, so PlatformVulkan_MT never existed on macOS and this link was a no-op there. macOS
# now builds it too (MoltenVK, see the TELEPORT_GUI_CLIENT comment in the root CMakeLists.txt),
# and firstparty/Platform/Vulkan/CMakeLists.txt links PlatformVulkan_MT against GLFW - so
# linking it here on macOS would drag libglfw.dylib into teleportd, defeating the
# "no GPU/graphics dependency" guarantee the headless client is built to (see the "Confirm no
# unwanted dependencies" check in .github/workflows/build_macos_client.yml). The headless
# client draws nothing and its sole Platform include is Platform/Core/FileLoader.h, so this
# is a link-order convenience on Windows/Linux, not a real dependency - safe to skip on macOS.
if(TARGET PlatformVulkan_MT AND NOT TELEPORT_MACOS)
	target_link_libraries(teleportd PlatformVulkan_MT)
endif()

if(WIN32)
	target_link_directories(teleportd PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/openssl/x64/lib)
	target_link_libraries(teleportd
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
		target_link_directories(teleportd PRIVATE ${OPENSSL_ROOT_DIR}/lib)
	endif()
	target_link_libraries(teleportd ssl crypto pthread)
elseif(TELEPORT_UNIX)
	target_link_libraries(teleportd ssl crypto pthread dl)
endif()

SetTeleportDefaults(teleportd)
set_target_properties(teleportd PROPERTIES FOLDER Client)

# --- CLI: teleport_cli --------------------------------------------------------
# Intentionally tiny: no libavstream, no WebRTC, no OpenSSL. Just the command-line
# front end, the socket shim, and (on Windows) winsock.

add_static_executable(teleport_cli CONSOLE SOURCES cli_main.cpp SocketUtil.h SocketUtil.cpp ControlProtocol.h)
# ${CMAKE_CURRENT_SOURCE_DIR}/.. resolves "TeleportCore/StringFunctions.h" (GetEnvVar,
# header-only) without linking TeleportCore.
target_include_directories(teleport_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(teleport_cli PRIVATE cxx_std_17)
target_compile_definitions(teleport_cli PRIVATE ${def_platform})
# Interactive line editing, persistent history and tab completion. Header-only;
# the local terminal session needs no asio, so teleport_cli stays free of
# libavstream/WebRTC/OpenSSL.
target_link_libraries(teleport_cli cli::cli)
if(WIN32)
	target_link_libraries(teleport_cli ws2_32.lib)
endif()

SetTeleportDefaults(teleport_cli)
set_target_properties(teleport_cli PROPERTIES FOLDER Client)

# --- Install ------------------------------------------------------------------

if(TELEPORT_UNIX)
	install(TARGETS teleportd teleport_cli RUNTIME DESTINATION bin COMPONENT client)
elseif(TELEPORT_WINDOWS)
	install(TARGETS teleportd teleport_cli RUNTIME DESTINATION bin COMPONENT client)
endif()

if(TELEPORT_MACOS)
	# Unlike libktx/libcurl/libopus/libdraco/libozz_*, which are FetchContent-built as
	# real targets in this project and install(TARGETS ...) themselves into <prefix>/lib,
	# Homebrew's libssl/libcrypto are prebuilt dylibs found by find_package(OpenSSL) —
	# nothing else copies them into the package. Bundle them by hand, same destination.
	# Homebrew dylibs carry an absolute install_name (e.g.
	# /opt/homebrew/opt/openssl@3/lib/libssl.3.dylib) rather than an @rpath-relative one,
	# so copying the files above is not enough on its own: teleportd, and
	# libssl.3.dylib's own reference to libcrypto, still point at the Homebrew path baked
	# in at link time. On a Mac without that exact keg installed this fails at launch with
	# "Library not loaded: .../openssl@3/lib/libssl.3.dylib". Rewrite the load commands to
	# point at the copies bundled next to the binary instead. (teleport_cli does not link
	# OpenSSL and needs no rewriting.)
	#
	# Staging path below: "\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}" evaluated at install
	# time. CPack leaves DESTDIR empty and overrides CMAKE_INSTALL_PREFIX with the
	# staging prefix; a manual install keeps the configured prefix and honours DESTDIR.
	# $ENV{DESTDIR} alone resolves to bare /opt/teleportxr under CPack, which made
	# install_name_tool fail with "can't open file" and silently left the Homebrew
	# paths in the package.
	if(OPENSSL_ROOT_DIR)
		# libssl.3.dylib and libcrypto.3.dylib both self-identify (LC_ID_DYLIB) via the
		# opt/ symlink OPENSSL_ROOT_DIR points at, which is what a linker embeds in any
		# binary *we* link against them (teleportd, below) - so matching on
		# "${OPENSSL_ROOT_DIR}/lib/libcrypto.3.dylib" works for teleportd. But Homebrew's
		# own build of libssl.3.dylib recorded *its* dependency on libcrypto using the
		# resolved Cellar path (e.g. .../Cellar/openssl@3/3.6.3/lib/libcrypto.3.dylib),
		# not the opt/ symlink form - install_name_tool -change only rewrites an exact
		# match, so assuming the symlink form here silently does nothing and ships a
		# libssl.3.dylib that still points at a Homebrew Cellar path. Ask the actual
		# dylib what it references instead of assuming its form.
		execute_process(
			COMMAND otool -L ${OPENSSL_ROOT_DIR}/lib/libssl.3.dylib
			OUTPUT_VARIABLE _libssl_deps
		)
		string(REGEX MATCH "\n[ \t]+([^ \t\n]*libcrypto\\.3\\.dylib)" _libssl_libcrypto_match "${_libssl_deps}")
		if(NOT CMAKE_MATCH_1)
			message(FATAL_ERROR "Could not find libssl.3.dylib's libcrypto dependency in: ${_libssl_deps}")
		endif()
		set(_libssl_libcrypto_dep "${CMAKE_MATCH_1}")

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
				\"\${_stage}/bin/teleportd\"
				RESULT_VARIABLE _rc
			)
			if(NOT _rc EQUAL 0)
				message(FATAL_ERROR \"install_name_tool failed on \${_stage}/bin/teleportd\")
			endif()
			execute_process(COMMAND install_name_tool
				-id \"@loader_path/libssl.3.dylib\"
				-change \"${_libssl_libcrypto_dep}\" \"@loader_path/libcrypto.3.dylib\"
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
	# copy inside the .pkg would fail Gatekeeper. This covers not just teleportd
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
