#pragma once

#include <memory>
#include <string>

#include <SDL.h>
#include "glm/glm.hpp"

namespace SOIS
{
  class Texture
  {
  public:
      Texture(
      int aWidth,
      int aHeight)
          : Width{aWidth}
          , Height{aHeight}
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
    virtual ~Renderer() = 0 {};

    virtual void Initialize(SDL_Window*) {};

    virtual SDL_WindowFlags GetAdditionalWindowFlags() { return (SDL_WindowFlags)0;  };

    virtual void NewFrame() = 0;
    virtual void ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight) = 0;

    virtual std::unique_ptr<Texture> LoadTextureFromFile(std::string const& aFile) = 0;

    virtual void ClearRenderTarget(glm::vec4 aClearColor) = 0;
    virtual void RenderImguiData() = 0;
    virtual void Present() = 0;
  };
}