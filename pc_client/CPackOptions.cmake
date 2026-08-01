
# get commit id from a file:
set(TELEPORT_COMMIT $ENV{TELEPORT_COMMIT})
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
# CPACK_TELEPORT_PACKAGE_ARCH is set in CMake/TeleportPackaging.cmake: x64 on Windows and
# Linux, arm64 on macOS. It has to be a CPACK_* name to survive into CPackConfig.cmake,
# which is all this file can see. Default kept for a stale CPackConfig.cmake from before
# the variable existed.
if(NOT CPACK_TELEPORT_PACKAGE_ARCH)
	set(CPACK_TELEPORT_PACKAGE_ARCH "x64")
endif()
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${TELEPORT_COMMIT}-${CPACK_TELEPORT_PACKAGE_ARCH}")
message("CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_FILE_NAME}")
 