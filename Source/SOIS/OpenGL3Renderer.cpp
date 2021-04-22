
#include <glbinding/gl/gl.h>
#include <glbinding/glbinding.h>


#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "SOIS/OpenGL3Renderer.hpp"

namespace SOIS
{
  static char const* Source(gl::GLenum source)
  {
    switch (source)
    {
    case gl::GL_DEBUG_SOURCE_API: return "DEBUG_SOURCE_API";
    case gl::GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "DEBUG_SOURCE_WINDOW_SYSTEM";
    case gl::GL_DEBUG_SOURCE_SHADER_COMPILER: return "DEBUG_SOURCE_SHADER_COMPILER";
    case gl::GL_DEBUG_SOURCE_THIRD_PARTY: return "DEBUG_SOURCE_THIRD_PARTY";
    case gl::GL_DEBUG_SOURCE_APPLICATION: return "DEBUG_SOURCE_APPLICATION";
    case gl::GL_DEBUG_SOURCE_OTHER: return "DEBUG_SOURCE_OTHER";
    default: return "unknown";
    }
  }

  static char const* Severity(gl::GLenum severity)
  {
    switch (severity)
    {
    case gl::GL_DEBUG_SEVERITY_HIGH: return "DEBUG_SEVERITY_HIGH";
    case gl::GL_DEBUG_SEVERITY_MEDIUM: return "DEBUG_SEVERITY_MEDIUM";
    case gl::GL_DEBUG_SEVERITY_LOW: return "DEBUG_SEVERITY_LOW";
    case gl::GL_DEBUG_SEVERITY_NOTIFICATION: return "DEBUG_SEVERITY_NOTIFICATION";
    default: return "unknown";
    }
  }


  static char const* Type(gl::GLenum type)
  {
    switch (type)
    {
    case gl::GL_DEBUG_TYPE_ERROR: return "DEBUG_TYPE_ERROR";
    case gl::GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "DEBUG_TYPE_DEPRECATED_BEHAVIOR";
    case gl::GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "DEBUG_TYPE_UNDEFINED_BEHAVIOR";
    case gl::GL_DEBUG_TYPE_PORTABILITY: return "DEBUG_TYPE_PORTABILITY";
    case gl::GL_DEBUG_TYPE_PERFORMANCE: return "DEBUG_TYPE_PERFORMANCE";
    case gl::GL_DEBUG_TYPE_MARKER: return "DEBUG_TYPE_MARKER";
    case gl::GL_DEBUG_TYPE_PUSH_GROUP: return "DEBUG_TYPE_PUSH_GROUP";
    case gl::GL_DEBUG_TYPE_POP_GROUP: return "DEBUG_TYPE_POP_GROUP";
    case gl::GL_DEBUG_TYPE_OTHER: return "DEBUG_TYPE_OTHER";
    default: return "unknown";
    }
  }

  static
    void GL_APIENTRY messageCallback(gl::GLenum source,
      gl::GLenum type,
      gl::GLuint id,
      gl::GLenum severity,
      gl::GLsizei length,
      const gl::GLchar* message,
      const void* userParam)
  {
    if (gl::GL_DEBUG_SEVERITY_NOTIFICATION == severity)
    {
      return;
    }

    printf("GL DEBUG CALLBACK:\n    Source = %s\n    type = %s\n    severity = %s\n    message = %s\n",
      Source(source),
      Type(type),
      Severity(severity),
      message);
  }

  glbinding::ProcAddress GLFunctionLoader(const char* aName)
  {
    return reinterpret_cast<glbinding::ProcAddress>(SDL_GL_GetProcAddress(aName));
  }

  // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 Core + GLSL 150
  static const char* gGlslVersion = "#version 150";
#else
    // GL 3.0 + GLSL 130
  static const char* gGlslVersion = "#version 130";
#endif

  OpenGL3Renderer::OpenGL3Renderer()
    : Renderer{}
  {
    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  }


  SDL_WindowFlags OpenGL3Renderer::GetAdditionalWindowFlags()
  {
    return SDL_WINDOW_OPENGL;
  }


  void OpenGL3Renderer::Initialize(SDL_Window* aWindow)
  {
    mWindow = aWindow;

    mContext = SDL_GL_CreateContext(mWindow);
    SDL_GL_MakeCurrent(mWindow, mContext);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Initialize OpenGL loader
    glbinding::initialize(GLFunctionLoader);

    gl::glEnable(gl::GL_DEBUG_OUTPUT);
    gl::glDebugMessageCallback(messageCallback, this);

    ImGui_ImplSDL2_InitForOpenGL(mWindow, mContext);
    ImGui_ImplOpenGL3_Init(gGlslVersion);
  }


  OpenGL3Renderer::~OpenGL3Renderer()
  {
    ImGui_ImplOpenGL3_Shutdown();
  }

  void OpenGL3Renderer::NewFrame()
  {
    ImGui_ImplOpenGL3_NewFrame();
  }

  void OpenGL3Renderer::ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight)
  {
    // Nothing special here, taken care of when we clear the render target.
  }

  void OpenGL3Renderer::ClearRenderTarget(glm::vec4 aClearColor)
  {
    // Clear the viewport to prepare for user rendering.
    SDL_GL_MakeCurrent(mWindow, mContext);
    ImGuiIO& io = ImGui::GetIO();
    gl::glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    gl::glClearColor(aClearColor.x, aClearColor.y, aClearColor.z, aClearColor.w);
    gl::glClear(gl::GL_COLOR_BUFFER_BIT);
  }

  void OpenGL3Renderer::RenderImguiData()
  {
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  }

  void OpenGL3Renderer::Present()
  {
    // Swap the buffers and prepare for next frame.
    SDL_GL_MakeCurrent(mWindow, mContext);
    SDL_GL_SwapWindow(mWindow);

  }

  class OpenGL3Texture : public Texture
  {
  public:
      OpenGL3Texture(int aWidth, int aHeight)
          : Texture{ aWidth, aHeight }
      {

      }

      ~OpenGL3Texture() override
      {
      }
  };
  
  std::unique_ptr<Texture> OpenGL3Renderer::LoadTextureFromData(unsigned char* data, int format, int w, int h, int pitch)
  {
      return nullptr;
  }

  std::unique_ptr<Texture> OpenGL3Renderer::LoadTextureFromFile(std::string const& aFile)
  {
      return nullptr;
  }
}