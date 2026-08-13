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
   jcs
   manifest
)
# The avatars test exercises the codecs defined in TeleportCore/AvatarsJson.cpp,
# so it must link against TeleportCore.
target_link_libraries(avatars PRIVATE TeleportCore)

# jcs and manifest exercise TeleportCore/Jcs.cpp and ManifestJson.cpp. The
# manifest header pulls in libavstream/common_exports.h transitively via
# Avatars.h, so both need that include path for the avs::uid alias.
target_link_libraries(jcs PRIVATE TeleportCore)
target_link_libraries(manifest PRIVATE TeleportCore)
target_include_directories(jcs PRIVATE ${CMAKE_SOURCE_DIR}/libavstream/export_include)
target_include_directories(manifest PRIVATE ${CMAKE_SOURCE_DIR}/libavstream/export_include)

# manifest_verify is the cross-language contract test: a manifest signed by
# the Node.js implementation, verified by this one. It compiles the
# ManifestVerify translation unit directly rather than linking the whole
# TeleportServer shared library (which on Linux additionally depends on CUDA
# and Compressonator at link time), and needs OpenSSL for Ed25519.
add_executable(manifest_verify
   test_manifest_verify.cpp
   ${CMAKE_SOURCE_DIR}/TeleportServer/ManifestVerify.cpp)
add_test(NAME manifest_verify COMMAND manifest_verify)
target_link_libraries(manifest_verify PRIVATE Catch2::Catch2WithMain TeleportCore)
target_include_directories(manifest_verify PRIVATE
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_SOURCE_DIR}/TeleportServer/export_include
   ${CMAKE_SOURCE_DIR}/libavstream/export_include
   ${CMAKE_SOURCE_DIR}/thirdparty/openssl/include)
if(WIN32)
   target_link_libraries(manifest_verify PRIVATE libssl libcrypto)
else()
   target_link_libraries(manifest_verify PRIVATE ssl crypto)
endif()

# manifest_resolver drives the six-stage evaluation sequence with an injected
# transport, and exercises the SSRF guard directly. It compiles the resolver
# and verifier translation units rather than linking TeleportServer, and needs
# libcurl (the resolver's default transport) and OpenSSL.
add_executable(manifest_resolver
   test_manifest_resolver.cpp
   ${CMAKE_SOURCE_DIR}/TeleportServer/DefaultAvatarManifestResolver.cpp
   ${CMAKE_SOURCE_DIR}/TeleportServer/ManifestVerify.cpp)
add_test(NAME manifest_resolver COMMAND manifest_resolver)
target_link_libraries(manifest_resolver PRIVATE Catch2::Catch2WithMain TeleportCore libcurl)
target_include_directories(manifest_resolver PRIVATE
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_SOURCE_DIR}/TeleportServer/export_include
   ${CMAKE_SOURCE_DIR}/libavstream/export_include
   ${CMAKE_SOURCE_DIR}/thirdparty/openssl/include
   ${curl_include_paths})
if(WIN32)
   target_link_libraries(manifest_resolver PRIVATE libssl libcrypto)
else()
   target_link_libraries(manifest_resolver PRIVATE ssl crypto)
endif()

# manifest_cache exercises the header-only LRU used by the manifest resolver.
add_executable(manifest_cache test_manifest_cache.cpp)
add_test(NAME manifest_cache COMMAND manifest_cache)
target_link_libraries(manifest_cache PRIVATE Catch2::Catch2WithMain TeleportCore)
target_include_directories(manifest_cache PRIVATE
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_SOURCE_DIR}/TeleportServer/export_include
   ${CMAKE_SOURCE_DIR}/libavstream/export_include)

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

# animation_state pins the behaviour of AnimationLayerStateSequence: the timeline of
# animation states a layer plays through, and how a query between two of them blends.
# Like anim it compiles the translation unit directly rather than linking ClientRender,
# and supplies the two clientrender::Animation members the sequence reads. The extra
# include paths are for the headers AnimationState.h pulls in transitively: tiny_gltf.h
# from draco's vendored copy, and ClientRender's own ozz/gltf2ozz.h.
if(TARGET ozz_animation)
   add_executable(animation_state
      test_animation_state.cpp
      ${CMAKE_SOURCE_DIR}/ClientRender/NodeComponents/AnimationState.cpp)
   add_test(NAME animation_state COMMAND animation_state)
   target_link_libraries(animation_state PRIVATE Catch2::Catch2WithMain TeleportCore ozz_animation_offline ozz_animation ozz_base)
   target_include_directories(animation_state PRIVATE
      ${CMAKE_SOURCE_DIR}
      ${CMAKE_SOURCE_DIR}/ClientRender
      ${CMAKE_SOURCE_DIR}/libavstream/export_include
      ${CMAKE_SOURCE_DIR}/libavstream/include
      ${CMAKE_SOURCE_DIR}/firstparty
      ${CMAKE_SOURCE_DIR}/thirdparty/draco/third_party/tinygltf)
   target_compile_definitions(animation_state PRIVATE TINYGLTF_NO_STB_IMAGE=1)
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

