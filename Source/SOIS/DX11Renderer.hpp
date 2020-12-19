#pragma once

#define NOMINMAX
#include <winrt/base.h>
#include <d3d11.h>

#include <SDL.h> // Include glfw3.h after our OpenGL definitions

#include "SOIS/Renderer.hpp"

namespace SOIS
{
  class DX11Renderer : public Renderer
  {
  public:
    DX11Renderer();
    ~DX11Renderer() override;

    void Initialize(SDL_Window* aWindow) override;

    void NewFrame() override;
    void ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight) override;

    void ClearRenderTarget(glm::vec4 aClearColor) override;
    void RenderImguiData() override;
    void Present() override;


    // Ostensibly private
    bool CreateDeviceD3D(HWND hWnd);
    void CreateRenderTarget();
    void CleanupDeviceD3D();
    void CleanupRenderTarget();

    std::unique_ptr<Texture> LoadTextureFromFile(std::string const& aFile) override;

    winrt::com_ptr<ID3D11Device> mD3DDevice = nullptr;
    winrt::com_ptr<ID3D11DeviceContext> mD3DDeviceContext = nullptr;
    winrt::com_ptr<IDXGISwapChain> mSwapChain = nullptr;
    ID3D11RenderTargetView* mMainRenderTargetView = nullptr;
    SDL_Window* mWindow = nullptr;
  };
}