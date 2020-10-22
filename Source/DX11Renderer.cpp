#include <SDL_syswm.h>

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_dx11.h"

#include "DX11Renderer.hpp"

namespace SOIS
{
  ////////////////////
  // ImGui Helpers
  bool DX11Renderer::CreateDeviceD3D(HWND hWnd)
  {
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &mSwapChain, &mD3DDevice, &featureLevel, &mD3DDeviceContext) != S_OK)
    {
      throw "Bad device and swapchain";
      return false;
    }

    CreateRenderTarget();
    return true;
  }

  void DX11Renderer::CleanupDeviceD3D()
  {
    CleanupRenderTarget();
    if (mSwapChain) { mSwapChain->Release(); mSwapChain = NULL; }
    if (mD3DDeviceContext) { mD3DDeviceContext->Release(); mD3DDeviceContext = NULL; }
    if (mD3DDevice) { mD3DDevice->Release(); mD3DDevice = NULL; }
  }

  void DX11Renderer::CreateRenderTarget()
  {
    ID3D11Texture2D* pBackBuffer;
    mSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    mD3DDevice->CreateRenderTargetView(pBackBuffer, NULL, &mMainRenderTargetView);
    pBackBuffer->Release();
  }

  void DX11Renderer::CleanupRenderTarget()
  {
    if (mMainRenderTargetView) { mMainRenderTargetView->Release(); mMainRenderTargetView = NULL; }
  }

  DX11Renderer::DX11Renderer()
    : Renderer{}
  {
  }


  void DX11Renderer::Initialize(SDL_Window* aWindow)
  {
    mWindow = aWindow;

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(mWindow, &wmInfo);
    HWND hwnd = (HWND)wmInfo.info.win.window;

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
      CleanupDeviceD3D();
      return;
    }

    ImGui_ImplSDL2_InitForD3D(mWindow);
    ImGui_ImplDX11_Init(mD3DDevice, mD3DDeviceContext);
  }


  DX11Renderer::~DX11Renderer()
  {
    ImGui_ImplDX11_Shutdown();
    CleanupDeviceD3D();
  }

  void DX11Renderer::NewFrame()
  {
    ImGui_ImplDX11_NewFrame();
  }

  void DX11Renderer::ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight)
  {
    CleanupRenderTarget();
    mSwapChain->ResizeBuffers(0, (UINT)aWidth, (UINT)aHeight, DXGI_FORMAT_UNKNOWN, 0);
    CreateRenderTarget();
  }

  void DX11Renderer::ClearRenderTarget(glm::vec4 aClearColor)
  {
    mD3DDeviceContext->OMSetRenderTargets(1, &mMainRenderTargetView, NULL);
    mD3DDeviceContext->ClearRenderTargetView(mMainRenderTargetView, &(aClearColor.x));
  }

  void DX11Renderer::RenderImguiData()
  {
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  }

  void DX11Renderer::Present()
  {
    mSwapChain->Present(1, 0); // Present with vsync
  }
}