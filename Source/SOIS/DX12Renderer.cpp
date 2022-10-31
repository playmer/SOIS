#include "directx/d3dx12.h"
#include <dxgi1_6.h>

#include <SDL_syswm.h>

#include "imgui_impl_sdl.h"
#include "imgui_impl_dx12.h"


#include "DX12Renderer.hpp"

using namespace Microsoft::WRL;

namespace SOIS
{
  ///////////////////////////////////////////////////////////////////
  // DX12 Texture
  class DX12Texture : public Texture
  {
  public:
    DX12Texture(
      Microsoft::WRL::ComPtr<ID3D12Resource> aTexture,
      Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> aSrvHeap,
      D3D12_GPU_DESCRIPTOR_HANDLE aGpuHandle,
      int aWidth,
      int aHeight)
      : Texture{ aWidth, aHeight }
      , mTexture{ aTexture }
      , mSrvHeap{ aSrvHeap }
      , mGpuHandle{ aGpuHandle }
    {

    }

    ~DX12Texture() override
    {
    }

    virtual ImTextureID GetTextureId()
    {
      return ImTextureID{ (void*)mGpuHandle.ptr, mSrvHeap.Get()};
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> mTexture;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap;
    D3D12_GPU_DESCRIPTOR_HANDLE mGpuHandle;
  };

  // From DXSampleHelper.h 
  // Source: https://github.com/Microsoft/DirectX-Graphics-Samples
  inline void ThrowIfFailed(HRESULT hr)
  {
    if (FAILED(hr))
    {
      throw std::exception();
    }
  }

  ///////////////////////////////////////////////////////////////////
  // DX12GPUAllocator
  struct DX12GPUAllocatorData
  {
    DX12GPUAllocatorData(DX12Renderer* aRenderer)
      : mRenderer{ aRenderer }
    {

    }

    DX12Renderer* mRenderer;
  };


  DX12GPUAllocator::DX12GPUAllocator(
    std::string const& aAllocatorType,
    size_t aBlockSize,
    DX12Renderer* aRenderer)
    : GPUAllocator{ aBlockSize }
  {
    auto self = mData.ConstructAndGet<DX12GPUAllocatorData>(aRenderer);
  }

  DX12UBOUpdates::DX12UBOReference::DX12UBOReference(Microsoft::WRL::ComPtr<ID3D12Resource> const& aBuffer,
    size_t aBufferOffset,
    size_t aSize,
    D3D12_RESOURCE_STATES aState)
    : mBuffer(aBuffer)
    , mBufferOffset{ aBufferOffset }
    , mSize{ aSize }
    , mState{ aState }
  {

  }

  void DX12UBOUpdates::Add(DX12UBOData const& aBuffer,
    uint8_t const* aData,
    size_t aSize,
    size_t aOffset)
  {
    std::lock_guard<std::mutex> lock(mAddingMutex);

    auto& resource = aBuffer.mBuffer;

    mReferences.emplace_back(resource, aOffset, aSize, aBuffer.mState);
    mData.insert(mData.end(), aData, aData + aSize);

    // Populate the transition buffer now, this saves us from an N operation when doing
    // the buffer copies.
    mTransitionBarriers.emplace_back(CD3DX12_RESOURCE_BARRIER::Transition(
      resource.Get(),
      aBuffer.mState,
      D3D12_RESOURCE_STATE_COPY_DEST));
  }

  void DX12UBOUpdates::Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>& aCommandBuffer)
  {
    std::lock_guard<std::mutex> lock(mAddingMutex);

    auto size = mData.size();

    if (0 == size)
    {
      return;
    }

    // Create a new buffer if we either don't currently have one, or if our current one
    // isn't big enough.
    if ((nullptr == mMappingBuffer) || size < mMappingBufferSize)
    {
      auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
      auto resourceDescription = CD3DX12_RESOURCE_DESC::Buffer(size);
      
      ThrowIfFailed(mRenderer->mDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDescription,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mMappingBuffer)));

      mMappingBufferSize = size;
    }

    // Copy data over to the upload buffer.
    void* pData;
    CD3DX12_RANGE readRange(0, 0); // We don't need to read any data from this buffer.

    ThrowIfFailed(mMappingBuffer->Map(0, &readRange, &pData));
    std::memcpy(pData, mData.data(), size);

    CD3DX12_RANGE writeRange(0, size);
    mMappingBuffer->Unmap(0, &writeRange);

    size_t dataOffset = 0;

    // Transition our buffers to their copy states.
    aCommandBuffer->ResourceBarrier(static_cast<uint32_t>(mTransitionBarriers.size()), mTransitionBarriers.data());

    // Copy from the upload buffer to all the buffers added this frame.
    for (auto const& reference : mReferences)
    {
      aCommandBuffer->CopyBufferRegion(
        reference.mBuffer.Get(),
        reference.mBufferOffset,
        mMappingBuffer.Get(),
        dataOffset,
        reference.mSize);

      dataOffset += reference.mSize;
    }

    // We now know the number of resources we're copying, so we can remove the transitions to 
    // the copy state. Then we can create Transition barriers to move buffers back to their 
    // original resource states 
    mTransitionBarriers.clear();

    for (auto& reference : mReferences)
    {
      mTransitionBarriers.emplace_back(CD3DX12_RESOURCE_BARRIER::Transition(
        reference.mBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        reference.mState));
    }

    // And transition the barriers to their final states
    aCommandBuffer->ResourceBarrier(static_cast<uint32_t>(mTransitionBarriers.size()), mTransitionBarriers.data());

    // Frame cleanup.
    mData.clear();
    mReferences.clear();
    mTransitionBarriers.clear();
  }


  //////////////////////////////////////////////////////////////////////////////////////////////
  // DX12 Helpers:
  ComPtr<IDXGIFactory4> CreateFactory()
  {
    ComPtr<IDXGIFactory4> dxgiFactory4;
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

    return dxgiFactory4;
  }

  ComPtr<IDXGIAdapter4> GetAdapter(ComPtr<IDXGIFactory4> aFactory)
  {
    ComPtr<IDXGIAdapter1> dxgiAdapter1;
    ComPtr<IDXGIAdapter4> dxgiAdapter4;

    SIZE_T maxDedicatedVideoMemory = 0;
    for (UINT i = 0; aFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
    {
      DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
      dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

      // Check to see if the adapter can create a D3D12 device without actually 
      // creating it. The adapter with the largest dedicated video memory
      // is favored.
      if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
        SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
          D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
        dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
      {
        maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
        ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
      }
    }

    return dxgiAdapter4;
  }

  ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter)
  {
    ComPtr<ID3D12Device2> d3d12Device2;
    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));

    // Enable debug messages in debug mode.
