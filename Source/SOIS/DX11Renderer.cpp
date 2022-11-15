#include <SDL_syswm.h>

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_dx11.h"

#include <stb_image.h>

#include "SOIS/DX11Renderer.hpp"

namespace SOIS
{
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

  class DX11GPUBuffer : public GPUBufferBase
  {

  };


  struct DX11GPUPiplineData
  {
    Microsoft::WRL::ComPtr<ID3D11InputLayout> mInputLayout = nullptr;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> mVertexShader = nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> mPixelShader = nullptr;
  };

  struct DX11GPUBufferData
  {
    Microsoft::WRL::ComPtr<ID3D11Buffer> mVertexBuffer = nullptr;
  };

  class DX11Texture : public Texture
  {
  public:
    DX11Texture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> aShaderResourceView, int aWidth, int aHeight)
      : Texture{ aWidth, aHeight }
      , ShaderResourceView{ aShaderResourceView }
    {

    }

    ~DX11Texture() override
    {
    }

    virtual ImTextureID GetTextureId()
    {
      return ImTextureID{ ShaderResourceView.Get() };
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ShaderResourceView;
  };

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
    if (D3D11CreateDeviceAndSwapChain(
      NULL, 
      D3D_DRIVER_TYPE_HARDWARE, 
      NULL, 
      createDeviceFlags, 
      featureLevelArray, 
      2, 
      D3D11_SDK_VERSION, 
      &sd, 
      &mSwapChain, 
      &mDevice, 
      &featureLevel, 
      &mD3DDeviceContext) != S_OK)
    {
      throw "Bad device and swapchain";
      return false;
    }

    if (mDevice->CreateDeferredContext(0, mDeferredContext.GetAddressOf()) != S_OK)
    {
      throw "Bad deferred context and swapchain";
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
    mDevice = nullptr;
  }

  void DX11Renderer::CreateRenderTarget()
  {
    ID3D11Texture2D* pBackBuffer;
    mSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    mDevice->CreateRenderTargetView(pBackBuffer, NULL, &mMainRenderTargetView);
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


  void DX11Renderer::Initialize(SDL_Window* aWindow, char8_t const* /*aPreferredGpu*/)
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
    ImGui_ImplDX11_Init(mDevice.Get(), mD3DDeviceContext.Get());


    mUploadThread = std::thread([this]()
      {
        UploadThread();
      });
  }

  std::optional<DX11Renderer::Job> DX11Renderer::JobVisitor::operator()(CreateVertexBufferJob& aJob)
  {
    Microsoft::WRL::ComPtr<ID3D11Buffer> mVertexBuffer = nullptr;
    D3D11_BUFFER_DESC vertex_buff_descr = {};
    vertex_buff_descr.ByteWidth = aJob.mData.size();
    vertex_buff_descr.Usage = D3D11_USAGE_DEFAULT;
    vertex_buff_descr.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sr_data = { 0 };
    sr_data.pSysMem = aJob.mData.data();
    HRESULT result = mRenderer->mDevice->CreateBuffer(
      &vertex_buff_descr,
      &sr_data,
      mVertexBuffer.GetAddressOf());

    if (FAILED(result))
    {
      printf("Vertex Buffer failed to be created");
      return std::nullopt;
    }

    //aJob.mBufferPromise.set_value()
    return std::nullopt;
  }

