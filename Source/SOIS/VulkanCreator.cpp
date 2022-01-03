#include "SOIS/VulkanRenderer.hpp"

namespace SOIS
{
  std::unique_ptr<Renderer> MakeVulkanRenderer()
  {
    return std::make_unique<VulkanRenderer>();
  }
}
