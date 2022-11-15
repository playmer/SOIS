#pragma once


#include <cstring>
#include <future>
#include <limits>
#include <memory>
#include <semaphore>
#include <string>
#include <thread>
#include <vector>
#include <variant>
#include <unordered_map>
#include <map>

#include "imgui.h"

#include <SDL.h>
#include "glm/glm.hpp"


#include "SOIS/Pipeline.hpp"
#include "PrivateImplementation.hpp"

namespace SOIS
{
  class OpenGL45Renderer;
  class DX11Renderer;
  class DX12Renderer;
  class Renderer;

  // Make the renderers, we put these into their own cpp files to simplify the code around
  // compiling them on different platforms.
  std::unique_ptr<Renderer> MakeOpenGL3Renderer();
  std::unique_ptr<Renderer> MakeDX11Renderer();
  std::unique_ptr<Renderer> MakeDX12Renderer();
  std::unique_ptr<Renderer> MakeVulkanRenderer();

  struct GPUAllocator;

  enum class TextureLayout
  {
    RGBA_Unorm,
    RGBA_Srgb,
    Bc1_Rgba_Unorm,
    Bc1_Rgba_Srgb,
    Bc3_Srgb,
    Bc3_Unorm,
    Bc7_Unorm,
    Bc7_Srgb,
    InvalidLayout
  };


  class Texture
  {
  public:
    Texture(
      int aWidth,
      int aHeight)
      : Width{ aWidth }
      , Height{ aHeight }
    {

    }

    virtual ~Texture()
    {
    };

    virtual ImTextureID GetTextureId()
    {
      return ImTextureID{ nullptr };
    };

    int Width;
    int Height;
  };


  namespace GPUAllocation
  {
    enum class MemoryProperty
    {
      DeviceLocal = 0x00000001,
      HostVisible = 0x00000002,
      HostCoherent = 0x00000004,
      HostCached = 0x00000008,
      LazilyAllocated = 0x00000010,
      Protected = 0x00000020,
    };

    enum class BufferUsage
    {
      TransferSrc = 0x00000001,
      TransferDst = 0x00000002,
      UniformTexelBuffer = 0x00000004,
      StorageTexelBuffer = 0x00000008,
      UniformBuffer = 0x00000010,
      StorageBuffer = 0x00000020,
      IndexBuffer = 0x00000040,
      VertexBuffer = 0x00000080,
      IndirectBuffer = 0x00000100,
    };


    // https://softwareengineering.stackexchange.com/a/204566
    inline MemoryProperty operator | (MemoryProperty lhs, MemoryProperty rhs)
    {
      using T = std::underlying_type_t <MemoryProperty>;
      return static_cast<MemoryProperty>(static_cast<T>(lhs) | static_cast<T>(rhs));
    }

    inline MemoryProperty& operator |= (MemoryProperty& lhs, MemoryProperty rhs)
    {
      lhs = lhs | rhs;
      return lhs;
    }

    inline BufferUsage operator | (BufferUsage lhs, BufferUsage rhs)
    {
      using T = std::underlying_type_t <BufferUsage>;
      return static_cast<BufferUsage>(static_cast<T>(lhs) | static_cast<T>(rhs));
    }

    inline GPUAllocation::BufferUsage& operator |= (BufferUsage& lhs, BufferUsage rhs)
    {
      lhs = lhs | rhs;
      return lhs;
    }
  }

  class GPUBufferBase
  {
  public:
    GPUBufferBase(size_t aSize)
      : mArraySize{ aSize }
    {

    }

    void Update(uint8_t const* aPointer, size_t aBytes, size_t aOffset) 
    {
      mUpdateFn(this, aPointer, aBytes, aOffset);
    };

  protected:
    using UpdateFn = void (*)(GPUBufferBase* aBase, uint8_t const* aPointer, size_t aBytes, size_t aOffset);
    
    friend class Renderer;

    PrivateImplementationLocal<32> mData;
    size_t mArraySize;
    UpdateFn mUpdateFn;
  };


  template <typename tType>
  class GPUBuffer
  {
  public:
    GPUBuffer()
    {

    }

    GPUBuffer(GPUBufferBase&& aBuffer)
      : mBuffer{ std::move(aBuffer) }
    {

    }

    GPUBufferBase& GetBase()
    {
      return *mBuffer;
    }

    void Update(tType const& aData)
    {
      mBuffer.Update(reinterpret_cast<uint8_t const*>(&aData), sizeof(tType), 0);
    }

    void Update(tType const* aData, size_t aSize)
    {
      mBuffer.Update(reinterpret_cast<uint8_t const*>(aData), sizeof(tType) * aSize, 0);
    }

    void Update(ContiguousRange<tType> aData)
    {
      mBuffer.Update(reinterpret_cast<uint8_t const*>(aData.begin()), sizeof(tType) * aData.size(), 0);
    }

    operator bool()
    {
      return mBuffer != nullptr;
    }

    GPUBufferBase Steal()
    {
      return std::move(mBuffer);
    }

  private:
    GPUBufferBase mBuffer;
  };

  template <typename tType>
  class GPUBufferRef
  {
  public:
    GPUBufferRef()
    {

    }

    GPUBufferRef(GPUBuffer<tType>& aBuffer)
      : mBuffer{ &(aBuffer.GetBase()) }
    {

    }

    GPUBufferRef(GPUBufferBase& aBuffer)
      : mBuffer{ &aBuffer }
    {

    }

    GPUBufferRef(GPUBufferBase* aBuffer)
      : mBuffer{ aBuffer }
    {

    }

    GPUBufferBase& GetBase()
    {
      return *mBuffer;
    }

