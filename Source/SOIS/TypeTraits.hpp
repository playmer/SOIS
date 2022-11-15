#pragma once

#include <type_traits>

#include <cstddef>
#include <cstdio>
#include <memory>

namespace SOIS
{

#ifdef WIN32
  #define DEBUG_BREAK __debugbreak
#elif defined(__APPLE__)
  #include <signal.h>
  #define DEBUG_BREAK() raise(SIGTRAP)
#else
  #include <signal.h>
  #define DEBUG_BREAK() raise(SIGTRAP)
#endif

  inline void runtime_assert(bool value, char const* message)
  {
    if (!value)
    {
      printf("%s\nAborting...", message);
      DEBUG_BREAK();
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

    size_t size() const { return mEnd - mBegin; }
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
}
