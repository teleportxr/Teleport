cmake_minimum_required(VERSION 3.10.2)

#------VULKAN------
#Sets the Vulkan Include directory for source code build.
set(ANDROID_VULKAN_INCLUDE_DIR ${ANDROID_NDK}/sources/third_party/vulkan/src/include)
include_directories(${ANDROID_VULKAN_INCLUDE_DIR})

#---ANDROID_GLUE---
#Sets up the application for native c++ usage.
set(ANDROID_GLUE_INCLUDE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
include_directories(${ANDROID_GLUE_INCLUDE_DIR})
add_library(native_app_glue STATIC ${ANDROID_GLUE_INCLUDE_DIR}/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC ${ANDROID_GLUE_INCLUDE_DIR})

#---ANDROID_LOG----
#Find Android's Logging Library
find_library(log-lib log)

#-----BUILD_OPT----
#Sets C++17 and build arguements
#Sets the VK_USE_PLATFORM_ANDROID_KHR and _DEBUG definition

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Werror -Wno-unused-variable -Wno-self-assign-field -DVK_USE_PLATFORM_ANDROID_KHR -D_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS}")
#message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
#message(STATUS "CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")

#----EXPORT_JNI----
#Set the export of JNIEXPORT void ANativeActivity_onCreate(...);
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

#----TeleportVRQuestClient-----
include_directories(src/main/cpp)
include_directories(${ANDROID_NDK}/sources)
add_library(TeleportVRQuestClient SHARED
        src/main/cpp/vulkan_wrapper.cpp          #Load PFNs
        src/main/cpp/main.cpp                    #entry point
        )
target_link_libraries(TeleportVRQuestClient
        android
        native_app_glue
        ${log-lib}
        )