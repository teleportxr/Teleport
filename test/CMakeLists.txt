enable_testing()

if(TELEPORT_TEST)

function(SETUP_TESTS)
    foreach(_testname ${ARGN})
        add_executable(${_testname} test_${_testname}.cpp )
        add_test(NAME ${_testname} COMMAND ${_testname})
        target_link_libraries(${_testname} PRIVATE Catch2::Catch2WithMain)
    endforeach()
endfunction()

SETUP_TESTS(
   avatars
)
# The avatars test exercises the codecs defined in TeleportCore/AvatarsJson.cpp,
# so it must link against TeleportCore.
target_link_libraries(avatars PRIVATE TeleportCore)

# anim exercises the animation retargeter's bind-frame conjugation maths.
# It compiles the AnimationRetargeter translation unit directly so it doesn't
# need the whole ClientRender library (Vulkan, draco, ktx etc.). Requires the
# ozz targets, which are only fetched when TELEPORT_CLIENT is enabled.
if(TARGET ozz_animation)
   add_executable(anim
      test_anim.cpp
      ${CMAKE_SOURCE_DIR}/ClientRender/AnimationRetargeter.cpp)
   add_test(NAME anim COMMAND anim)
   target_link_libraries(anim PRIVATE Catch2::Catch2WithMain TeleportCore ozz_animation_offline ozz_animation ozz_base)
   target_include_directories(anim PRIVATE ${CMAKE_SOURCE_DIR})
endif()
# TeleportCore's public Avatars.h header transitively pulls in
# libavstream/common_exports.h for the avs::uid alias; the include
# path isn't propagated by the static library, so wire it up here.
target_include_directories(avatars PRIVATE ${CMAKE_SOURCE_DIR}/libavstream/export_include)

# avatar_service is a behavioural test for the server-side AvatarService.
# It compiles the AvatarService translation unit directly so it doesn't
# need to link the whole TeleportServer shared library (which on Linux
# additionally depends on CUDA + Compressonator at link time).
add_executable(avatar_service
   test_avatar_service.cpp
   ${CMAKE_SOURCE_DIR}/TeleportServer/AvatarService.cpp)
add_test(NAME avatar_service COMMAND avatar_service)
target_link_libraries(avatar_service PRIVATE Catch2::Catch2WithMain TeleportCore)
target_include_directories(avatar_service PRIVATE
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_SOURCE_DIR}/TeleportServer/export_include
   ${CMAKE_SOURCE_DIR}/libavstream/export_include)

# avatar_manager is a behavioural test for the client-side AvatarManager.
# It compiles the AvatarManager translation unit directly so it doesn't
# pull in the whole TeleportClient static library (which carries OpenSSL,
# libsecret, libdatachannel etc. as link-time dependencies).
add_executable(avatar_manager
   test_avatar_manager.cpp
   ${CMAKE_SOURCE_DIR}/TeleportClient/AvatarManager.cpp)
add_test(NAME avatar_manager COMMAND avatar_manager)
target_link_libraries(avatar_manager PRIVATE Catch2::Catch2WithMain TeleportCore)
target_include_directories(avatar_manager PRIVATE
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_SOURCE_DIR}/libavstream/export_include)

# redact exercises the header-only URL/proof redaction helpers in
# TeleportCore/Redact.h. Header-only, so no TeleportCore link is needed —
# just the include path.
add_executable(redact test_redact.cpp)
add_test(NAME redact COMMAND redact)
target_link_libraries(redact PRIVATE Catch2::Catch2WithMain)
target_include_directories(redact PRIVATE ${CMAKE_SOURCE_DIR}/TeleportCore/export_include)

# avatar_validation_cache exercises the header-only LRU cache used by
# host-supplied IAvatarValidator implementations (plan §4.1). No
# TeleportServer link is required — the header pulls in IAvatarValidator
# which transitively needs TeleportCore for AvatarOffer / AvatarRequirements.
add_executable(avatar_validation_cache
   test_avatar_validation_cache.cpp)
add_test(NAME avatar_validation_cache COMMAND avatar_validation_cache)
target_link_libraries(avatar_validation_cache PRIVATE Catch2::Catch2WithMain TeleportCore)
target_include_directories(avatar_validation_cache PRIVATE
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_SOURCE_DIR}/TeleportServer/export_include
   ${CMAKE_SOURCE_DIR}/libavstream/export_include)

# spatial_audio exercises the client-side SpatialAudioMixer: the spatialisation
# maths (header-only) and multi-source Opus mixing. Links the TeleportAudio
# static library (which carries the mixer + LinuxAudioPlayer/PulseAudio) and
# libopus for the test's synthetic Opus frames.
# identity exercises the provider-agnostic identity manager with a fake provider:
# that a sign-in is remembered across sessions without contacting the provider, and
# that signing out forgets it. It links the TeleportClient library, since the manager
# transitively reaches Config, the platform FileLoader, and the OS credential store.
if(TARGET TeleportClient)
   add_executable(identity test_identity.cpp)
   add_test(NAME identity COMMAND identity)
   target_link_libraries(identity PRIVATE Catch2::Catch2WithMain TeleportClient TeleportCore Core_MT)
   target_include_directories(identity PRIVATE
      ${CMAKE_SOURCE_DIR}
      ${CMAKE_SOURCE_DIR}/libavstream/export_include
      ${CMAKE_SOURCE_DIR}/thirdparty/json)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSEAUDIO REQUIRED libpulse-simple)
add_executable(spatial_audio test_spatial_audio.cpp)
add_test(NAME spatial_audio COMMAND spatial_audio)
target_link_libraries(spatial_audio PRIVATE Catch2::Catch2WithMain TeleportAudio TeleportCore opus ${PULSEAUDIO_LIBRARIES})
target_include_directories(spatial_audio PRIVATE
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_SOURCE_DIR}/TeleportAudio
   ${CMAKE_SOURCE_DIR}/libavstream/include
   ${CMAKE_SOURCE_DIR}/libavstream/export_include
   ${CMAKE_SOURCE_DIR}/firstparty)
endif()