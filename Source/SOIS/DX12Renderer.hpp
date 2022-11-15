#pragma once

#define NOMINMAX
#include "directx/d3d12.h"
#include "directx/d3dx12.h"
#include <dxgi1_4.h>
#include <dxgi1_6.h>
#include <dxgiformat.h>

#include <wrl.h>

#include <array>
#include <mutex>
#include <queue>
#include <string>

#include <SDL.h>

#include "SOIS/Renderer.hpp"

namespace SOIS
{
  struct FrameContext
  {
    ID3D12CommandAllocator* CommandAllocator;
    UINT64                  FenceValue;
  };

  class DX12Texture;
  class DX12Texture;


  struct DX12UBOData
  {
    Microsoft::WRL::ComPtr<ID3D12Resource> mBuffer;
    DX12Renderer* mRenderer;
    D3D12_RESOURCE_STATES mState;
  };


  //struct DX12UBOUpdates
  //{
  //  DX12UBOUpdates(DX12Renderer* aRenderer)
  //    : mRenderer{ aRenderer }
  //  {
  //
  //  }
  //
  //  struct DX12UBOReference
  //  {
  //    DX12UBOReference(Microsoft::WRL::ComPtr<ID3D12Resource> const& aBuffer,
  //      size_t aBufferOffset,
  //      size_t aSize,
  //      D3D12_RESOURCE_STATES aState);
  //
  //    Microsoft::WRL::ComPtr<ID3D12Resource> mBuffer;
  //    size_t mBufferOffset;
  //    size_t mSize;
  //    D3D12_RESOURCE_STATES mState;
  //  };
  //
  //  void Add(DX12UBOData const& aBuffer, uint8_t const* aData, size_t aSize, size_t aOffset);
  //
  //  template <typename tType>
  //  void Add(DX12UBOData const& aBuffer, tType const& aData, size_t aNumber = 1, size_t aOffset = 0)
  //  {
  //    Add(aBuffer, reinterpret_cast<u8 const*>(&aData), sizeof(tType) * aNumber, aOffset);
  //  }
  //
  //  void Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>& aBuffer);
  //
  //  std::vector<uint8_t> mData;
  //  std::mutex mAddingMutex;
  //  std::vector<DX12UBOReference> mReferences;
  //  Microsoft::WRL::ComPtr<ID3D12Resource> mMappingBuffer;
  //  std::vector<CD3DX12_RESOURCE_BARRIER> mTransitionBarriers;
  //  size_t mMappingBufferSize;
  //
  //  DX12Renderer* mRenderer;
  //};

  class Dx12Queue;
  struct DX12CommandBuffer
  {
    ID3D12GraphicsCommandList2* operator->() const;
    ID3D12GraphicsCommandList2& operator&();
    ID3D12GraphicsCommandList2* operator&() const;
    operator ID3D12GraphicsCommandList2* ();

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>& Buffer() const;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>& Buffer();
    Microsoft::WRL::ComPtr<ID3D12Fence>& Fence();
    HANDLE& FenceEvent();
    uint64_t& FenceValue();

    void Wait();
    void ExecuteOnQueue();

    Dx12Queue* mQueue;
    size_t mIndex;
  };


  class Dx12Queue
  {
  public:

    Dx12Queue();

    void Initialize(Microsoft::WRL::ComPtr<ID3D12Device2> aDevice, D3D12_COMMAND_LIST_TYPE aType, size_t aNumberOfBuffers);

    DX12CommandBuffer WaitOnNextCommandList();
    DX12CommandBuffer GetNextCommandList();
    DX12CommandBuffer GetCurrentCommandList();

    auto* operator&()
    {
      return mQueue.Get();
    }

    auto* operator&() const
    {
      return mQueue.Get();
    }

    operator ID3D12CommandQueue*()
    {
      return mQueue.Get();
    }

    bool IsInitialized()
    {
      return (nullptr != mPool)
        && (nullptr != mCommandBuffers.back())
        && (nullptr != mFences.back())
        && (NULL != mFenceEvents.back());
        //&& (nullptr != mFinishedSemaphore.back());
    }

    void Flush();

  private:
    friend DX12CommandBuffer;

    Microsoft::WRL::ComPtr<ID3D12Device2> mDevice;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mPool;
    std::vector<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>> mCommandBuffers;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Fence>> mFences;
    std::vector<HANDLE> mFenceEvents;
    std::vector<uint64_t> mFenceValues;

    std::vector<bool> mUsed; // lmao vector bool
    size_t mCurrentBuffer = 0;
    D3D12_COMMAND_LIST_TYPE mType;
  };



  class DX12Renderer : public Renderer
  {
  public:
    DX12Renderer();
    ~DX12Renderer() override;

    void Initialize(SDL_Window* aWindow, char8_t const* /*aPreferredGpu*/) override;

    void NewFrame() override;
    void ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight) override;

    //void PreImguiImage(Texture* aTexture) override;
    //void PostImguiImage(Texture* aTexture) override;

    void ClearRenderTarget(glm::vec4 aClearColor) override;
    void RenderImguiData() override;
    void Present() override;
    void Upload() override;


