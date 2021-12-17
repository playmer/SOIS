#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h> // For CommandLineToArgvW

// The min/max macros conflict with like-named member functions.
// Only use std::min and std::max defined in <algorithm>.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

// In order to define a function called CreateWindow, the Windows macro needs to
// be undefined.
#if defined(CreateWindow)
#undef CreateWindow
#endif

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using namespace Microsoft::WRL;

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// D3D12 extension library.
#include "directx/d3dx12.h"

#include "../Renderer.hpp"

#include <optional>
#include <queue>
#include <mutex>

// From DXSampleHelper.h 
// Source: https://github.com/Microsoft/DirectX-Graphics-Samples
inline void ThrowIfFailed(HRESULT hr)
{
  if (FAILED(hr))
  {
    throw std::exception();
  }
}


class Dx12Renderer;
class Dx12Texture;

struct Dx12UBOData
{
  Microsoft::WRL::ComPtr<ID3D12Resource> mBuffer;
  Dx12Renderer* mRenderer;
  D3D12_RESOURCE_STATES mState;
};


struct Dx12UBOUpdates
{
  Dx12UBOUpdates(Dx12Renderer* aRenderer)
    : mRenderer{ aRenderer }
  {

  }

  struct Dx12UBOReference
  {
    Dx12UBOReference(Microsoft::WRL::ComPtr<ID3D12Resource> const& aBuffer,
      size_t aBufferOffset,
      size_t aSize,
      D3D12_RESOURCE_STATES aState);

    Microsoft::WRL::ComPtr<ID3D12Resource> mBuffer;
    size_t mBufferOffset;
    size_t mSize;
    D3D12_RESOURCE_STATES mState;
  };

  void Add(Dx12UBOData const& aBuffer, uint8_t const* aData, size_t aSize, size_t aOffset);

  template <typename tType>
  void Add(Dx12UBOData const& aBuffer, tType const& aData, size_t aNumber = 1, size_t aOffset = 0)
  {
    Add(aBuffer, reinterpret_cast<u8 const*>(&aData), sizeof(tType) * aNumber, aOffset);
  }

  void Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>& aBuffer);

  std::vector<uint8_t> mData;
  std::mutex mAddingMutex;
  std::vector<Dx12UBOReference> mReferences;
  Microsoft::WRL::ComPtr<ID3D12Resource> mMappingBuffer;
  std::vector<CD3DX12_RESOURCE_BARRIER> mTransitionBarriers;
  size_t mMappingBufferSize;

  Dx12Renderer* mRenderer;
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
  using CommandListQueue = std::queue< Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> >;

  D3D12_COMMAND_LIST_TYPE                     mCommandListType;
  Microsoft::WRL::ComPtr<ID3D12Device2>       mDevice;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue>  mQueue;
  Microsoft::WRL::ComPtr<ID3D12Fence>         mFence;
  HANDLE                                      mFenceEvent;
  uint64_t                                    mFenceValue;

  CommandAllocatorQueue                       mCommandAllocatorQueue;
  CommandListQueue                            mCommandListQueue;
};

class Dx12Renderer : public Renderer
{
public:
  Dx12Renderer();
  ~Dx12Renderer();

  Texture* CreateTexture(std::string const& aFilename) override;
  GPUAllocator* MakeAllocator(std::string const& aAllocatorType, size_t aBlockSize) override;

  void Initialize(SDL_Window* aWindow) override;
  SDL_WindowFlags GetAdditionalWindowFlags() override;
  void NewFrame() override;
  void RenderImguiData() override;

  void Update() override;
  void RenderAndPresent() override;
  void ResizeRenderTarget(unsigned aWidth, unsigned aHeight) override;
  void SetFullscreen(bool aFullscreen) override;

  bool UpdateWindow() override;

  Dx12UBOUpdates mUBOUpdates;

public:
  // Window handle.
  HWND mWindowHandle;
  // Window rectangle (used to toggle fullscreen state).
  RECT mWindowRect;

  // DirectX 12 Objects
  ComPtr<ID3D12Device2> mDevice;
  std::optional<Dx12Queue> mGraphicsQueue;
  std::optional<Dx12Queue> mComputeQueue;
  std::optional<Dx12Queue> mTransferQueue;
  ComPtr<IDXGISwapChain4> mSwapChain;
  ComPtr<ID3D12Resource> mBackBuffers[gNumFrames];
  //ComPtr<ID3D12GraphicsCommandList> mCommandList;
  ComPtr<ID3D12CommandAllocator> mCommandAllocators[gNumFrames];
  ComPtr<ID3D12DescriptorHeap> mRTVDescriptorHeap;
  UINT mRTVDescriptorSize;
  UINT mCurrentBackBufferIndex;
  
  // Synchronization objects
  //ComPtr<ID3D12Fence> g_Fence;
  //uint64_t g_FenceValue = 0;
  uint64_t mFrameFenceValues[gNumFrames] = {};
  //HANDLE g_FenceEvent;


  bool mUseWarp = false;

  uint32_t mClientWidth = 1280;
  uint32_t mClientHeight = 720;

  // By default, enable V-Sync.
  // Can be toggled with the V key.
  bool mVSync = true;
  bool mTearingSupported = false;
  // By default, use windowed mode.
  // Can be toggled with the Alt+Enter or F11
  bool mFullscreen = false;
};


class Dx12UBO : public GPUBufferBase
{
public:
  Dx12UBO(size_t aSize)
    : GPUBufferBase{ aSize }
  {

  }

  void Update(uint8_t const* aPointer, size_t aBytes, size_t aOffset) override
  {
    auto self = mData.Get<Dx12UBOData>();

    self->mRenderer->mUBOUpdates.Add(*self, aPointer, aBytes, aOffset);
  }

  Microsoft::WRL::ComPtr<ID3D12Resource>& GetBuffer()
  {
    auto self = mData.Get<Dx12UBOData>();

    return self->mBuffer;
  }
};

class Dx12GPUAllocator : public GPUAllocator
{
public:
  Dx12GPUAllocator(std::string const& aAllocatorType, size_t aBlockSize, Dx12Renderer* aRenderer);
  std::unique_ptr<GPUBufferBase> CreateBufferInternal(
    size_t aSize,
    GPUAllocation::BufferUsage aUse,
    GPUAllocation::MemoryProperty aProperties) override;
};