    void Update(tType const& aData)
    {
      mBuffer->Update(reinterpret_cast<u8 const*>(&aData), sizeof(tType), 0);
    }

    void Update(tType const* aData, size_t aSize)
    {
      mBuffer->Update(reinterpret_cast<u8 const*>(aData), sizeof(tType) * aSize, 0);
    }

    void Update(ContiguousRange<tType> aData)
    {
      mBuffer->Update(reinterpret_cast<u8 const*>(aData.begin()), sizeof(tType) * aData.size(), 0);
    }

    operator bool()
    {
      return mBuffer != nullptr;
    }

  private:
    GPUBufferBase* mBuffer;
  };

  template<typename tType>
  class GPUBufferFuture
  {
  public:
    GPUBufferFuture(std::future<GPUBufferBase>&& aFuture)
      : mFuture{std::move(aFuture)}
    {

    }

    GPUBuffer<tType> Get()
    {
      return GPUBuffer<tType>{std::move(mFuture.get())};
    }

  private:
    std::future<GPUBufferBase> mFuture;
  };

  class GPUPipeline
  {
  private:
    friend class Renderer;
    PrivateImplementationLocal<32> mData;
  };

  struct ClearColor
  {
    u8 r, g, b, a;
  };

  struct RenderStateCommand
  {
    ClearColor mColor;
  };

  struct BindVertexBufferCommand
  {
    std::vector<GPUBufferBase> mGPUBuffers;
  };
  struct BindIndexBufferCommand
  {
    GPUBufferBase mGPUBuffer;
  };
  struct BindPipelineCommand
  {
    GPUPipeline mPipeline;
  };
  struct DrawCommand
  {
    u32 mIndexCount;
  };

  class GPUCommandList
  {
  public:
    void SetRenderState(RenderStateCommand aRenderState);
    void BindVertexBuffer(std::vector<GPUBufferBase> aGPUBuffers);
    void BindIndexBuffer(GPUBufferBase aGPUBuffer);
    void BindPipeline(GPUPipeline aPipeline);
    void DrawIndexed();


  protected:
    using Command = std::variant<RenderStateCommand, BindVertexBufferCommand, BindIndexBufferCommand, BindPipelineCommand, DrawCommand>;

    friend class Renderer;

    std::vector<Command> mCommands;
  };


  class Renderer
  {
  public:
    Renderer()
      : mUploadJobsWakeUp{ 0 }
    {
    };
    virtual ~Renderer() {};

    virtual void Initialize(SDL_Window*, char8_t const* /*aPreferredGpu*/) {};

    virtual SDL_WindowFlags GetAdditionalWindowFlags() { return (SDL_WindowFlags)0; };

    virtual void NewFrame() = 0;
    virtual void ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight) = 0;

    // Some backends (dx12, vulkan) need to set descriptors before/after any calls to image
    // functions. So this is to allow for that.
    //virtual void PreImguiImage(Texture* aTexture) { (void)aTexture; };
    //virtual void PostImguiImage(Texture* aTexture) { (void)aTexture; };


    virtual std::future<std::unique_ptr<Texture>> LoadTextureFromDataAsync(unsigned char* data, TextureLayout format, int w, int h, int pitch) = 0;
    std::future<std::unique_ptr<Texture>> LoadTextureFromFileAsync(std::u8string const& aFile);
    std::future<GPUPipeline> CreatePipeline(std::u8string const& aVertexShader, std::u8string const& aFragmentShader);




    // Creates a buffer of the given type, aSize allows you to make an array of them.
    // Size must be at least 1
    template <typename tType>
    GPUBufferFuture<tType> CreateBuffer(size_t aSize,
      GPUAllocation::BufferUsage aUse,
      GPUAllocation::MemoryProperty aProperties)
    {
      size_t sizeOfObject = sizeof(tType) * aSize;
    
      auto buffer = CreateBufferInternal(sizeOfObject, aUse, aProperties);
    
      return GPUBufferFuture<tType>(std::move(buffer));
    }
    
    //virtual GPUAllocator* MakeAllocator(std::string const& aAllocatorType, size_t aBlockSize)
    //{
    //  (void)aAllocatorType;
    //  (void)aBlockSize;
    //  return nullptr;
    //};

    virtual void ClearRenderTarget(glm::vec4 aClearColor) = 0;
    virtual void Upload() {};
    virtual void RenderImguiData() = 0;
    virtual void Present() = 0;
    virtual void ExecuteCommandList(GPUCommandList& aList) {};

  protected:
    static PrivateImplementationLocal<32>& GetDataFromGPUObject(GPUBufferBase& aBuffer)
    {
      return aBuffer.mData;
    }
    static PrivateImplementationLocal<32>& GetDataFromGPUObject(GPUPipeline& aPipeline)
    {
      return aPipeline.mData;
    }

    std::vector<GPUCommandList::Command>& GetCommandsFromList(GPUCommandList& aList)
    {
      return aList.mCommands;
    }


    virtual std::future<GPUBufferBase> CreateBufferInternal(size_t aSize,
      GPUAllocation::BufferUsage aUse,
      GPUAllocation::MemoryProperty aProperties)
    {
      (void)aSize; (void)aUse; (void)aProperties;
      std::promise<GPUBufferBase> p;
      std::future<GPUBufferBase> empty = p.get_future();
      return empty;
    };

    friend class GPUCommandList;


    //std::unordered_map<std::string, std::unique_ptr<GPUAllocator>> mAllocators;
    std::thread mUploadThread;
    std::atomic_bool mShouldJoin;
    std::mutex mUploadJobsMutex;
    std::counting_semaphore<std::numeric_limits<std::ptrdiff_t>::max()> mUploadJobsWakeUp;
  };
}
