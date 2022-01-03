#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include <map>

#include "imgui.h"

#include <SDL.h>
#include "glm/glm.hpp"

namespace SOIS
{
  class OpenGL3Renderer;
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

  using s8 = signed char;
  using s16 = signed short;
  using s32 = signed int;
  using s64 = signed long long;

  using i8 = char;
  using i16 = std::int16_t;
  using i32 = std::int32_t;
  using i64 = std::int64_t;

  using u8 = std::uint8_t;
  using u16 = std::uint16_t;
  using u32 = std::uint32_t;
  using u64 = std::uint64_t;

  using byte = std::uint8_t;

  inline void runtime_assert(bool value, char const* message)
  {
    if (!value)
    {
      printf("%s\nAborting...", message);
      __debugbreak();
      abort();
    }
  }

  // Enable if helpers
  namespace EnableIf
  {
    template<typename tType>
    using IsNotVoid = std::enable_if_t<!std::is_void_v<tType>>;

    template<typename tType>
    using IsVoid = std::enable_if_t<std::is_void_v<tType>>;

    template<typename tType>
    using IsMoveConstructible = std::enable_if_t<std::is_move_constructible_v<tType>>;

    template<typename tType>
    using IsNotMoveConstructible = std::enable_if_t<!std::is_move_constructible_v<tType>>;

    template<typename tType>
    using IsMoveAssignable = std::enable_if_t<std::is_move_assignable_v<tType>>;

    template<typename tType>
    using IsNotMoveAssignable = std::enable_if_t<!std::is_move_assignable_v<tType>>;

    template<typename tType>
    using IsCopyConstructible = std::enable_if_t<std::is_copy_constructible_v<tType>>;

    template<typename tType>
    using IsNotCopyConstructible = std::enable_if_t<!std::is_copy_constructible_v<tType>>;

    template<typename tType>
    using IsCopyAssignable = std::enable_if_t<std::is_copy_assignable_v<tType>>;

    template<typename tType>
    using IsNotCopyAssignable = std::enable_if_t<!std::is_copy_assignable_v<tType>>;

    template<typename tType>
    using IsDefaultConstructible = std::enable_if_t<std::is_default_constructible_v<tType>>;

    template<typename tType>
    using IsNotDefaultConstructible = std::enable_if_t<!std::is_default_constructible_v<tType>>;
  }


  template<typename tTo, typename tFrom>
  std::unique_ptr<tTo> static_unique_pointer_cast(std::unique_ptr<tFrom>&& aValue)
  {
    //conversion: unique_ptr<FROM>->FROM*->TO*->unique_ptr<TO>
    return std::unique_ptr<tTo>{static_cast<tTo*>(aValue.release())};
  }


  template <typename tType>
  inline void GenericDestructByte(byte* aMemory)
  {
    (reinterpret_cast<tType*>(aMemory))->~tType();
  }

  template <typename tType>
  inline void GenericDestructor(void* aMemory)
  {
    (static_cast<tType*>(aMemory))->~tType();
  }


  ///////////////////////////////////////
  // Default Constructor
  ///////////////////////////////////////
  template <typename tType, typename = void>
  struct GenericDefaultConstructStruct
  {

  };

  template <typename tType>
  struct GenericDefaultConstructStruct<tType, EnableIf::IsNotDefaultConstructible<tType>>
  {
    static inline void DefaultConstruct(void*)
    {
      runtime_assert(false, "Trying to default construct something without a default constructor.");
    }
  };

  template <typename tType>
  struct GenericDefaultConstructStruct<tType, EnableIf::IsDefaultConstructible<tType>>
  {
    static inline void DefaultConstruct(void* aMemory)
    {
      new (aMemory) tType();
    }
  };

  template <typename tType>
  inline void GenericDefaultConstruct(void* aMemory)
  {
    GenericDefaultConstructStruct<tType>::DefaultConstruct(aMemory);
  }

  template <>
  inline void GenericDefaultConstruct<void>(void*)
  {
  }

  template <>
  inline void GenericDestructor<void>(void*)
  {
  }

  ///////////////////////////////////////
  // Move Constructor
  ///////////////////////////////////////
  template <typename tType, typename Enable = void>
  struct GenericMoveConstructStruct
  {

  };

  template <typename tType>
  struct GenericMoveConstructStruct<tType, EnableIf::IsNotMoveConstructible<tType>>
  {
    static inline void MoveConstruct(void*, void*)
    {
      runtime_assert(false, "Trying to move construct something without a move constructor.");
    }
  };

  template <typename tType>
  struct GenericMoveConstructStruct<tType, EnableIf::IsMoveConstructible<tType>>
  {
    static inline void MoveConstruct(void* aLeft, void* aRight)
    {
      new (aLeft) tType(std::move(*static_cast<tType*>(aRight)));
    }
  };

