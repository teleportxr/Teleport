# TeleportPackaging.cmake — all CPack configuration for the client installers.
#
# Included from the root CMakeLists.txt *after* every add_subdirectory(), because
# include(CPack) must see the final value of every CPACK_* variable, and CMake
# variables set in a subdirectory do not propagate back up to the parent scope.
# That is why this file lives here rather than in pc_client/, where it used to sit:
# macOS builds the headless client without pc_client at all, and would otherwise
# have no packaging.
#
# The individual install() rules stay with their own targets (pc_client/CMakeLists.txt,
# HeadlessClient/CMakeLists.txt) — install() is recorded globally, so only the
# CPACK_* variables need to be hoisted.
#
# Generators: NSIS on Windows, DEB on Linux, productbuild on macOS.

# Architecture suffix for the package filename. Named CPACK_* deliberately: CPack copies
# every CPACK_* variable into CPackConfig.cmake, which is the only way CPackOptions.cmake
# (CPACK_PROJECT_CONFIG_FILE, evaluated later in a fresh script context) can see it.
if(TELEPORT_MACOS)
	set(CPACK_TELEPORT_PACKAGE_ARCH "arm64")
else()
	set(CPACK_TELEPORT_PACKAGE_ARCH "x64")
endif()

set(CPACK_PACKAGE_NAME "TeleportClientInstaller")
set(CPACK_PACKAGE_VENDOR "Simul")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Installer for the TeleportXR PC CLient")

set(CPACK_PACKAGE_INSTALL_DIRECTORY "TeleportXR")

set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/pc_client/CPackOptions.cmake")
set(CPACK_COMPONENTS_ALL client)

if(TELEPORT_WINDOWS)
	set(CPACK_GENERATOR "NSIS")

	set(TELEPORT_INSTALLED_CLIENT_EXE "$INSTDIR\\\\build\\\\bin\\\\Release\\\\TeleportPCClient.exe")
	LIST(APPEND CPACK_NSIS_CREATE_ICONS_EXTRA
	    "CreateShortCut '$INSTDIR\\\\TeleportClient.lnk' '${TELEPORT_INSTALLED_CLIENT_EXE}' '-log'"
	)
	LIST(APPEND CPACK_NSIS_DELETE_ICONS_EXTRA
	    "Delete '$INSTDIR\\\\TeleportClient.lnk'"
	)
	SET(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
  WriteRegStr HKCR 'Teleport' '' 'URL:Teleport Protocol'
  WriteRegStr HKCR 'Teleport' 'URL Protocol' ''
  WriteRegStr HKCR 'Teleport\\\\DefaultIcon' '' '${TELEPORT_INSTALLED_CLIENT_EXE}'
  WriteRegStr HKCR 'Teleport\\\\shell\\\\open\\\\command' '' '\\\"${TELEPORT_INSTALLED_CLIENT_EXE}\\\" \\\"%1\\\" \\\"%2\\\" \\\"%3\\\"'")
	SET(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
  DeleteRegKey HKCR 'Teleport'
 ")

	# NSIS MUI chooses the license page parser based on the file extension;
	# a plain-text copy is kept alongside the markdown source so the installer
	# and the standalone NSIS script both reference a file NSIS can parse reliably.
	set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/Installers/TeleportClientLicence.txt")
	# CPACK_PACKAGE_ICON maps to NSIS MUI_HEADERIMAGE_BITMAP, which must be a BMP
	# (makensis aborts on a non-bitmap here). Use the same header bitmap as the
	# standalone Installers/TeleportClient.nsi. The .ico is only for MUI_ICON below.
	#
	# This value feeds NSIS's File command, which needs Windows backslashes; forward
	# slashes give "no files found". CPack neither converts the separators nor escapes
	# the value when writing CPackConfig.cmake, so emit doubled backslashes: the raw
	# config round-trip collapses them to single backslashes in the generated project.nsi
	# (same convention as CPACK_NSIS_EXTRA_INSTALL_COMMANDS above).
	string(REPLACE "/" "\\\\" CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/Installers/TeleportLogo150x57.bmp")
	set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/Installers/TeleportIcon.ico")
	set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/Installers/TeleportIcon.ico")
	set(CPACK_NSIS_MENU_LINKS "build\\\\bin\\\\Release\\\\TeleportPCClient.exe" "Teleport VR Client")

elseif(TELEPORT_MACOS)
	set(CPACK_GENERATOR "productbuild")
	# Same staging prefix as the Linux .deb.
	set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/teleportxr")
	set(CPACK_PACKAGE_NAME "teleportxr")
	# Reverse-DNS package identifier. Gatekeeper ties the notarisation ticket to it,
	# so it must stay stable across releases.
	set(CPACK_PRODUCTBUILD_IDENTIFIER "co.simul.teleportxr")
	set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/Installers/TeleportClientLicence.txt")
	# Set by CI to the "Developer ID Installer: ..." identity so cpack emits an
	# already-signed .pkg; left empty for local builds, which produce an unsigned one.
	set(TELEPORT_MACOS_INSTALLER_IDENTITY "" CACHE STRING "Developer ID Installer identity used to sign the .pkg")
	if(TELEPORT_MACOS_INSTALLER_IDENTITY)
		set(CPACK_PRODUCTBUILD_IDENTITY_NAME "${TELEPORT_MACOS_INSTALLER_IDENTITY}")
		set(CPACK_PKGBUILD_IDENTITY_NAME "${TELEPORT_MACOS_INSTALLER_IDENTITY}")
	endif()

else()
	set(CPACK_GENERATOR "DEB")
	# Stage payload under /opt/teleportxr. Absolute install() destinations
	# (e.g. /usr/share/applications) escape this prefix and are honoured literally.
	set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/teleportxr")
	set(CPACK_PACKAGE_NAME "teleportxr")
	set(CPACK_DEBIAN_PACKAGE_NAME "teleportxr")
	set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Teleport XR <contact@teleportxr.io>")
	set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
	set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
	set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
	set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/teleportxr/Teleport")
	set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
		"TeleportXR spatial streaming client\n TeleportXR streams XR scenes from a remote server to a local OpenXR\n headset or desktop window using the Teleport protocol.")
	# Let dpkg-shlibdeps fill in shared-library dependencies automatically.
	set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
	# dpkg-shlibdeps runs from the package staging root; point it at the bundled
	# private libs so it doesn't error on libktx, libglfw, libdraco, etc.
	string(REGEX REPLACE "^/" "" _rel_prefix "${CPACK_PACKAGING_INSTALL_PREFIX}")
	set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS "${_rel_prefix}/lib")
endif()

# Convenience symlinks so the installed binaries can be invoked from a shell.
# macOS must use /usr/local/bin: /usr/bin is protected by System Integrity Protection
# and a package that writes there fails to install.
if(TELEPORT_MACOS)
	if(TARGET teleport_terminal)
		install(CODE "
			file(MAKE_DIRECTORY \"\$ENV{DESTDIR}/usr/local/bin\")
			file(CREATE_LINK \"/opt/teleportxr/bin/teleport_terminal\"
				\"\$ENV{DESTDIR}/usr/local/bin/teleport_terminal\" SYMBOLIC)
		" COMPONENT client)
	endif()
endif()

include(CPack)