# repl_command_parser exercises the whitespace-tokenising line parser shared by the
# local control server (CommandProcessor) and the legacy stdin REPL grammar. Compiles
# ReplCommandParser.cpp directly, so it needs neither libavstream nor TeleportClient.
add_executable(repl_command_parser
   test_repl_command_parser.cpp
   ${CMAKE_SOURCE_DIR}/HeadlessClient/ReplCommandParser.cpp)
add_test(NAME repl_command_parser COMMAND repl_command_parser)
target_link_libraries(repl_command_parser PRIVATE Catch2::Catch2WithMain)
target_include_directories(repl_command_parser PRIVATE ${CMAKE_SOURCE_DIR}/HeadlessClient)

# control_protocol exercises the dot-stuffed wire framing in ControlProtocol.h
# (header-only) shared by teleportd's control server and teleport_cli, plus the
# ReplCommandParser tokeniser both of them use. Compiles ReplCommandParser.cpp
# directly (not header-only), same as repl_command_parser above.
add_executable(control_protocol
   test_control_protocol.cpp
   ${CMAKE_SOURCE_DIR}/HeadlessClient/ReplCommandParser.cpp)
add_test(NAME control_protocol COMMAND control_protocol)
target_link_libraries(control_protocol PRIVATE Catch2::Catch2WithMain)
target_include_directories(control_protocol PRIVATE ${CMAKE_SOURCE_DIR}/HeadlessClient)

# control_processor is a behavioural test for CommandProcessor, the local control
# server's command dispatcher, run against a real ConnectionManager with no
# connections created (see the test file for why "connect" itself is out of scope
# here). ConnectionManager pulls in HeadlessConnection and so the whole of
# TeleportClient, hence linking the same teleport_headless_core static library
# teleportd and teleport_cli are built from, guarded the same way the
# identity test above guards on TeleportClient.
if(TARGET teleport_headless_core)
   add_executable(control_processor test_control_processor.cpp)
   add_test(NAME control_processor COMMAND control_processor)
   target_link_libraries(control_processor PRIVATE Catch2::Catch2WithMain teleport_headless_core)
   target_include_directories(control_processor PRIVATE ${CMAKE_SOURCE_DIR}/HeadlessClient)
endif()

# End-to-end: launch teleportd and drive it with teleport_cli over the real control
# socket (ping, multi-connection management, CLI exit without disconnecting the
# service, remote shutdown). Bash only; Windows CI uses the PowerShell equivalent.
if(TARGET teleportd AND (TELEPORT_UNIX OR TELEPORT_MACOS))
   add_test(NAME control_integration
      COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/control_integration.sh
         $<TARGET_FILE:teleportd> $<TARGET_FILE:teleport_cli>)
endif()

# gltf_texture_scan is the server half of the same story: finding the external texture uris a
# .glb/.vrm declares, so they can be streamed with the mesh. It compiles the one translation
# unit rather than linking TeleportServer, which pulls in CUDA and Compressonator at link time.
add_executable(gltf_texture_scan
   test_gltf_texture_scan.cpp
   ${CMAKE_SOURCE_DIR}/TeleportServer/GltfTextureScan.cpp)
add_test(NAME gltf_texture_scan COMMAND gltf_texture_scan)
target_link_libraries(gltf_texture_scan PRIVATE Catch2::Catch2WithMain TeleportCore)
target_include_directories(gltf_texture_scan PRIVATE
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_SOURCE_DIR}/TeleportServer/export_include
   ${CMAKE_SOURCE_DIR}/libavstream/export_include)

# resource_url pins how a client resolves the external texture uris inside a .glb against
# the url it fetched the asset from. It compiles the one translation unit rather than
# linking ClientRender, which would drag in Vulkan and the whole render platform for what
# is pure string handling.
add_executable(resource_url
   test_resource_url.cpp
   ${CMAKE_SOURCE_DIR}/ClientRender/ResourceUrl.cpp)
add_test(NAME resource_url COMMAND resource_url)
target_link_libraries(resource_url PRIVATE Catch2::Catch2WithMain)
target_include_directories(resource_url PRIVATE ${CMAKE_SOURCE_DIR})

# Guarded on the target rather than built unconditionally: a headless-only build
# (macOS, TELEPORT_GUI_CLIENT=OFF) never adds TeleportAudio, and the PulseAudio
# pkg-config probe below is REQUIRED, so it would hard-fail at configure time.
if(TARGET TeleportAudio)
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
endif()
