target_sources(SOIS 
PRIVATE
    ImGuiSample.hpp
    ApplicationContext.cpp
    ApplicationContext.hpp
    OpenGL3Creator.cpp
    OpenGL3Renderer.cpp
    OpenGL3Renderer.hpp
    Renderer.cpp
    Renderer.hpp
)

find_package(Vulkan)

if (${Vulkan_FOUND})
    target_sources(SOIS
    PRIVATE
        VulkanCreator.cpp
        VulkanRenderer.cpp
        VulkanRenderer.hpp
    )

    target_include_directories(SOIS PUBLIC ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(SOIS 
    PUBLIC 
        Vulkan::Vulkan 
        vk-bootstrap::vk-bootstrap
    )
endif()

if (WIN32)
    target_sources(SOIS 
    PRIVATE
        DX11Creator.cpp
        DX11Renderer.cpp
        DX11Renderer.hpp
        DX12Creator.cpp
        DX12Renderer.cpp
        DX12Renderer.hpp
    )
    
    target_link_libraries(SOIS 
    PUBLIC
        DirectX-Headers 
        d3d12.lib
        dxgi.lib
        dxguid.lib
    )
endif()
