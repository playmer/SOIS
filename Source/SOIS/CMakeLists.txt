target_sources(SOIS 
PRIVATE
    ImGuiSample.hpp
    ApplicationContext.cpp
    ApplicationContext.hpp
    OpenGL3Creator.cpp
    OpenGL3Renderer.cpp
    OpenGL3Renderer.hpp
    Renderer.hpp
)

if (WIN32)
    target_sources(SOIS 
    PRIVATE
        DX11Creator.cpp
        DX11Renderer.cpp
        DX11Renderer.hpp
    )
endif()