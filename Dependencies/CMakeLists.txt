
add_subdirectory(freetype2)

option(BUILD_SHARED_LIBS     "Build shared instead of static libraries." OFF)
option(OPTION_BUILD_TOOLS    "Build tools."                              OFF)
option(OPTION_BUILD_EXAMPLES "Build examples."                           OFF)
add_subdirectory(glbinding)

option(GLM_TEST_ENABLE "Build unit tests" OFF)
option(GLM_INSTALL_ENABLE "Allow installing of the library" OFF)
option(BUILD_SHARED_LIBS "Build shared library" OFF)
option(BUILD_STATIC_LIBS "Build static library" OFF)

add_subdirectory(glm)

target_compile_definitions(glm 
  INTERFACE 
    GLM_FORCE_CXX17 
    GLM_FORCE_SIZE_T_LENGTH 
    GLM_FORCE_DEPTH_ZERO_TO_ONE 
    GLM_FORCE_CTOR_INIT 
    GLM_ENABLE_EXPERIMENTAL 
    GLM_FORCE_SILENT_WARNINGS
)

target_sources(glm INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/glm/util/glm.natvis
)

add_subdirectory(DirectX-Headers)
add_subdirectory(nativefiledialog)
add_subdirectory(SDL)


#####################################
# Vulkan
#####################################
find_package(Vulkan)

set(VulkanIncludeDir ${Vulkan_INCLUDE_DIRS})
set(VulkanLib Vulkan::Vulkan)

#####################################
# ImGui
#####################################
add_library(imgui
    ImGuiSource/imconfig.h
    ImGuiSource/imgui.cpp
    ImGuiSource/imgui.h
    ImGuiSource/imgui_demo.cpp
    ImGuiSource/imgui_draw.cpp
    ImGuiSource/imgui_widgets.cpp
    ImGuiSource/imgui_internal.h
    ImGuiSource/misc/freetype/imgui_freetype.cpp
    ImGuiSource/misc/freetype/imgui_freetype.h
    ImGuiSource/misc/cpp/imgui_stdlib.cpp
    ImGuiSource/misc/cpp/imgui_stdlib.h
    ImGuiSource/backends/imgui_impl_sdl.cpp
    ImGuiSource/backends/imgui_impl_sdl.h
    ImGuiSource/backends/imgui_impl_dx12.cpp
    ImGuiSource/backends/imgui_impl_dx12.h
    ImGuiSource/backends/imgui_impl_vulkan.cpp
    ImGuiSource/backends/imgui_impl_vulkan.h
)

target_include_directories(imgui 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/ImGuiSource/
        ${CMAKE_CURRENT_SOURCE_DIR}/ImGuiSource/misc/freetype/
        ${CMAKE_CURRENT_SOURCE_DIR}/ImGuiSource/misc/cpp/
        ${CMAKE_CURRENT_SOURCE_DIR}/ImGuiSource/backends/
)

target_link_libraries(imgui PUBLIC STB SDL2-static glbinding freetype Vulkan::Vulkan)

#####################################
# ImGuizmo
#####################################
add_library(imguizmo
    ImGuizmo/ImCurveEdit.cpp
    ImGuizmo/ImCurveEdit.h
    ImGuizmo/ImGradient.cpp
    ImGuizmo/ImGradient.h
    ImGuizmo/ImGuizmo.cpp
    ImGuizmo/ImGuizmo.h
    ImGuizmo/ImSequencer.cpp
    ImGuizmo/ImSequencer.h
)

target_link_libraries(imguizmo PUBLIC imgui)

add_subdirectory(STB)
add_subdirectory(imgui_node_editor)