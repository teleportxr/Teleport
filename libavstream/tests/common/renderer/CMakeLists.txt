cmake_minimum_required(VERSION 3.8)

set(sources
	src/util.cpp
	src/util.hpp
)
set(headers
	include/librenderer.hpp
	include/renderer_interface.hpp
)

if(WIN32)
	set(sources ${sources}
		src/renderer_dx11.cpp
	)
	set(libraries ${libraries}
		d3d11
		d3dcompiler
	)
endif()

add_subdirectory(../../../thirdparty/glfw build)

add_library(renderer STATIC ${sources} ${headers})
set_target_properties( renderer PROPERTIES FOLDER AVStream/Test)
target_compile_features(renderer PUBLIC cxx_std_17)
target_include_directories(renderer PRIVATE src PUBLIC include)
target_link_libraries(renderer glfw libavstream imgui_impl ${libraries})