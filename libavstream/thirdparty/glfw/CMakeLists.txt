cmake_minimum_required(VERSION 3.8)
add_library(glfw INTERFACE)

target_compile_definitions(glfw INTERFACE GLFW_INCLUDE_NONE)
target_include_directories(glfw INTERFACE include)

if(WIN32)
	target_link_libraries(glfw INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/win64/glfw3.lib)
endif()
