# Platform description for the custom "AndroidAGDE" system name set by
# CMake/AGDE.toolchain.cmake. CMake loads this automatically (via CMAKE_MODULE_PATH)
# because CMAKE_SYSTEM_NAME is AndroidAGDE; without it CMake would warn that the
# system is unknown and apply no platform defaults at all.
#
# The target really is Android (Linux-based, ELF, Bionic libc), so this mirrors the
# relevant parts of CMake's own Android platform modules. Note that with a Visual
# Studio generator most link-rule variables are unused — MSBuild and AGDE's Clang
# toolset drive the actual tools — but the artefact naming below determines the
# output file names CMake writes into the vcxproj files.

set(ANDROID 1)
set(UNIX 1)

set(CMAKE_DL_LIBS "dl")

# ELF artefact naming: lib<name>.a / lib<name>.so, no import libraries, no .exe suffix.
set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")
set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
set(CMAKE_SHARED_MODULE_PREFIX "lib")
set(CMAKE_SHARED_MODULE_SUFFIX ".so")
set(CMAKE_EXECUTABLE_SUFFIX "")
set(CMAKE_LINK_LIBRARY_SUFFIX "")

set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".a")

set(CMAKE_SHARED_LIBRARY_SONAME_C_FLAG "-Wl,-soname,")
set(CMAKE_SHARED_LIBRARY_SONAME_CXX_FLAG "-Wl,-soname,")

# Android only supports position-independent code.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