  template <typename tType>
  inline void GenericMoveConstruct(void* aObject, void* aMemory)
  {
    GenericMoveConstructStruct<tType>::MoveConstruct(aObject, aMemory);
  }

  ///////////////////////////////////////
  // Move Assignment
  ///////////////////////////////////////
  template <typename tType, typename Enable = void>
  struct GenericMoveAssignmentStruct
  {

  };

  template <typename tType>
  struct GenericMoveAssignmentStruct<tType, EnableIf::IsNotMoveConstructible<tType>>
  {
    static inline void MoveAssign(void*, void*)
    {
      runtime_assert(false, "Trying to move construct something without a move constructor.");
    }
  };

  template <typename tType>
  struct GenericMoveAssignmentStruct<tType, EnableIf::IsMoveConstructible<tType>>
  {
    static inline void MoveAssign(void* aLeft, void* aRight)
    {
      static_cast<tType*>(aLeft) = std::move(*static_cast<tType*>(aRight));
    }
  };

  template <typename tType>
  inline void GenericMoveAssignment(void* aObject, void* aMemory)
  {
    GenericMoveAssignmentStruct<tType>::MoveAssign(aObject, aMemory);
  }

  ///////////////////////////////////////
  // Copy Constructor
  ///////////////////////////////////////
  template <typename T, typename Enable = void>
  struct GenericCopyConstructStruct
  {

  };

  template <typename tType>
  struct GenericCopyConstructStruct<tType, EnableIf::IsNotCopyConstructible<tType>>
  {
    static inline void CopyConstruct(const void*, void*)
    {
      runtime_assert(false, "Trying to copy construct something without a copy constructor.");
    }
  };

  template <typename tType>
  struct GenericCopyConstructStruct<tType, EnableIf::IsCopyConstructible<tType>>
  {
    static inline void CopyConstruct(void* aLeft, void const* aRight)
    {
      new (aLeft) tType(*static_cast<tType const*>(aRight));
    }
  };

  template <typename tType>
  inline void GenericCopyConstruct(const void* aObject, void* aMemory)
  {
    GenericCopyConstructStruct<tType>::CopyConstruct(aObject, aMemory);
  }

  template <>
  inline void GenericCopyConstruct<void>(const void*, void*)
  {
  }

  ///////////////////////////////////////
  // Copy Assignment
  ///////////////////////////////////////
  template <typename tType, typename Enable = void>
  struct GenericCopyAssignmentStruct
  {

  };

  template <typename tType>
  struct GenericCopyAssignmentStruct<tType, EnableIf::IsNotCopyConstructible<tType>>
  {
    static inline void CopyAssign(void*, void*)
    {
      runtime_assert(false, "Trying to move construct something without a move constructor.");
    }
  };

  template <typename tType>
  struct GenericCopyAssignmentStruct<tType, EnableIf::IsCopyConstructible<tType>>
  {
    static inline void CopyAssign(void* aLeft, void* aRight)
    {
      static_cast<tType*>(aLeft) = std::move(*static_cast<tType*>(aRight));
    }
  };

  template <typename tType>
  inline void GenericCopyAssignment(void* aObject, void* aMemory)
  {
    GenericCopyAssignmentStruct<tType>::MoveAssign(aObject, aMemory);
  }

  // Helper to call the constructor of a type.
  template<typename tType = void>
  inline void GenericDoNothing(tType*)
  {
  }



  template <typename tType>
  class ContiguousRange
  {
  public:
    ContiguousRange(std::vector<tType>& aContainer)
      : mBegin(aContainer.data())
      , mEnd(aContainer.data() + aContainer.size())
    {
    }

    ContiguousRange(tType& aValue)
      : mBegin(&aValue)
      , mEnd(&aValue + 1)
    {
    }

    ContiguousRange(tType* aBegin, tType* aEnd)
      : mBegin(aBegin)
      , mEnd(aEnd)
    {
    }

    bool IsRange() { return mBegin != mEnd; }

    tType const* begin() const { return mBegin; }
    tType const* end() const { return mEnd; }
    tType* begin() { return mBegin; }
    tType* end() { return mEnd; }

    typename size_t size() const { return mEnd - mBegin; }
  protected:
    tType* mBegin;
    tType* mEnd;
  };

  template <typename tType>
  ContiguousRange<tType> MakeContiguousRange(std::vector<tType> aContainer)
  {
    return ContiguousRange<tType>(&*aContainer.begin(), &*aContainer.end());
  }

  //template<typename tType, size_t tElementCount>
  //ContiguousRange<tType> MakeContiguousRange(std::array<tType, tElementCount> aContainer)
  //{
  //  return ContiguousRange<tType>(&*aContainer.begin(), &*aContainer.end());
  //}

