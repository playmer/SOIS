#pragma once

#include <cstdint>

#include "Meta.hpp"
#include "Types.hpp"

namespace SOIS
{
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
        mType->mDestructor((void*)mMemory);
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
}
