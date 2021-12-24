#include <SDL_syswm.h>

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_dx11.h"

#include <stb_image.h>

#include "SOIS/DX11Renderer.hpp"

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
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; //DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;

    // If the project is in a debug build, enable the debug layer.
#if !defined(NDEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, mSwapChain.put(), mD3DDevice.put(), &featureLevel, mD3DDeviceContext.put()) != S_OK)
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
    mSwapChain = nullptr;
    mD3DDeviceContext = nullptr;
    mD3DDevice = nullptr;
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
    ImGui_ImplDX11_Init(mD3DDevice.get(), mD3DDeviceContext.get());
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

  class DX11Texture : public Texture
  {
  public:
    DX11Texture(winrt::com_ptr<ID3D11ShaderResourceView> aShaderResourceView, int aWidth, int aHeight)
      : Texture{ aWidth, aHeight }
      , ShaderResourceView{ aShaderResourceView }
    {

    }

    ~DX11Texture() override
    {
    }

    virtual void* GetTextureId()
    {
      return ShaderResourceView.get();
    }

    winrt::com_ptr<ID3D11ShaderResourceView> ShaderResourceView;
  };

  static DXGI_FORMAT FromSOIS(TextureLayout aLayout)
  {
    switch (aLayout)
    {
    case TextureLayout::RGBA_Unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureLayout::RGBA_Srgb: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case TextureLayout::Bc1_Rgba_Unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureLayout::Bc1_Rgba_Srgb: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case TextureLayout::Bc3_Srgb: return DXGI_FORMAT_BC3_UNORM_SRGB;
    case TextureLayout::Bc3_Unorm: return DXGI_FORMAT_BC3_UNORM;
    case TextureLayout::Bc7_Unorm: return DXGI_FORMAT_BC7_UNORM;
    case TextureLayout::Bc7_Srgb: return DXGI_FORMAT_BC7_UNORM_SRGB;
    case TextureLayout::InvalidLayout: return (DXGI_FORMAT)0;
    }
  }

  std::unique_ptr<Texture> DX11Renderer::LoadTextureFromData(unsigned char* data, TextureLayout format, int w, int h, int pitch)
  {
    // Create texture
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = FromSOIS(format);
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D* pTexture = NULL;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = data;
    subResource.SysMemPitch = pitch;
    subResource.SysMemSlicePitch = 0;
    mD3DDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    // Create texture view
    winrt::com_ptr<ID3D11ShaderResourceView> shaderResourceView;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = (DXGI_FORMAT)format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    mD3DDevice->CreateShaderResourceView(pTexture, &srvDesc, shaderResourceView.put());
    pTexture->Release();

    auto texture = std::make_unique<DX11Texture>(shaderResourceView, w, h);

    return std::unique_ptr<Texture>(texture.release());
  }

  std::unique_ptr<Texture> DX11Renderer::LoadTextureFromFile(std::u8string const& aFile)
  {
    // Load from disk into a raw RGBA buffer
    std::vector<char> imageData;
    SDL_RWops* io = SDL_RWFromFile((char const*)aFile.c_str(), "rb");
    if (io != nullptr)
    {
      /* Seek to 0 bytes from the end of the file */
      Sint64 length = SDL_RWseek(io, 0, RW_SEEK_END);
      SDL_RWseek(io, 0, RW_SEEK_SET);
      imageData.resize(length);
      SDL_RWread(io, imageData.data(), length, 1);
      SDL_RWclose(io);
    }

    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load_from_memory((unsigned char*)imageData.data(), imageData.size(), &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
      return nullptr;

    // Create texture
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = image_width;
    desc.Height = image_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D* pTexture = NULL;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = image_data;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    mD3DDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    // Create texture view
    winrt::com_ptr<ID3D11ShaderResourceView> shaderResourceView;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    mD3DDevice->CreateShaderResourceView(pTexture, &srvDesc, shaderResourceView.put());
    pTexture->Release();

    stbi_image_free(image_data);
    auto texture = std::make_unique<DX11Texture>(shaderResourceView, image_width, image_height);

    return std::unique_ptr<Texture>(texture.release());
  }
}
