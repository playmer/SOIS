
#include "SOIS/DX11Renderer.hpp"

namespace SOIS
{
  std::unique_ptr<Renderer> MakeDX11Renderer()
  {
    return std::make_unique<DX11Renderer>();
  }
}