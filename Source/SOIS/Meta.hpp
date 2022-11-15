#pragma once

#include <cstdint>

#include "TypeTraits.hpp"

namespace SOIS
{
  // Functions needed to implement various operations once we have data stored.
  struct Type
  {
    using CopyConstructor = decltype(GenericCopyConstruct<void>)*;
    using CopyAssignment = decltype(GenericCopyAssignment<void>)*;
    using MoveConstructor = decltype(GenericMoveConstruct<void>)*;
    using MoveAssignment = decltype(GenericMoveAssignment<void>)*;
    using Destructor = decltype(GenericDestructor<void>)*;

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
    return &sType;
  }
}