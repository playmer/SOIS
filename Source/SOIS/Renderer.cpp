#include "Renderer.hpp"

#include <stb_image.h>

namespace SOIS
{
  void GPUCommandList::SetRenderState(RenderStateCommand aRenderState)
  {
    mCommands.emplace_back(RenderStateCommand(aRenderState));
  }

  void GPUCommandList::BindVertexBuffer(std::vector<GPUBufferBase> aGPUBuffers)
  {
    mCommands.emplace_back(BindVertexBufferCommand(std::move(aGPUBuffers)));
  }

  void GPUCommandList::BindIndexBuffer(GPUBufferBase aGPUBuffer)
  {
    mCommands.emplace_back(BindIndexBufferCommand(aGPUBuffer));
  }

  void GPUCommandList::BindPipeline(GPUPipeline aPipeline)
  {
    mCommands.emplace_back(BindPipelineCommand(aPipeline));
  }


  std::future<std::unique_ptr<Texture>> Renderer::LoadTextureFromFileAsync(std::u8string const& aFile)
  {
    // Load from disk into a raw RGBA buffer
    std::vector<char> imageFileData;
    SDL_RWops* io = SDL_RWFromFile((char const*)aFile.c_str(), "rb");
    if (io != nullptr)
    {
      // Seek to 0 bytes from the end of the file
      Sint64 length = SDL_RWseek(io, 0, RW_SEEK_END);
      SDL_RWseek(io, 0, RW_SEEK_SET);
      imageFileData.resize(length);
      SDL_RWread(io, imageFileData.data(), length, 1);
      SDL_RWclose(io);
    }

    int width = 0;
    int height = 0;
    unsigned char* data = stbi_load_from_memory((stbi_uc*)imageFileData.data(), static_cast<int>(imageFileData.size()), &width, &height, NULL, 4);
    if (data == NULL)
    {
      std::promise<std::unique_ptr<Texture>> p;
      std::future<std::unique_ptr<Texture>> empty = p.get_future();
      p.set_value(nullptr);
      return empty;
    }

    auto texture = LoadTextureFromDataAsync(data, SOIS::TextureLayout::RGBA_Unorm, width, height, width * 4);

    stbi_image_free(data);

    return std::move(texture);
  }



  std::future<std::unique_ptr<Texture>> Renderer::LoadTextureFromDataAsync(unsigned char* data, TextureLayout format, int w, int h, int pitch)
  {
    std::promise<std::unique_ptr<Texture>> p;
    std::future<std::unique_ptr<Texture>> empty = p.get_future();
    p.set_value(nullptr);
    return empty;
  };
}