  std::optional<DX11Renderer::Job> DX11Renderer::JobVisitor::operator()(UpdateVertexBufferJob& aJob)
  {
    D3D11_MAPPED_SUBRESOURCE resource;
    mRenderer->mDeferredContext->Map(aJob.mBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    memcpy(resource.pData, aJob.mData.data(), aJob.mData.size());
    mRenderer->mDeferredContext->Unmap(aJob.mBuffer.Get(), 0);
    return std::nullopt;
  }

  std::optional<DX11Renderer::Job> DX11Renderer::JobVisitor::operator()(UploadTextureJob& aJob)
  {
    auto format = FromSOIS(aJob.mFormat);
    // Create texture
    D3D11_TEXTURE2D_DESC description;
    ZeroMemory(&description, sizeof(description));
    description.Width = aJob.mWidth;
    description.Height = aJob.mHeight;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = format;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    description.CPUAccessFlags = 0;

    ID3D11Texture2D* pTexture = nullptr;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = aJob.mStoredTextureData.data();
    subResource.SysMemPitch = aJob.mPitch;
    subResource.SysMemSlicePitch = 0;
    if (FAILED(mRenderer->mDevice->CreateTexture2D(&description, &subResource, &pTexture)))
    {
      return std::nullopt;
    }
    
    // Create texture view
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = description.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    mRenderer->mDevice->CreateShaderResourceView(pTexture, &srvDesc, &shaderResourceView);
    pTexture->Release();


    return UploadedTextureJob{
      std::move(aJob.mTexturePromise),
      shaderResourceView,
      aJob.mWidth,
      aJob.mHeight
    };
  }

  std::optional<DX11Renderer::Job> DX11Renderer::JobVisitor::operator()(UploadedTextureJob& aJob)
  {
    auto texture = std::make_unique<DX11Texture>(aJob.mShaderResourceView, aJob.mWidth, aJob.mHeight);
    aJob.mTexturePromise.set_value(std::unique_ptr<Texture>(texture.release()));
    return std::nullopt;
  }

  void DX11Renderer::UploadThread()
  {
    std::vector<Job> jobs;

    while (!mShouldJoin)
    {
      // Aquire Job
      mUploadJobsMutex.lock();
      std::swap(jobs, mUploadJobs);
      mUploadJobsMutex.unlock();

      for (auto jobIt = jobs.begin(); jobIt < jobs.end(); ++jobIt)
      {
        if (auto newJob = std::visit(JobVisitor{ this }, *jobIt); newJob.has_value())
        {
          auto index = jobIt - jobs.begin();
          jobs.emplace_back(std::move(newJob.value()));
          jobIt = index + jobs.begin();
        }
      }

      jobs.clear();

      mUploadJobsWakeUp.acquire();
    }
  }



  DX11Renderer::~DX11Renderer()
  {
    mShouldJoin = true;
    mUploadJobsWakeUp.release();
    mUploadThread.join();

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

  std::future<std::unique_ptr<Texture>> DX11Renderer::LoadTextureFromDataAsync(unsigned char* data, TextureLayout format, int aWidth, int aHeight, int aPitch)
  {
    std::vector<unsigned char> storedData;
    storedData.resize(aHeight * aPitch);
    memcpy(storedData.data(), data, storedData.size());

    std::promise<std::unique_ptr<SOIS::Texture>> texturePromise;
    std::future<std::unique_ptr<SOIS::Texture>> textureFuture = texturePromise.get_future();

    {
      std::unique_lock lock{ mUploadJobsMutex };
      mUploadJobs.emplace_back(UploadTextureJob{
          std::move(texturePromise),
          std::move(storedData),
          format,
          aWidth,
          aHeight,
          aPitch
        }
      );
    }
    mUploadJobsWakeUp.release(1);

    return textureFuture;
  }

  void DX11Renderer::CommandVisitor::operator()(RenderStateCommand& aJob)
  {
    mRenderer->mD3DDeviceContext->RSSetViewports(1, &viewport);
    mRenderer->mD3DDeviceContext->OMSetRenderTargets(1, &mMainRenderTargetView, nullptr);
    mRenderer->mD3DDeviceContext->ClearRenderTargetView(mMainRenderTargetView, color.data());
  }
  void DX11Renderer::CommandVisitor::operator()(BindVertexBufferCommand & aJob)
  {
    mRenderer->mD3DDeviceContext->IASetVertexBuffers(
      0,
      1,
      mVertexBuffer.GetAddressOf(),
      &cVertexStride,
      &cVertexOffset);
  }
  void DX11Renderer::CommandVisitor::operator()(BindIndexBufferCommand & aJob)
  {
    mRenderer->mD3DDeviceContext->IASetIndexBuffer();
  }
  void DX11Renderer::CommandVisitor::operator()(BindPipelineCommand & aJob)
  {
    auto pipelineData = GetDataFromGPUObject(aJob.mPipeline).Get<DX11GPUPiplineData>();

    mRenderer->mD3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mRenderer->mD3DDeviceContext->IASetInputLayout(pipelineData->mInputLayout.Get());
    mRenderer->mD3DDeviceContext->VSSetShader(pipelineData->mVertexShader.Get(), nullptr, 0);
    mRenderer->mD3DDeviceContext->PSSetShader(pipelineData->mPixelShader.Get(), nullptr, 0);
  }
  void DX11Renderer::CommandVisitor::operator()(DrawCommand& aJob)
  {
    mRenderer->mD3DDeviceContext->DrawIndexed(6, 0, 0);
  }

  void DX11Renderer::ExecuteCommandList(GPUCommandList& aList)
  {
    CommandVisitor visitor{ this };
    for (auto& command : GetCommandsFromList(aList))
    {
      std::visit(visitor, command);
    }
  }
}
