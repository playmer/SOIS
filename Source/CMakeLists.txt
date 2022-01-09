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
    imguizmo
    STB
    nativefiledialog
    glbinding
)

target_compile_definitions(SOIS PRIVATE GLFW_INCLUDE_NONE)

# Android is causing issues on C++20, and due to how target_compile_features interacts with command line order
# and PUBLIC props, we need to just use target_compile_options directly on Android until the NDK/CMake supports
# cxx_std_20 properly and not using gnu extensions.
if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL Android)
    target_compile_features(SOIS PUBLIC cxx_std_20)
else()
    target_compile_options(SOIS PRIVATE -std=c++20)
endif()

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL Android)
    if (${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
        target_link_libraries(SOIS 
        PRIVATE 
            opengl32.lib
            D3D11.lib
        )
    
        target_compile_options(SOIS 
        PUBLIC
            -permissive-
        PRIVATE
            -WX- -W4
        )
    endif()
endif()
