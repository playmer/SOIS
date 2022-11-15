#pragma once

#define NOMINMAX
#include <d3d11.h>

#include <optional>

#include <wrl.h>

#include <string>

#include <SDL.h> // Include glfw3.h after our OpenGL definitions

#include "SOIS/Renderer.hpp"

namespace SOIS
{
  struct DX11BufferData
  {
  
  };

  class DX11Renderer : public Renderer
  {
  public:
    DX11Renderer();
    ~DX11Renderer() override;

    void Initialize(SDL_Window* aWindow, char8_t const* /*aPreferredGpu*/) override;

    void NewFrame() override;
    void ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight) override;

    void ClearRenderTarget(glm::vec4 aClearColor) override;
    void RenderImguiData() override;
    void Present() override;

    void ExecuteCommandList(GPUCommandList& aList) override;

    void UploadThread();



    // Ostensibly private
    bool CreateDeviceD3D(HWND hWnd);
    void CreateRenderTarget();
    void CleanupDeviceD3D();
    void CleanupRenderTarget();

    std::future<std::unique_ptr<Texture>> LoadTextureFromDataAsync(unsigned char* data, TextureLayout format, int aWidth, int aHeight, int pitch) override;

    struct CreateVertexBufferJob
    {
      std::promise<GPUBufferBase> mBufferPromise;
      std::vector<byte> mData;
    };

    struct UpdateVertexBufferJob
    {
      Microsoft::WRL::ComPtr<ID3D11Buffer> mBuffer;
      std::vector<byte> mData;
    };

    struct UploadTextureJob
    {
      std::promise<std::unique_ptr<SOIS::Texture>> mTexturePromise;
      std::vector<unsigned char> mStoredTextureData;
      TextureLayout mFormat;
      int mWidth;
      int mHeight;
      int mPitch;
    };

    struct UploadedTextureJob
    {
      std::promise<std::unique_ptr<SOIS::Texture>> mTexturePromise;
      Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mShaderResourceView;
      int mWidth;
      int mHeight;
    };


    struct CommandVisitor
    {
      CommandVisitor(DX11Renderer* aRenderer)
        : mRenderer{ aRenderer }
      {

      }

      void operator()(RenderStateCommand& aJob);
      void operator()(BindVertexBufferCommand& aJob);
      void operator()(BindIndexBufferCommand& aJob);
      void operator()(BindPipelineCommand& aJob);
      void operator()(DrawCommand& aJob);

      DX11Renderer* mRenderer;
    };

    using Job = std::variant<CreateVertexBufferJob, UploadTextureJob, UploadedTextureJob>;

    struct JobVisitor
    {
      JobVisitor(DX11Renderer* aRenderer)
        : mRenderer{aRenderer}
      {

      }
      std::optional<Job> operator()(CreateVertexBufferJob& aJob);
      std::optional<Job> operator()(UpdateVertexBufferJob& aJob);
      std::optional<Job> operator()(UploadTextureJob& aJob);
      std::optional<Job> operator()(UploadedTextureJob& aJob);

      DX11Renderer* mRenderer;
    };

    friend struct JobVisitor;

    std::vector<Job> mUploadJobs;

    Microsoft::WRL::ComPtr<ID3D11Device> mDevice = nullptr;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> mD3DDeviceContext = nullptr;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> mDeferredContext = nullptr;
    Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain = nullptr;
    ID3D11RenderTargetView* mMainRenderTargetView = nullptr;
    SDL_Window* mWindow = nullptr;
  };
}
