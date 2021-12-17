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
    #glbinding
    #D3D11.lib
)

target_compile_definitions(SOIS PRIVATE GLFW_INCLUDE_NONE)

if (${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
    target_link_libraries(SOIS 
    PRIVATE 
        opengl32.lib 
        DirectX-Headers #technically I guess this could be used for a WSL target...not sure how to detect that at Runtime.
    )

    target_compile_options(SOIS PRIVATE 
    PUBLIC
        -permissive- -std:c++17
    PRIVATE
        -WX- -W4
    )
endif()

get_target_property(targetSources SOIS SOURCES)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} PREFIX "" FILES ${targetSources}) 