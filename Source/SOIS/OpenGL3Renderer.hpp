#pragma once


#include <SDL.h> // Include glfw3.h after our OpenGL definitions

#include "SOIS/Renderer.hpp"

namespace SOIS
{
  class OpenGL3Renderer : public Renderer
  {
  public:
    OpenGL3Renderer();
    ~OpenGL3Renderer() override;

    void Initialize(SDL_Window* aWindow) override;

    SDL_WindowFlags GetAdditionalWindowFlags() override;

    void NewFrame() override;
    void ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight) override;

    void ClearRenderTarget(glm::vec4 aClearColor) override;
    void RenderImguiData() override;
    void Present() override;
    
    std::unique_ptr<Texture> LoadTextureFromFile(std::string const& aFile) override;

  private:
    SDL_GLContext mContext;
    SDL_Window* mWindow = nullptr;
  };
}