  template <typename tType>
  ContiguousRange<tType> MakeContiguousRange(tType& aValue)
  {
    return ContiguousRange<tType>(&aValue, &aValue + 1);
  }




  namespace detail
  {
    template <typename tReturn, typename Arg = tReturn>
    struct GetReturnType {};

    template <typename tReturn, typename tObject, typename ...tArguments>
    struct GetReturnType<tReturn(tObject::*)(tArguments...)>
    {
      using ReturnType = tReturn;
      using tObjectType = tObject;
    };

    template <typename tIteratorType>
    struct iterator
    {
      iterator(tIteratorType& aIterator)
        : mPair(aIterator, 0)
      {

      }

      iterator& operator++()
      {
        ++mPair.first;
        ++mPair.second;

        return *this;
      }

      auto& operator*()
      {
        return mPair;
      }

      template <typename tOtherIterator>
      bool operator!=(const iterator<tOtherIterator> aRight)
      {
        return mPair.first != aRight.mPair.first;
      }

    private:
      std::pair<tIteratorType, size_t> mPair;
    };

    template <typename tIteratorBegin, typename tIteratorEnd = tIteratorBegin>
    struct range
    {
    public:
      range(tIteratorBegin aBegin, tIteratorEnd aEnd)
        : mBegin(aBegin)
        , mEnd(aEnd)
      {

      }

      bool IsRange() { return mBegin != mEnd; }

      iterator<tIteratorBegin> begin() { return mBegin; }
      iterator<tIteratorEnd> end() { return mEnd; }

    private:
      iterator<tIteratorBegin> mBegin;
      iterator<tIteratorEnd> mEnd;
    };
  }

  template <typename tType>
  auto enumerate(tType& aContainer)
  {
    detail::range<decltype(aContainer.begin()), decltype(aContainer.end())> toReturn{ aContainer.begin(), aContainer.end() };

    return toReturn;
  }


  // Functions needed to implement various operations once we have data stored.
  struct Type
  {
    using CopyConstructor = decltype(GenericDoNothing<void>)*;
    using CopyAssignment = decltype(GenericDoNothing<void>)*;
    using MoveConstructor = decltype(GenericDoNothing<void>)*;
    using MoveAssignment = decltype(GenericDoNothing<void>)*;
    using Destructor = decltype(GenericDoNothing<void>)*;

    template <typename tType>
    static Type MakeType()
    {
      Type type;

      type.mCopyConstructor = &GenericCopyConstruct<tType>;
      type.mCopyAssignment = &GenericCopyAssignment<tType>;
      type.mMoveConstructor = &GenericMoveConstruct<tType>;
      type.mMoveAssignment = &GenericMoveAssignment<tType>;
      type.mDestructor = &GenericDestructor<tType>;

      return type;
    }

    Type()
      : mCopyConstructor{ nullptr }
      , mCopyAssignment{ nullptr }
      , mMoveConstructor{ nullptr }
      , mMoveAssignment{ nullptr }
      , mDestructor{ nullptr }
    {

    }

    CopyConstructor mCopyConstructor;
    CopyAssignment mCopyAssignment;
    MoveConstructor mMoveConstructor;
    MoveAssignment mMoveAssignment;
    Destructor mDestructor;
  };

  template <typename tType>
  static Type* GetType()
  {
    static Type sType = Type::MakeType<tType>();
    return sType;
  }

  template <int SizeInBytes>
  class PrivateImplementationLocal
  {



  public:
    PrivateImplementationLocal() : mType{ nullptr }
    {
    }

    ~PrivateImplementationLocal()
    {
      // Destruct our data if it's already been constructed.
      Release();
    }

    PrivateImplementationLocal(PrivateImplementationLocal&& aRight)
    {
      if (mType)
      {
        mType->mMoveConstructor(mMemory, aRight.mMemory);
      }
    }

    PrivateImplementationLocal(PrivateImplementationLocal& aRight)
    {
      if (mType)
      {
        mType->mCopyConstructor(mMemory, aRight.mMemory);
      }
    }

    void operator=(PrivateImplementationLocal&& aRight)
    {
      if (mType)
      {
        mType->mMoveAssignment(mMemory, aRight.mMemory);
      }
    }

    void operator=(PrivateImplementationLocal& aRight)
    {
      if (mType)
      {
        mType->mCopyAssignment(mMemory, aRight.mMemory);
      }
    }

    void Release()
    {
      if (mType)
      {
        mType->mDestructor(mMemory);
      }

      mType = nullptr;
    }

