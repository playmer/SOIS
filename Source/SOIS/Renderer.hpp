#pragma once

#include <memory>
#include <string>

#include <SDL.h>
#include "glm/glm.hpp"

namespace SOIS
{
  class OpenGL3Renderer;
  class DX11Renderer;
  class Renderer;

  // Make the renderers, we put these into their own cpp files to simplify the code around
  // compiling them on different platforms.
  std::unique_ptr<Renderer> MakeOpenGL3Renderer();
  std::unique_ptr<Renderer> MakeDX11Renderer();

  enum class TextureLayout
  {
    RGBA_Unorm,
    RGBA_Srgb,
    Bc1_Rgba_Unorm,
    Bc1_Rgba_Srgb,
    Bc3_Srgb,
    Bc3_Unorm,
    Bc7_Unorm,
    Bc7_Srgb,
    InvalidLayout
  };


  class Texture
  {
  public:
    Texture(
      int aWidth,
      int aHeight)
      : Width{ aWidth }
      , Height{ aHeight }
    {

    }

    virtual ~Texture()
    {
    };

    virtual void* GetTextureId()
    {
      return nullptr;
    };

    int Width;
    int Height;
  };

  class Renderer
  {
  public:
    virtual ~Renderer() {};

    virtual void Initialize(SDL_Window*) {};

    virtual SDL_WindowFlags GetAdditionalWindowFlags() { return (SDL_WindowFlags)0; };

    virtual void NewFrame() = 0;
    virtual void ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight) = 0;


    virtual std::unique_ptr<Texture> LoadTextureFromData(unsigned char* data, TextureLayout format, int w, int h, int pitch) = 0;
    virtual std::unique_ptr<Texture> LoadTextureFromFile(std::string const& aFile) = 0;

    virtual void ClearRenderTarget(glm::vec4 aClearColor) = 0;
    virtual void RenderImguiData() = 0;
    virtual void Present() = 0;
  };
}
