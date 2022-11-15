
#include "SOIS/OpenGL3Renderer.hpp"

namespace SOIS
{
  std::unique_ptr<Renderer> MakeOpenGL3Renderer()
  {
    return std::make_unique<OpenGL45Renderer>();
  }
}
