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
   anim
   avatars
)
# The avatars test exercises the codecs defined in TeleportCore/AvatarsJson.cpp,
# so it must link against TeleportCore (the anim test currently has no body).
target_link_libraries(avatars PRIVATE TeleportCore)
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
endif()