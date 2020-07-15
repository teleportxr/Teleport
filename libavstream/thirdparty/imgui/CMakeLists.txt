cmake_minimum_required(VERSION 3.8)

set(sources
	src/imgui.cpp
	src/imgui_demo.cpp
	src/imgui_draw.cpp
	src/imgui_widgets.cpp
)
set(headers
	src/imconfig.h
	src/imgui.h
	src/imgui_internal.h
	src/imstb_rectpack.h
	src/imstb_textedit.h
	src/imstb_truetype.h
)

add_library(imgui STATIC ${sources} ${headers})
target_include_directories(imgui PUBLIC src)
set_target_properties(imgui PROPERTIES FOLDER AVStream/Test)