#if defined(_DEBUG)
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
    {
      pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
      pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
      pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

      // Suppress whole categories of messages
      //D3D12_MESSAGE_CATEGORY Categories[] = {};

      // Suppress messages based on their severity level
      D3D12_MESSAGE_SEVERITY Severities[] =
      {
          D3D12_MESSAGE_SEVERITY_INFO
      };

      // Suppress individual messages by their ID
      D3D12_MESSAGE_ID DenyIds[] = {
          D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
          D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
          D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
      };

      D3D12_INFO_QUEUE_FILTER NewFilter = {};
      //NewFilter.DenyList.NumCategories = _countof(Categories);
      //NewFilter.DenyList.pCategoryList = Categories;
      NewFilter.DenyList.NumSeverities = _countof(Severities);
      NewFilter.DenyList.pSeverityList = Severities;
      NewFilter.DenyList.NumIDs = _countof(DenyIds);
      NewFilter.DenyList.pIDList = DenyIds;

      ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
    }
#endif

    return d3d12Device2;
  }

  bool CheckTearingSupport()
  {
    BOOL allowTearing = FALSE;

    // Rather than create the DXGI 1.5 factory interface directly, we create the
    // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
    // graphics debugging tools which will not support the 1.5 factory interface 
    // until a future update.
    ComPtr<IDXGIFactory4> factory4;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
    {
      ComPtr<IDXGIFactory5> factory5;
      if (SUCCEEDED(factory4.As(&factory5)))
      {
        if (FAILED(factory5->CheckFeatureSupport(
          DXGI_FEATURE_PRESENT_ALLOW_TEARING,
          &allowTearing, sizeof(allowTearing))))
        {
          allowTearing = FALSE;
        }
      }
    }

    return allowTearing == TRUE;
  }

  ComPtr<IDXGISwapChain4> CreateSwapChain(
    HWND hWnd,
    ComPtr<IDXGIFactory4> aFactory,
    ComPtr<ID3D12CommandQueue> commandQueue,
    uint32_t width,
    uint32_t height,
    uint32_t bufferCount)
  {
    ComPtr<IDXGISwapChain4> dxgiSwapChain4;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc = { 1, 0 };
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = bufferCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    // It is recommended to always allow tearing if tearing support is available.
    swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(aFactory->CreateSwapChainForHwnd(
      commandQueue.Get(),
      hWnd,
      &swapChainDesc,
      nullptr,
      nullptr,
      &swapChain1));

    // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
    // will be handled manually.
    ThrowIfFailed(aFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));

    return dxgiSwapChain4;
  }


  void EnableDebugLayer()
  {
#if defined(_DEBUG)
    // Always enable the debug layer before doing anything DX12 related
    // so all possible errors generated while creating DX12 objects
    // are caught by the debug layer.
    ComPtr<ID3D12Debug> debugInterface;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
#endif
  }

  ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
    ComPtr<ID3D12Device2> device,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    uint32_t numDescriptors)
  {
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = numDescriptors;
    desc.Type = type;

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
  }

  void UpdateRenderTargetViews(
    DX12Renderer* aRenderer,
    ComPtr<ID3D12Device2> device,
    ComPtr<IDXGISwapChain4> swapChain,
    ComPtr<ID3D12DescriptorHeap> descriptorHeap)
  {
    auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < DX12Renderer::cNumFramesInFlight; ++i)
    {
      ComPtr<ID3D12Resource> backBuffer;
      ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

      device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

      aRenderer->mBackBuffers[i] = backBuffer;

      rtvHandle.Offset(rtvDescriptorSize);
    }
  }

  ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device2> device,
    D3D12_COMMAND_LIST_TYPE type)
  {
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));

    return commandAllocator;
  }

  ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12Device2> device,
    ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type)
  {
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ThrowIfFailed(device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

    ThrowIfFailed(commandList->Close());

    return commandList;
  }

  ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> device)
  {
    ComPtr<ID3D12Fence> fence;

    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    return fence;
  }


  uint64_t Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence,
    uint64_t& fenceValue)
  {
    uint64_t fenceValueForSignal = ++fenceValue;
    ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceValueForSignal));

    return fenceValueForSignal;
  }

  void WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent,
    std::chrono::milliseconds duration = std::chrono::milliseconds::max())
  {
    if (fence->GetCompletedValue() < fenceValue)
    {
      ThrowIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
      ::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
    }
  }

  void Flush(
    ComPtr<ID3D12CommandQueue> commandQueue,
    ComPtr<ID3D12Fence> fence,
    uint64_t& fenceValue,
    HANDLE fenceEvent)
  {
    uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
    WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
  }

  HANDLE CreateEventHandle()
  {
    HANDLE fenceEvent;

    fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(fenceEvent && "Failed to create fence event.");

    return fenceEvent;
  }

  //////////////////////////////////////////////////////////////////////////////////////////////
  // DX12CommandBuffer
  ID3D12GraphicsCommandList2* DX12CommandBuffer::operator->() const
  {
    return Buffer().Get();
  }

  ID3D12GraphicsCommandList2& DX12CommandBuffer::operator&()
  {
    return *Buffer().Get();
  }

  ID3D12GraphicsCommandList2* DX12CommandBuffer::operator&() const
  {
    return Buffer().Get();
  }

  DX12CommandBuffer::operator ID3D12GraphicsCommandList2* ()
  {
    return Buffer().Get();
  }

  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>& DX12CommandBuffer::Buffer() const
  {
    return mQueue->mCommandBuffers[mIndex];
  }

  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>& DX12CommandBuffer::Buffer()
  {
    return mQueue->mCommandBuffers[mIndex];
  }

  Microsoft::WRL::ComPtr<ID3D12Fence>& DX12CommandBuffer::Fence()
  {
    return mQueue->mFences[mIndex];
  }

  HANDLE& DX12CommandBuffer::FenceEvent()
  {
    return mQueue->mFenceEvents[mIndex];
  }

  uint64_t& DX12CommandBuffer::FenceValue()
  {
    return mQueue->mFenceValues[mIndex];
  }

  void DX12CommandBuffer::ExecuteOnQueue()
  {
    Buffer()->Close();

    ID3D12CommandList* const ppCommandLists[] = {
        Buffer().Get()
    };

    mQueue->mQueue->ExecuteCommandLists(1, ppCommandLists);
    uint64_t fenceValueForSignal = ++FenceValue();
    ThrowIfFailed(mQueue->mQueue->Signal(Fence().Get(), fenceValueForSignal));
  }

  void DX12CommandBuffer::Wait()
  {
    Fence()->SetEventOnCompletion(FenceValue(), FenceEvent());
    ::WaitForSingleObject(FenceEvent(), DWORD_MAX);
    ::ResetEvent(FenceEvent());
  }

  //////////////////////////////////////////////////////////////////////////////////////////////
  // Dx12Queue
  Dx12Queue::Dx12Queue()
  {
  }

  void Dx12Queue::Initialize(Microsoft::WRL::ComPtr<ID3D12Device2> aDevice, D3D12_COMMAND_LIST_TYPE aType, size_t aNumberOfBuffers)
  {
    mDevice = aDevice;
    mCommandBuffers.resize(aNumberOfBuffers, nullptr);
    mFences.resize(aNumberOfBuffers, nullptr);
    mFenceEvents.resize(aNumberOfBuffers, NULL);
    mFenceValues.resize(aNumberOfBuffers, 0);
    mUsed.resize(aNumberOfBuffers, false);
    mType = aType;


    ///////////////////////////////////////
    // Create Queue
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = aType;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    ThrowIfFailed(mDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&mQueue)));

    ///////////////////////////////////////
    // Create Command Pool
    ThrowIfFailed(mDevice->CreateCommandAllocator(aType, IID_PPV_ARGS(&mPool)));

    ///////////////////////////////////////
    // Create Fences
    for (auto& fence : mFences)
    {
      ThrowIfFailed(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    }

    ///////////////////////////////////////
    // Create Fence Events
    for (auto& fenceEvent : mFenceEvents)
    {
      fenceEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
    }
  }

  DX12CommandBuffer Dx12Queue::WaitOnNextCommandList()
  {
    mCurrentBuffer = (1 + mCurrentBuffer) % mCommandBuffers.size();

    if (nullptr == mCommandBuffers[mCurrentBuffer].Get())
    {
      ThrowIfFailed(mDevice->CreateCommandList(0, mType, mPool.Get(), nullptr, IID_PPV_ARGS(&mCommandBuffers[mCurrentBuffer])));
    }

    if (mUsed[mCurrentBuffer])
    {
      //vkWaitForFences(mDevice.device, 1, &mFences[mCurrentBuffer], true, UINT64_MAX);
      mFences[mCurrentBuffer]->SetEventOnCompletion(mFenceValues[mCurrentBuffer], mFenceEvents[mCurrentBuffer]);
      ::WaitForSingleObject(mFenceEvents[mCurrentBuffer], DWORD_MAX);
      ::ResetEvent(mFenceEvents[mCurrentBuffer]);
      ThrowIfFailed(mCommandBuffers[mCurrentBuffer]->Reset(mPool.Get(), nullptr));
    }
    else
    {
      mUsed[mCurrentBuffer] = true;
    }

    return DX12CommandBuffer{ this, mCurrentBuffer };
  }


  DX12CommandBuffer Dx12Queue::GetNextCommandList()
  {
    mCurrentBuffer = (1 + mCurrentBuffer) % mCommandBuffers.size();

    if (nullptr == mCommandBuffers[mCurrentBuffer].Get())
    {
      ThrowIfFailed(mDevice->CreateCommandList(0, mType, mPool.Get(), nullptr, IID_PPV_ARGS(&mCommandBuffers[mCurrentBuffer])));
    }

    if (mUsed[mCurrentBuffer])
    {
      ThrowIfFailed(mCommandBuffers[mCurrentBuffer]->Reset(mPool.Get(), nullptr));
    }
    else
    {
      mUsed[mCurrentBuffer] = true;
    }

    return DX12CommandBuffer{ this, mCurrentBuffer };
  }


  DX12CommandBuffer Dx12Queue::GetCurrentCommandList()
  {
    return DX12CommandBuffer{ this, mCurrentBuffer };
  }

  void Dx12Queue::Flush()
  {
    for (size_t i = 0; i < mCommandBuffers.size(); ++i)
    {
      if (nullptr == mCommandBuffers[i])
        continue;


      uint64_t fenceValueForSignal = ++mFenceValues[i];
      ThrowIfFailed(mQueue->Signal(mFences[i].Get(), fenceValueForSignal));

      mFences[i]->SetEventOnCompletion(mFenceValues[i], mFenceEvents[i]);
      ::WaitForSingleObject(mFenceEvents[i], DWORD_MAX);
    }
  }


  ////////////////////////////////////////////////////////////////////////////////////////////
  // Upload Thread

  DX12Renderer::UploadJob::UploadJob(DX12Renderer* aRenderer, Texture aTexture)
    : mRenderer{ aRenderer }
    , mVariant{ std::move(aTexture) }
  {

  }
  DX12Renderer::UploadJob::UploadJob(DX12Renderer* aRenderer, TextureTransition aTextureTransition)
    : mRenderer{ aRenderer }
    , mVariant{ std::move(aTextureTransition) }
  {
  }

  DX12Renderer::UploadJob::UploadJob(DX12Renderer* aRenderer, Buffer aBuffer)
    : mRenderer{ aRenderer }
    , mVariant{ std::move(aBuffer) }
  {
  }

  std::optional<DX12Renderer::UploadJob> DX12Renderer::UploadJob::operator()(DX12CommandBuffer aCommandBuffer)
  {
    return std::visit([this, aCommandBuffer](auto&& arg) -> std::optional<DX12Renderer::UploadJob> {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, Texture>)
      {
        auto [srvHeap, gpuHandle] = TextureUpload(aCommandBuffer, &arg);

        return UploadJob(mRenderer,
          TextureTransition{
            std::move(arg.mTexturePromise),
            std::move(arg.mTexture),
            std::move(arg.mUploadBuffer),
            std::move(srvHeap),
            gpuHandle,
            arg.mWidth,
            arg.mHeight,
            arg.mUploadPitch
          });
      }
      else if constexpr (std::is_same_v<T, TextureTransition>)
        TextureTransitionTask(aCommandBuffer, &arg);
      else if constexpr (std::is_same_v<T, Buffer>)
        BufferUpload(aCommandBuffer, &arg);
      else
        static_assert(always_false_v<T>, "non-exhaustive visitor!");

      return std::nullopt;
      }, mVariant);
  }

  void DX12Renderer::UploadJob::FulfillPromise()
  {
    std::visit([this](auto&& arg) {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, Texture>)
      {
        // Textures can't fulfill their promise, the TextureTransition has to do it.
      }
      else if constexpr (std::is_same_v<T, TextureTransition>)
      {
        arg.mTexturePromise.set_value(std::make_unique<DX12Texture>(
          std::move(arg.mTexture), 
          std::move(arg.mSrvHeap),
          arg.mGpuHandle,
          static_cast<int>(arg.mWidth), 
          static_cast<int>(arg.mHeight)));
      }
      else if constexpr (std::is_same_v<T, Buffer>)
      {
      }
      else
        static_assert(always_false_v<T>, "non-exhaustive visitor!");
      }, mVariant);
  }

  void DX12Renderer::UploadThread()
  {
    std::vector<UploadJob> uploads;
    std::vector<UploadJob> transitions;

    while (!mShouldJoin)
    {
      // Aquire Job
      mUploadJobsMutex.lock();
      std::swap(uploads, mUploadJobs);
      mUploadJobsMutex.unlock();

      /////////////////////////////////////////////
      // Uploads accrued
      auto transferCommandList = mTransferQueue.WaitOnNextCommandList();

      //transferCommandList.Begin();
      for (auto& job : uploads)
      {
        auto result = job(transferCommandList);

        if (result.has_value())
        {
          transitions.emplace_back(std::move(result.value()));
        }
      }

      //transferCommandList.End();
      //mTransferQueue.Submit(transferCommandList);
      transferCommandList.ExecuteOnQueue();

      //if (VK_NULL_HANDLE != transferCommandList.mFence)
      //{
      //  vkWaitForFences(mDevice.device, 1, &transferCommandList.mFence, true, UINT64_MAX);
      //}

      // Fulfill promises
      for (auto& job : uploads)
      {
        job.FulfillPromise();
      }

      /////////////////////////////////////////////
      // Transitions accrued
      auto transitionCommandList = mTextureTransitionQueue.WaitOnNextCommandList();

      //transitionCommandList.Begin();

      for (auto& textureTransfer : transitions)
      {
        textureTransfer(transitionCommandList);
      }

      transitionCommandList.ExecuteOnQueue();
      //transitionCommandList.End();
      //mTextureTransitionQueue.Submit(transitionCommandList);

      //if (VK_NULL_HANDLE != transitionCommandList.mFence)
      //{
      //  vkWaitForFences(mDevice.device, 1, &transitionCommandList.mFence, true, UINT64_MAX);
      //}

      transitionCommandList.Wait();

      // Fulfill promises
      for (auto& job : transitions)
      {
        job.FulfillPromise();
      }

      /////////////////////////////////////////////
      // End
      transitions.clear();
      uploads.clear();

      mUploadJobsWakeUp.acquire();
    }
  }

















  //////////////////////////////////////////////////////////////////////////////////////////////
  // Renderer
  DX12Renderer::DX12Renderer()
    : mUBOUpdates{ this }
    , mUploadJobsWakeUp{ 0 }
  {
  }

  void DX12Renderer::Initialize(SDL_Window* aWindow, char8_t const* /*aPreferredGpu*/)
  {
    mWindow = aWindow;
    EnableDebugLayer();

    // Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
    // Using this awareness context allows the client area of the window 
    // to achieve 100% scaling while still allowing non-client window content to 
    // be rendered in a DPI sensitive fashion.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Window class name. Used for registering / creating the window.
    const wchar_t* windowClassName = L"DX12WindowClass";

    mTearingSupported = CheckTearingSupport();

    auto factory = CreateFactory();

    ComPtr<IDXGIAdapter4> dxgiAdapter4 = GetAdapter(factory);

    mDevice = CreateDevice(dxgiAdapter4);

    mTransferQueue.Initialize(mDevice, D3D12_COMMAND_LIST_TYPE_COPY, 30);
    mTextureTransitionQueue.Initialize(mDevice, D3D12_COMMAND_LIST_TYPE_DIRECT, cNumFramesInFlight);
    mComputeQueue.Initialize(mDevice, D3D12_COMMAND_LIST_TYPE_COMPUTE, cNumFramesInFlight);
    mGraphicsQueue.Initialize(mDevice, D3D12_COMMAND_LIST_TYPE_DIRECT, cNumFramesInFlight);
    //mPresentQueue.Initialize(mDevice, D3D12_COMMAND_LIST_TYPE_DIRECT, cNumFramesInFlight);

    {
      D3D12_DESCRIPTOR_HEAP_DESC desc = {};
      desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
      desc.NumDescriptors = 1;
      desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
      if (mDevice.Get()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mSrvDescHeap)) != S_OK)
        return;
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(mWindow, &wmInfo);
    HWND windowHandle = (HWND)wmInfo.info.win.window;

    ImGui_ImplSDL2_InitForD3D(mWindow);

    ImGui_ImplDX12_Init(mDevice.Get(), cNumFramesInFlight,
      DXGI_FORMAT_R8G8B8A8_UNORM,
      mSrvDescHeap,
      mSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
      mSrvDescHeap->GetGPUDescriptorHandleForHeapStart()
    );

    mSwapChain = CreateSwapChain(windowHandle, factory, &(*mGraphicsQueue), mClientWidth, mClientHeight, cNumFramesInFlight);

    mCurrentBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

    mRTVDescriptorHeap = CreateDescriptorHeap(mDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, cNumFramesInFlight);
    mRTVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    UpdateRenderTargetViews(this, mDevice, mSwapChain, mRTVDescriptorHeap);

    for (int i = 0; i < cNumFramesInFlight; ++i)
    {
      mCommandAllocators[i] = CreateCommandAllocator(mDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
    }



    mUploadThread = std::thread([this]()
      {
        UploadThread();
      });
  }


  DX12Renderer::~DX12Renderer()
  {
    mShouldJoin = true;
    mUploadJobsWakeUp.release();
    mUploadThread.join();

    // Make sure the command queue has finished all commands before closing.
    mTransferQueue.Flush();
    mTextureTransitionQueue.Flush();
    mComputeQueue.Flush();
    mGraphicsQueue.Flush();
    mComputeQueue.Flush();

    ImGui_ImplDX12_Shutdown();
  }


  void DX12Renderer::NewFrame()
  {
    ImGui_ImplDX12_NewFrame();

    mCurrentBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

    auto commandBuffer = mGraphicsQueue.WaitOnNextCommandList();
    mTexturesToDestroyNextFrame.clear();
  }


  void DX12Renderer::ClearRenderTarget(glm::vec4 aClearColor)
  {
    // Clear the render target.
    auto commandBuffer = mGraphicsQueue.GetCurrentCommandList();

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      mBackBuffers[mCurrentBackBufferIndex].Get(),
      D3D12_RESOURCE_STATE_PRESENT, 
      D3D12_RESOURCE_STATE_RENDER_TARGET
    );

    commandBuffer->ResourceBarrier(1, &barrier);

    FLOAT clearColor[] = { aClearColor.x * aClearColor.w, aClearColor.y * aClearColor.w, aClearColor.z * aClearColor.w, aClearColor.w };
    CD3DX12_CPU_DESCRIPTOR_HANDLE renderTargetView{
      mRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
      static_cast<INT>(mCurrentBackBufferIndex),
      mRTVDescriptorSize
    };

    commandBuffer->ClearRenderTargetView(renderTargetView, clearColor, 0, nullptr);
    commandBuffer->OMSetRenderTargets(1, &renderTargetView, FALSE, NULL);
  }

  void DX12Renderer::RenderImguiData()
  {
    auto commandBuffer = mGraphicsQueue.GetCurrentCommandList();

    // Record dear imgui primitives into command buffer
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandBuffer.Buffer().Get());
  }

  void DX12Renderer::Present()
  {
    // Present
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      mBackBuffers[mCurrentBackBufferIndex].Get(),
      D3D12_RESOURCE_STATE_RENDER_TARGET, 
      D3D12_RESOURCE_STATE_PRESENT);

    auto commandBuffer = mGraphicsQueue.GetCurrentCommandList();
    commandBuffer->ResourceBarrier(1, &barrier);
    
    // We wait for the upload command buffer fence before executing the rendering command buffer.
    mTextureTransitionQueue.GetCurrentCommandList().Wait();
    commandBuffer.ExecuteOnQueue();
    
    UINT syncInterval = mVSync ? 1 : 0;
    UINT presentFlags = mTearingSupported && !mVSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
    ThrowIfFailed(mSwapChain->Present(syncInterval, presentFlags));
  }

  void DX12Renderer::ResizeRenderTarget(unsigned aWidth, unsigned aHeight)
  {
    if (mClientWidth != aWidth || mClientHeight != aHeight)
    {
      // Don't allow 0 size swap chain back buffers.
      mClientWidth = std::max(1u, aWidth);
      mClientHeight = std::max(1u, aHeight);

      // Flush the GPU queue to make sure the swap chain's back buffers
      // are not being referenced by an in-flight command list.
      mGraphicsQueue.Flush();

      for (auto& backBuffer : mBackBuffers)
      {
        // Any references to the back buffers must be released
        // before the swap chain can be resized.
        backBuffer.Reset();
      }
      mBackBuffers[mCurrentBackBufferIndex].Reset();

      DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
      ThrowIfFailed(mSwapChain->GetDesc(&swapChainDesc));
      ThrowIfFailed(mSwapChain->ResizeBuffers(cNumFramesInFlight, mClientWidth, mClientHeight,
        swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

      mCurrentBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

      UpdateRenderTargetViews(this, mDevice, mSwapChain, mRTVDescriptorHeap);
    }
  }

  GPUAllocator* DX12Renderer::MakeAllocator(std::string const& aAllocatorType, size_t aBlockSize)
  {
    auto it = mAllocators.find(aAllocatorType);

    if (it != mAllocators.end())
    {
      return it->second.get();
    }

    auto allocator = std::make_unique<DX12GPUAllocator>(aAllocatorType, aBlockSize, this);

    auto ptr = allocator.get();

    mAllocators.emplace(aAllocatorType, std::move(allocator));

    return ptr;
  }



  template <typename tType>
  static uint64_t ToU64(tType aValue)
  {
    return static_cast<uint64_t>(aValue);
  }


  static D3D12_HEAP_TYPE ToDX12(GPUAllocation::MemoryProperty aValue)
  {
    D3D12_HEAP_TYPE toReturn{};

    auto value = ToU64(aValue);

    if (0 != (value & ToU64(GPUAllocation::MemoryProperty::DeviceLocal)))
    {
      return D3D12_HEAP_TYPE_DEFAULT;
    }

    if (0 != (value & ToU64(GPUAllocation::MemoryProperty::HostVisible)))
    {
      return D3D12_HEAP_TYPE_UPLOAD;
    }

    if (0 != (value & ToU64(GPUAllocation::MemoryProperty::HostCoherent)))
    {
      return D3D12_HEAP_TYPE_UPLOAD;
    }

    if (0 != (value & ToU64(GPUAllocation::MemoryProperty::HostCached)))
    {
      return D3D12_HEAP_TYPE_UPLOAD;
    }

    if (0 != (value & ToU64(GPUAllocation::MemoryProperty::LazilyAllocated)))
    {
      return D3D12_HEAP_TYPE_DEFAULT;
    }

    if (0 != (value & ToU64(GPUAllocation::MemoryProperty::Protected)))
    {
      return D3D12_HEAP_TYPE_DEFAULT;
    }

    return toReturn;
  }

  static D3D12_RESOURCE_STATES ToDX12(GPUAllocation::BufferUsage aValue)
  {
    D3D12_RESOURCE_STATES toReturn{};

    auto value = ToU64(aValue);

    if (0 != (value & ToU64(GPUAllocation::BufferUsage::TransferSrc)))
    {
      toReturn = toReturn | D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (0 != (value & ToU64(GPUAllocation::BufferUsage::TransferDst)))
    {
      toReturn = toReturn | D3D12_RESOURCE_STATE_COPY_DEST;
    }
    if (0 != (value & ToU64(GPUAllocation::BufferUsage::UniformTexelBuffer)))
    {
      //toReturn = toReturn | eUniformTexelBuffer;
    }
    if (0 != (value & ToU64(GPUAllocation::BufferUsage::StorageTexelBuffer)))
    {
      //toReturn = toReturn | eStorageTexelBuffer;
    }
    if (0 != (value & ToU64(GPUAllocation::BufferUsage::UniformBuffer)))
    {
      toReturn = toReturn | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    }
    if (0 != (value & ToU64(GPUAllocation::BufferUsage::StorageBuffer)))
    {
      //toReturn = toReturn | eStorageBuffer;
    }
    if (0 != (value & ToU64(GPUAllocation::BufferUsage::IndexBuffer)))
    {
      toReturn = toReturn | D3D12_RESOURCE_STATE_INDEX_BUFFER;
    }
    if (0 != (value & ToU64(GPUAllocation::BufferUsage::VertexBuffer)))
    {
      toReturn = toReturn | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    }
    if (0 != (value & ToU64(GPUAllocation::BufferUsage::IndirectBuffer)))
    {
      toReturn = toReturn | D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    }

    return toReturn;
  }


  //std::unique_ptr<GPUBufferBase> DX12GPUAllocator::CreateBufferInternal(size_t aSize,
  //  GPUAllocation::BufferUsage aUsage,
  //  GPUAllocation::MemoryProperty aProperties)
  std::unique_ptr<GPUBufferBase> DX12GPUAllocator::CreateBufferInternal(
    size_t aSize,
    GPUAllocation::BufferUsage aUse,
    GPUAllocation::MemoryProperty aProperties)
  {
    auto self = mData.Get<DX12GPUAllocatorData>();

    auto base = std::make_unique<DX12UBO>(aSize);
    auto uboData = base->GetData().ConstructAndGet<DX12UBOData>();

    auto usage = ToDX12(aUse);
    auto property = ToDX12(aProperties);

    auto heapProps = CD3DX12_HEAP_PROPERTIES(property);
    auto bufferDescription = CD3DX12_RESOURCE_DESC::Buffer(aSize, D3D12_RESOURCE_FLAG_NONE);
    ThrowIfFailed(self->mRenderer->mDevice->CreateCommittedResource(
      &heapProps,
      D3D12_HEAP_FLAG_NONE,
      &bufferDescription,
      usage,
      nullptr,
      IID_PPV_ARGS(&base->GetBuffer())
    ));

    uboData->mRenderer = self->mRenderer;
    uboData->mState = usage;

    return static_unique_pointer_cast<GPUBufferBase>(std::move(base));
  }
  
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

  void DX12Renderer::Upload()
  {
    //auto commandBuffer = mTransferQueue.WaitOnNextCommandList();
    //mUBOUpdates.Update(commandBuffer.Buffer());
    //commandBuffer.ExecuteOnQueue();
    //commandBuffer.Wait();
    //TransitionTextures();
  }

  void DX12Renderer::TransitionTextures()
  {
    //if (mTexturesCreatedThisFrame.empty())
    //  return;
    //
    //auto commandBuffer = mTextureTransitionQueue.WaitOnNextCommandList();
    //
    //for (auto& textureTransfer : mTexturesCreatedThisFrame)
    //{
    //  D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    //  srcLocation.pResource = textureTransfer.mUploadBuffer.Get();
    //  srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    //  srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    //  srcLocation.PlacedFootprint.Footprint.Width = textureTransfer.mWidth;
    //  srcLocation.PlacedFootprint.Footprint.Height = textureTransfer.mHeight;
    //  srcLocation.PlacedFootprint.Footprint.Depth = 1;
    //  srcLocation.PlacedFootprint.Footprint.RowPitch = textureTransfer.mUploadPitch;
    //
    //  D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    //  dstLocation.pResource = textureTransfer.mTexture.Get();
    //  dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    //  dstLocation.SubresourceIndex = 0;
    //
    //  D3D12_RESOURCE_BARRIER barrier = {};
    //  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    //  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    //  barrier.Transition.pResource = textureTransfer.mTexture.Get();
    //  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    //  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    //  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    //
    //  commandBuffer->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, NULL);
    //  commandBuffer->ResourceBarrier(1, &barrier);
    //}
    //
    //commandBuffer.ExecuteOnQueue();
    //
    //mTexturesCreatedThisFrame.clear();
  }

  std::unique_ptr<Texture> DX12Renderer::LoadTextureFromData(unsigned char* aData, TextureLayout aFormat, int aWidth, int aHeight, int pitch)
  {
    // Create texture resource
    D3D12_HEAP_PROPERTIES props;
    memset(&props, 0, sizeof(D3D12_HEAP_PROPERTIES));
    props.Type = D3D12_HEAP_TYPE_DEFAULT;
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = aWidth;
    desc.Height = aHeight;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    ID3D12Resource* pTexture = NULL;
    mDevice->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
      D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&pTexture));

    // Create a temporary upload resource to move the data in
    UINT uploadPitch = (aWidth * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
    UINT uploadSize = aHeight * uploadPitch;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = uploadSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    props.Type = D3D12_HEAP_TYPE_UPLOAD;
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    ID3D12Resource* uploadBuffer = NULL;
    HRESULT hr = mDevice->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
      D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&uploadBuffer));
    IM_ASSERT(SUCCEEDED(hr));

    // Write pixels into the upload resource
    void* mapped = NULL;
    D3D12_RANGE range = { 0, uploadSize };
    hr = uploadBuffer->Map(0, &range, &mapped);
    IM_ASSERT(SUCCEEDED(hr));
    for (int y = 0; y < aHeight; y++)
      memcpy((void*)((uintptr_t)mapped + y * uploadPitch), aData + y * aWidth * 4, aWidth * 4);
    uploadBuffer->Unmap(0, &range);

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDescription = {
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
      1,
      D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
      0
    };
    ID3D12DescriptorHeap* srvDescHeap = nullptr;
    if (mDevice.Get()->CreateDescriptorHeap(&srvHeapDescription, IID_PPV_ARGS(&srvDescHeap)) != S_OK)
      return nullptr;

    UINT handle_increment = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    int descriptor_index = 0; // The descriptor table index to use (not normally a hard-coded constant, but in this case we'll assume we have slot 1 reserved for us)
    D3D12_CPU_DESCRIPTOR_HANDLE my_texture_srv_cpu_handle = srvDescHeap->GetCPUDescriptorHandleForHeapStart();
    my_texture_srv_cpu_handle.ptr += (handle_increment * descriptor_index);
    D3D12_GPU_DESCRIPTOR_HANDLE my_texture_srv_gpu_handle = srvDescHeap->GetGPUDescriptorHandleForHeapStart();
    my_texture_srv_gpu_handle.ptr += (handle_increment * descriptor_index);


    // Create a shader resource view for the texture
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    mDevice->CreateShaderResourceView(pTexture, &srvDesc, my_texture_srv_cpu_handle);

    mTexturesCreatedThisFrame.emplace_back(aWidth, aHeight, uploadPitch, pTexture, uploadBuffer);

    return std::make_unique<DX12Texture>(pTexture, srvDescHeap, my_texture_srv_gpu_handle, aWidth, aHeight);
  }


  std::future<std::unique_ptr<Texture>> DX12Renderer::LoadTextureFromDataAsync(unsigned char* aData, TextureLayout aFormat, int aWidth, int aHeight, int aPitch)
  {
    // Create texture resource
    D3D12_HEAP_PROPERTIES props;
    memset(&props, 0, sizeof(D3D12_HEAP_PROPERTIES));
    props.Type = D3D12_HEAP_TYPE_DEFAULT;
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = aWidth;
    desc.Height = aHeight;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3D12Resource> pTexture = NULL;
    mDevice->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
      D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&pTexture));

    // Create a temporary upload resource to move the data in
    UINT uploadPitch = (aWidth * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
    UINT uploadSize = aHeight * uploadPitch;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = uploadSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    props.Type = D3D12_HEAP_TYPE_UPLOAD;
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer = NULL;
    HRESULT hr = mDevice->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
      D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&uploadBuffer));
    IM_ASSERT(SUCCEEDED(hr));

    // Write pixels into the upload resource
    void* mapped = NULL;
    D3D12_RANGE range = { 0, uploadSize };
    hr = uploadBuffer->Map(0, &range, &mapped);
    IM_ASSERT(SUCCEEDED(hr));
    for (int y = 0; y < aHeight; y++)
      memcpy((void*)((uintptr_t)mapped + y * uploadPitch), aData + y * aWidth * 4, aWidth * 4);
    uploadBuffer->Unmap(0, &range);


    std::promise<std::unique_ptr<SOIS::Texture>> texturePromise;
    std::future<std::unique_ptr<SOIS::Texture>> textureFuture = texturePromise.get_future();

    {
      std::unique_lock lock{ mUploadJobsMutex };
      mUploadJobs.emplace_back(
        this,
        UploadJob::Texture{
          std::move(texturePromise),
          std::move(pTexture),
          std::move(uploadBuffer),
          desc,
          static_cast<size_t>(aWidth),
          static_cast<size_t>(aHeight),
          static_cast<size_t>(uploadPitch)
        });
    }
    mUploadJobsWakeUp.release(1);

    return textureFuture;
  }


  DX12Renderer::UploadJob::UploadValues DX12Renderer::UploadJob::TextureUpload(DX12CommandBuffer aCommandBuffer, Texture* aTexture)
  {
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDescription = {
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
      1,
      D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
      0
    };
    ID3D12DescriptorHeap* srvDescHeap = nullptr;
    if (mRenderer->mDevice.Get()->CreateDescriptorHeap(&srvHeapDescription, IID_PPV_ARGS(&srvDescHeap)) != S_OK)
    {
      printf("Couldn't create a DescriptorHeap for a texture\n");
      return {nullptr, NULL};
    }
    
    UINT handle_increment = mRenderer->mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    int descriptor_index = 0; // The descriptor table index to use (not normally a hard-coded constant, but in this case we'll assume we have slot 1 reserved for us)
    D3D12_CPU_DESCRIPTOR_HANDLE my_texture_srv_cpu_handle = srvDescHeap->GetCPUDescriptorHandleForHeapStart();
    my_texture_srv_cpu_handle.ptr += (handle_increment * descriptor_index);
    D3D12_GPU_DESCRIPTOR_HANDLE my_texture_srv_gpu_handle = srvDescHeap->GetGPUDescriptorHandleForHeapStart();
    my_texture_srv_gpu_handle.ptr += (handle_increment * descriptor_index);
    
    // Create a shader resource view for the texture
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = aTexture->mDescription.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    
    mRenderer->mDevice->CreateShaderResourceView(aTexture->mTexture.Get(), &srvDesc, my_texture_srv_cpu_handle);

    return { srvDescHeap, my_texture_srv_gpu_handle };
    
    //mTexturesCreatedThisFrame.emplace_back(aWidth, aHeight, uploadPitch, pTexture, uploadBuffer);
    //
    //return std::make_unique<DX12Texture>(pTexture, srvDescHeap, my_texture_srv_gpu_handle, aWidth, aHeight);
  }

  void DX12Renderer::UploadJob::BufferUpload(DX12CommandBuffer aCommandBuffer, Buffer* aBuffer)
  {

  }

  void DX12Renderer::UploadJob::TextureTransitionTask(DX12CommandBuffer aCommandBuffer, TextureTransition* aTextureTransition)
  {
    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    srcLocation.pResource = aTextureTransition->mUploadBuffer.Get();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srcLocation.PlacedFootprint.Footprint.Width = static_cast<UINT>(aTextureTransition->mWidth);
    srcLocation.PlacedFootprint.Footprint.Height = static_cast<UINT>(aTextureTransition->mHeight);
    srcLocation.PlacedFootprint.Footprint.Depth = 1;
    srcLocation.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(aTextureTransition->mUploadPitch);

    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    dstLocation.pResource = aTextureTransition->mTexture.Get();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = 0;

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = aTextureTransition->mTexture.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    
    aCommandBuffer->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, NULL);
    aCommandBuffer->ResourceBarrier(1, &barrier);
  }
}
