#pragma once

#include <SDL.h>
#include "glm/glm.hpp"

namespace SOIS
{
  class Renderer
  {
  public:
    virtual ~Renderer() = 0 {};

    virtual void Initialize(SDL_Window*) {};

    virtual SDL_WindowFlags GetAdditionalWindowFlags() { return (SDL_WindowFlags)0;  };

    virtual void NewFrame() = 0;
    virtual void ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight) = 0;

    virtual void ClearRenderTarget(glm::vec4 aClearColor) = 0;
    virtual void RenderImguiData() = 0;
    virtual void Present() = 0;
  };
}