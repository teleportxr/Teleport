cmake_minimum_required(VERSION 3.8)

set(sources
	src/imgui_impl_glfw.cpp
)
set(headers
	src/imgui_impl_glfw.h
)

if(WIN32)
	set(sources ${sources}
		src/imgui_impl_dx11.cpp
	)
	set(headers ${headers}
		src/imgui_impl_dx11.h
	)
endif()

add_subdirectory(../../../thirdparty/imgui build)

add_library(imgui_impl STATIC ${sources} ${headers})
target_include_directories(imgui_impl PUBLIC src)
target_link_libraries(imgui_impl imgui glfw ${libraries})
set_target_properties(imgui_impl PROPERTIES FOLDER AVStream/Test)