    template <typename tType, typename... tArguments>
    tType* ConstructAndGet(tArguments&& ...aArguments)
    {
      static_assert(sizeof(tType) < SizeInBytes,
        "Constructed Type must be smaller than our size.");

      // Destruct any undestructed object.
      if (mType)
      {
        mType->mDestructor(mMemory);
      }

      // Capture the destructor of the new type.
      mType = nullptr;

      // Create a T in our local memory by forwarding any provided arguments.
      new (mMemory) tType(std::forward<tArguments&&>(aArguments)...);

      return Get<tType>();
    }

    template <typename tType>
    tType* Get()
    {
      if (nullptr != mType)
      {
        return nullptr;
      }

      return std::launder<tType>(reinterpret_cast<tType*>(mMemory));
    }

  private:

    byte mMemory[SizeInBytes] = {};
    Type* mType;
  };



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

  class Renderer
  {
  public:
    virtual ~Renderer() {};

    virtual void Initialize(SDL_Window*) {};

    virtual SDL_WindowFlags GetAdditionalWindowFlags() { return (SDL_WindowFlags)0; };

    virtual void NewFrame() = 0;
    virtual void ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight) = 0;

    // Some backends (dx12, vulkan) need to set descriptors before/after any calls to image
    // functions. So this is to allow for that.
    //virtual void PreImguiImage(Texture* aTexture) { (void)aTexture; };
    //virtual void PostImguiImage(Texture* aTexture) { (void)aTexture; };


    virtual std::unique_ptr<Texture> LoadTextureFromData(unsigned char* data, TextureLayout format, int w, int h, int pitch) = 0;
    std::unique_ptr<Texture> LoadTextureFromFile(std::u8string const& aFile);

    virtual GPUAllocator* MakeAllocator(std::string const& aAllocatorType, size_t aBlockSize)
    {
      (void)aAllocatorType;
      (void)aBlockSize;
      return nullptr;
    };

    virtual void ClearRenderTarget(glm::vec4 aClearColor) = 0;
    virtual void RenderImguiData() = 0;
    virtual void Present() = 0;

  protected:
    std::unordered_map<std::string, std::unique_ptr<GPUAllocator>> mAllocators;
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

    virtual ~GPUBufferBase()
    {

    }

    PrivateImplementationLocal<32>& GetData()
    {
      return mData;
    }

    virtual void Update(uint8_t const* aPointer, size_t aBytes, size_t aOffset) = 0;

  protected:
    PrivateImplementationLocal<32> mData;
    size_t mArraySize;
  };


  template <typename tType>
  class GPUBuffer
  {
  public:
    GPUBuffer()
    {

    }

    GPUBuffer(std::unique_ptr<GPUBufferBase> aBuffer)
      : mBuffer{ std::move(aBuffer) }
    {

    }

    GPUBufferBase& GetBase()
    {
      return *mBuffer;
    }

    void Update(tType const& aData)
    {
      mBuffer->Update(reinterpret_cast<uint8_t const*>(&aData), sizeof(tType), 0);
    }

    void Update(tType const* aData, size_t aSize)
    {
      mBuffer->Update(reinterpret_cast<uint8_t const*>(aData), sizeof(tType) * aSize, 0);
    }

    void Update(ContiguousRange<tType> aData)
    {
      mBuffer->Update(reinterpret_cast<uint8_t const*>(aData.begin()), sizeof(tType) * aData.size(), 0);
    }

    operator bool()
    {
      return mBuffer != nullptr;
    }

    void reset()
    {
      mBuffer.reset();
    }

    std::unique_ptr<GPUBufferBase> Steal()
    {
      return std::move(mBuffer);
    }

  private:
    std::unique_ptr<GPUBufferBase> mBuffer;
  };

  template <typename tType>
  class GPUBufferRef
  {
  public:
    GPUBufferRef()
    {

    }

    template <typename tType>
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

  struct GPUAllocator
  {
    GPUAllocator(size_t aBlockSize)
      : mBlockSize{ aBlockSize }
    {

    }
    
    virtual ~GPUAllocator()
    {

    }

    // Creates a ubo of the given type, aSize allows you to make an array of them.
    // Size must be at least 1
    template <typename tType>
    GPUBuffer<tType> CreateBuffer(size_t aSize,
      GPUAllocation::BufferUsage aUse,
      GPUAllocation::MemoryProperty aProperties)
    {
      size_t sizeOfObject = sizeof(tType) * aSize;

      auto buffer = CreateBufferInternal(sizeOfObject, aUse, aProperties);

      return GPUBuffer<tType>(std::move(buffer));
    }

    PrivateImplementationLocal<64>& GetData()
    {
      return mData;
    }

  private:
    virtual std::unique_ptr<GPUBufferBase> CreateBufferInternal(size_t aSize,
      GPUAllocation::BufferUsage aUse,
      GPUAllocation::MemoryProperty aProperties) = 0;

  protected:
    PrivateImplementationLocal<64> mData;
    size_t mBlockSize;
  };
}