    // Ostensibly private
    bool CreateDeviceD3D(HWND hWnd);
    void CreateRenderTarget();
    void CleanupDeviceD3D();
    void CleanupRenderTarget();
    void WaitForLastSubmittedFrame();
    FrameContext* WaitForNextFrameResources();

//    void CopyTextureSubresource(const std::shared_ptr<DX12Texture>& texture, uint32_t firstSubresource, uint32_t numSubresources, D3D12_SUBRESOURCE_DATA* subresourceData);

    std::future<std::unique_ptr<Texture>> LoadTextureFromDataAsync(unsigned char* data, TextureLayout format, int w, int h, int pitch) override;
    void UploadThread();
    struct UploadJob
    {
      struct Texture
      {
        std::promise<std::unique_ptr<SOIS::Texture>> mTexturePromise;
        Microsoft::WRL::ComPtr<ID3D12Resource> mTexture;
        Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
        D3D12_RESOURCE_DESC mDescription;
        size_t mWidth;
        size_t mHeight;
        size_t mUploadPitch;
      };

      struct TextureTransition
      {
        std::promise<std::unique_ptr<SOIS::Texture>> mTexturePromise;
        Microsoft::WRL::ComPtr<ID3D12Resource> mTexture;
        Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap;
        D3D12_GPU_DESCRIPTOR_HANDLE mGpuHandle;
        size_t mWidth;
        size_t mHeight;
        size_t mUploadPitch;
      };

      struct Buffer
      {
        DX12Renderer* mRenderer;
        Microsoft::WRL::ComPtr<ID3D12Resource> mBuffer;
        D3D12_RESOURCE_STATES mState;
      };

      UploadJob(DX12Renderer* aRenderer, Texture aTexture);
      UploadJob(DX12Renderer* aRenderer, TextureTransition aTextureTransition);
      UploadJob(DX12Renderer* aRenderer, Buffer);

      UploadJob(UploadJob&&) = default;
      UploadJob(UploadJob&) = default;

      std::optional<UploadJob> operator()(DX12CommandBuffer aCommandBuffer);
      void FulfillPromise();

      struct UploadValues
      {
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap;
        D3D12_GPU_DESCRIPTOR_HANDLE mGpuHandle;
      };

      UploadValues TextureUpload(DX12CommandBuffer aCommandBuffer, Texture* aTexture);
      void BufferUpload(DX12CommandBuffer aCommandBuffer, Buffer* aBuffer);
      void TextureTransitionTask(DX12CommandBuffer aCommandBuffer, TextureTransition * aTextureTransition);

      DX12Renderer* mRenderer;
      std::variant<Texture, TextureTransition, Buffer> mVariant;
    };
    std::vector<UploadJob> mUploadJobs;


    //GPUAllocator* MakeAllocator(std::string const& aAllocatorType, size_t aBlockSize) override;

    SDL_Window* mWindow = nullptr;

    static constexpr int cNumFramesInFlight = 3;

    void TransitionTextures();
    
    struct TextureTransferData
    {
      size_t mWidth;
      size_t mHeight;
      size_t mUploadPitch;
      Microsoft::WRL::ComPtr<ID3D12Resource> mTexture;
      Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
    };
    
    std::vector<TextureTransferData> mTexturesCreatedThisFrame;
    

    //DX12UBOUpdates mUBOUpdates;


    Microsoft::WRL::ComPtr<ID3D12Device2> mDevice;

    Dx12Queue mTransferQueue;
    Dx12Queue mTextureTransitionQueue;
    Dx12Queue mGraphicsQueue;
    Dx12Queue mComputeQueue;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> mSwapChain;
    Microsoft::WRL::ComPtr<ID3D12Resource> mBackBuffers[cNumFramesInFlight];
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandAllocators[cNumFramesInFlight];
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRTVDescriptorHeap;

    UINT mRTVDescriptorSize;
    UINT mCurrentBackBufferIndex;

    uint32_t mClientWidth = 1280;
    uint32_t mClientHeight = 720;

    bool mVSync = true;
    bool mTearingSupported = false;
    bool mFullscreen = false;


    // imgui
    ID3D12DescriptorHeap* mSrvDescHeap = nullptr;
    std::vector<ID3D12DescriptorHeap*> mHeaps;
  };

  //class DX12UBO : public GPUBufferBase
  //{
  //public:
  //  DX12UBO(size_t aSize)
  //    : GPUBufferBase{ aSize }
  //  {
  //
  //  }
  //
  //  void Update(uint8_t const* aPointer, size_t aBytes, size_t aOffset) override
  //  {
  //    auto self = mData.Get<DX12UBOData>();
  //
  //    self->mRenderer->mUBOUpdates.Add(*self, aPointer, aBytes, aOffset);
  //  }
  //
  //  Microsoft::WRL::ComPtr<ID3D12Resource>& GetBuffer()
  //  {
  //    auto self = mData.Get<DX12UBOData>();
  //
  //    return self->mBuffer;
  //  }
  //};

  //class DX12GPUAllocator : public GPUAllocator
  //{
  //public:
  //  DX12GPUAllocator(std::string const& aAllocatorType, size_t aBlockSize, DX12Renderer* aRenderer);
  //  std::unique_ptr<GPUBufferBase> CreateBufferInternal(
  //    size_t aSize,
  //    GPUAllocation::BufferUsage aUse,
  //    GPUAllocation::MemoryProperty aProperties) override;
  //};
}
