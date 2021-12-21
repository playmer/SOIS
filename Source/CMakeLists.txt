add_library(SOIS)

add_subdirectory(SOIS)

target_include_directories(SOIS 
PUBLIC 
    ${CMAKE_CURRENT_LIST_DIR}
    ${DependenciesDirectory}/glm
)

target_link_libraries(SOIS 
PUBLIC 
    SDL2-static
    SDL2main
    glm::glm
    imgui
    imgui_node_editor
    STB
    nativefiledialog
    glbinding
)

target_compile_definitions(SOIS PRIVATE GLFW_INCLUDE_NONE)
target_compile_features(SOIS PUBLIC cxx_std_17)

if (${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
    target_link_libraries(SOIS 
    PRIVATE 
        opengl32.lib
        D3D11.lib
    )

    target_compile_options(SOIS PRIVATE 
    PUBLIC
        -permissive-
    PRIVATE
        -WX- -W4
    )
endif()
