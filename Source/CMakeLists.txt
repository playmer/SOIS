add_library(SimpleSDL2ImguiAppFramework)

target_sources(SimpleSDL2ImguiAppFramework 
PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/ImGuiSample.hpp
    ${CMAKE_CURRENT_LIST_DIR}/ApplicationContext.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ApplicationContext.hpp
    ${CMAKE_CURRENT_LIST_DIR}/OpenGL3Renderer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/OpenGL3Renderer.hpp
    ${CMAKE_CURRENT_LIST_DIR}/DX11Renderer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/DX11Renderer.hpp
    ${CMAKE_CURRENT_LIST_DIR}/Renderer.hpp
)

target_include_directories(SimpleSDL2ImguiAppFramework 
PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}
    ${DependenciesDirectory}/glm)

target_link_libraries(SimpleSDL2ImguiAppFramework 
PRIVATE 
    SDL2-static
    SDL2main
    glm_static
    imgui
    imgui_node_editor
    STB
    nativefiledialog
    glbinding
    D3D11.lib
)

target_compile_definitions(SimpleSDL2ImguiAppFramework PRIVATE GLFW_INCLUDE_NONE)

if (${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
    target_link_libraries(SimpleSDL2ImguiAppFramework PRIVATE opengl32.lib)

    target_compile_options(SimpleSDL2ImguiAppFramework PRIVATE 
    PUBLIC
        -permissive- -std:c++17
    PRIVATE
        -WX- -W4
    )
endif()

set_target_properties(SimpleSDL2ImguiAppFramework PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${AssetDirectory}/Bin)
