#pragma once

#define NOMINMAX
#include <winrt/base.h>
#include "directx/d3d12.h"
#include "directx/d3dx12.h"
#include <dxgi1_4.h>
#include <dxgi1_6.h>
#include <dxgiformat.h>

#include <wrl.h>

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


  struct DX12UBOUpdates
  {
    DX12UBOUpdates(DX12Renderer* aRenderer)
      : mRenderer{ aRenderer }
    {

    }

    struct DX12UBOReference
    {
      DX12UBOReference(Microsoft::WRL::ComPtr<ID3D12Resource> const& aBuffer,
        size_t aBufferOffset,
        size_t aSize,
        D3D12_RESOURCE_STATES aState);

      Microsoft::WRL::ComPtr<ID3D12Resource> mBuffer;
      size_t mBufferOffset;
      size_t mSize;
      D3D12_RESOURCE_STATES mState;
    };

    void Add(DX12UBOData const& aBuffer, uint8_t const* aData, size_t aSize, size_t aOffset);

    template <typename tType>
    void Add(DX12UBOData const& aBuffer, tType const& aData, size_t aNumber = 1, size_t aOffset = 0)
    {
      Add(aBuffer, reinterpret_cast<u8 const*>(&aData), sizeof(tType) * aNumber, aOffset);
    }

    void Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>& aBuffer);

    std::vector<uint8_t> mData;
    std::mutex mAddingMutex;
    std::vector<DX12UBOReference> mReferences;
    Microsoft::WRL::ComPtr<ID3D12Resource> mMappingBuffer;
    std::vector<CD3DX12_RESOURCE_BARRIER> mTransitionBarriers;
    size_t mMappingBufferSize;

    DX12Renderer* mRenderer;
  };


  class Dx12Queue
  {
  public:
    Dx12Queue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
    virtual ~Dx12Queue();

    // Get an available command list from the command queue.
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetCommandList();

    // Execute a command list.
    // Returns the fence value to wait for for this command list.
    uint64_t ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList);

    uint64_t Signal();
    bool IsFenceComplete(uint64_t fenceValue);
    void WaitForFenceValue(uint64_t fenceValue);
    void Flush();

    auto operator->() const
    {
      return mQueue.Get();
    }

    auto& operator&()
    {
      return mQueue;
    }

    auto& operator&() const
    {
      return mQueue;
    }

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const
    {
      return mQueue;
    }

  protected:
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator);

  private:
    // Keep track of command allocators that are "in-flight"
    struct CommandAllocatorEntry
    {
      uint64_t fenceValue;
      Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
    };

    using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
    using CommandListQueue = std::queue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> >;

    D3D12_COMMAND_LIST_TYPE                     mCommandListType;
    Microsoft::WRL::ComPtr<ID3D12Device2>       mDevice;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>  mQueue;
    Microsoft::WRL::ComPtr<ID3D12Fence>         mFence;
    HANDLE                                      mFenceEvent;
    uint64_t                                    mFenceValue;

    CommandAllocatorQueue                       mCommandAllocatorQueue;
    CommandListQueue                            mCommandListQueue;
  };



  class DX12Renderer : public Renderer
  {
  public:
    DX12Renderer();
    ~DX12Renderer() override;

    void Initialize(SDL_Window* aWindow) override;

    void NewFrame() override;
    void ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight) override;

    //void PreImguiImage(Texture* aTexture) override;
    //void PostImguiImage(Texture* aTexture) override;

    void ClearRenderTarget(glm::vec4 aClearColor) override;
    void RenderImguiData() override;
    void Present() override;


    // Ostensibly private
    bool CreateDeviceD3D(HWND hWnd);
    void CreateRenderTarget();
    void CleanupDeviceD3D();
    void CleanupRenderTarget();
    void WaitForLastSubmittedFrame();
    FrameContext* WaitForNextFrameResources();

