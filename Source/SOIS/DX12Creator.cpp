
#include "SOIS/DX12Renderer.hpp"

namespace SOIS
{
  std::unique_ptr<Renderer> MakeDX12Renderer()
  {
    return std::make_unique<DX12Renderer>();
  }
}
