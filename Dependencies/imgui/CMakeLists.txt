get_filename_component(ParentDirectory ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
include_directories(${ParentDirectory} ${CMAKE_CURRENT_SOURCE_DIR})

add_library(imgui 
    imconfig.h
    imgui.cpp
    imgui.h
    imgui_freetype.cpp
    imgui_freetype.h
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_widgets.cpp
    imgui_internal.h
    imgui_stdlib.cpp
    imgui_stdlib.h
    imgui_impl_sdl.cpp
    imgui_impl_sdl.h
    imgui_impl_dx11.cpp
    imgui_impl_dx11.h
    imgui_impl_opengl3.cpp
    imgui_impl_opengl3.h)

target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(imgui PUBLIC STB SDL2-static glbinding freetype)
