cmake_minimum_required(VERSION 2.9)

file(GLOB shaders "../*.sfx" )
file(GLOB shader_includes "../*.sl" )
file(GLOB global_shader_includes "${SIMUL_PLATFORM_DIR}/Shaders/SL/*.sl")

set( JSON_FILE_D3D12 "${SIMUL_PLATFORM_DIR}/DirectX12/Sfx/DirectX12.json" )

add_sfx_shader_project( DirectX12Shaders ${JSON_FILE_D3D12} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Sfx" "${SIMUL_PLATFORM_DIR}/Shaders/SL" "${SIMUL_PLATFORM_DIR}/DirectX12/Sfx" SOURCES ${shaders} ${shader_includes} ${global_shader_includes} ${JSON_FILE_D3D12} OPTIONS -w OUTPUT "${CMAKE_BINARY_DIR}/firstparty/Platform/DirectX12/shaderbin" DEFINES FOLDER Shaders )
