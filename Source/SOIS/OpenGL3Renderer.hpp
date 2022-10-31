#pragma once


#include <SDL.h> // Include glfw3.h after our OpenGL definitions

#include "SOIS/Renderer.hpp"

namespace SOIS
{
  class OpenGL3Renderer : public Renderer
  {
  public:
    OpenGL3Renderer();
    ~OpenGL3Renderer() override;

    void Initialize(SDL_Window* aWindow, char8_t const* /*aPreferredGpu*/) override;

    SDL_WindowFlags GetAdditionalWindowFlags() override;

    void NewFrame() override;
    void ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight) override;

    void ClearRenderTarget(glm::vec4 aClearColor) override;
    void RenderImguiData() override;
    void Present() override;


    void UploadThread();

    std::unique_ptr<Texture> LoadTextureFromData(unsigned char* data, TextureLayout format, int w, int h, int pitch) override;

    std::future<std::unique_ptr<Texture>> LoadTextureFromDataAsync(unsigned char* data, TextureLayout format, int w, int h, int pitch) override;

  private:
    struct UploadJob
    {
      std::vector<unsigned char> mStoredTextureData;
      TextureLayout format;
      int mWidth;
      int mHeight;
    };

    std::vector<UploadJob> mUploadJobs;


    std::mutex mUploadJobsMutex;
    std::counting_semaphore<std::numeric_limits<std::ptrdiff_t>::max()> mUploadJobsWakeUp;


    SDL_GLContext mContext;
    SDL_GLContext mUploadContext;
    SDL_Window* mWindow = nullptr;
  };
}