//    void CopyTextureSubresource(const std::shared_ptr<DX12Texture>& texture, uint32_t firstSubresource, uint32_t numSubresources, D3D12_SUBRESOURCE_DATA* subresourceData);

    std::unique_ptr<Texture> LoadTextureFromData(unsigned char* data, TextureLayout format, int w, int h, int pitch) override;
    GPUAllocator* MakeAllocator(std::string const& aAllocatorType, size_t aBlockSize) override;

    //Microsoft::WRL::ComPtr<ID3D12Device> mD3DDevice = nullptr;
    //Microsoft::WRL::ComPtr<ID3D12DeviceContext> mD3DDeviceContext = nullptr;
    //Microsoft::WRL::ComPtr<IDXGISwapChain3> mSwapChain = nullptr;
    //ID3D12RenderTargetView* mMainRenderTargetView = nullptr;
    SDL_Window* mWindow = nullptr;


    //static constexpr int cNumBackBuffers = 3;
    static constexpr int cNumFramesInFlight = 3;

    //std::array<ID3D12Resource*, cNumBackBuffers> mMainRenderTargetResource = {};
    //std::array<D3D12_CPU_DESCRIPTOR_HANDLE, cNumBackBuffers> mMainRenderTargetDescriptor = {};
    //std::array<FrameContext, cNumFramesInFlight> mFrameContexts = {};
    //
    //ID3D12DescriptorHeap* mRtvDescHeap = nullptr;
    //ID3D12DescriptorHeap* mSrvDescHeap = nullptr;
    //ID3D12CommandQueue* mCommandQueue = nullptr;
    //ID3D12GraphicsCommandList* mCommandList = nullptr;
    //FrameContext* mCurrentFrameContext = nullptr;
    //
    //
    //ID3D12Fence* mFence = nullptr;
    //HANDLE mFenceEvent = nullptr;
    //UINT64 mFenceLastSignaledValue = 0;
    //HANDLE mSwapChainWaitableObject = nullptr;
    //
    //UINT mFrameIndex = 0;
    //int mBackBufferIdx = 0;









    DX12UBOUpdates mUBOUpdates;


    Microsoft::WRL::ComPtr<ID3D12Device2> mDevice;
    std::optional<Dx12Queue> mGraphicsQueue;
    std::optional<Dx12Queue> mComputeQueue;
    std::optional<Dx12Queue> mTransferQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> mSwapChain;
    Microsoft::WRL::ComPtr<ID3D12Resource> mBackBuffers[cNumFramesInFlight];
    //Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandAllocators[cNumFramesInFlight];
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRTVDescriptorHeap;

    Microsoft::WRL::ComPtr<ID3D12Resource> mRenderingBackBuffer;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> mRenderingCommandBuffer;

    uint64_t mUploadCommandBufferFenceValue;

    std::array<uint64_t, cNumFramesInFlight> mFrameFenceValues = {};
    UINT mRTVDescriptorSize;
    UINT mCurrentBackBufferIndex;

    uint32_t mClientWidth = 1280;
    uint32_t mClientHeight = 720;

    bool mVSync = true;
    bool mTearingSupported = false;
    bool mFullscreen = false;


    // imgui
    //Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvDescHeap = nullptr;
    ID3D12DescriptorHeap* mSrvDescHeap = nullptr;
    std::vector<ID3D12DescriptorHeap*> mHeaps;
  };

  class DX12UBO : public GPUBufferBase
  {
  public:
    DX12UBO(size_t aSize)
      : GPUBufferBase{ aSize }
    {

    }

    void Update(uint8_t const* aPointer, size_t aBytes, size_t aOffset) override
    {
      auto self = mData.Get<DX12UBOData>();

      self->mRenderer->mUBOUpdates.Add(*self, aPointer, aBytes, aOffset);
    }

    Microsoft::WRL::ComPtr<ID3D12Resource>& GetBuffer()
    {
      auto self = mData.Get<DX12UBOData>();

      return self->mBuffer;
    }
  };

  class DX12GPUAllocator : public GPUAllocator
  {
  public:
    DX12GPUAllocator(std::string const& aAllocatorType, size_t aBlockSize, DX12Renderer* aRenderer);
    std::unique_ptr<GPUBufferBase> CreateBufferInternal(
      size_t aSize,
      GPUAllocation::BufferUsage aUse,
      GPUAllocation::MemoryProperty aProperties) override;
  };
}
