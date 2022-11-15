#pragma once


#include <SDL.h> // Include glfw3.h after our OpenGL definitions
#include <glbinding/gl41/gl.h>

#include "SOIS/Renderer.hpp"

namespace SOIS
{
  class OpenGL45Renderer : public Renderer
  {
  public:
    OpenGL45Renderer();
    ~OpenGL45Renderer() override;

    void Initialize(SDL_Window* aWindow, char8_t const* /*aPreferredGpu*/) override;

    SDL_WindowFlags GetAdditionalWindowFlags() override;

    void NewFrame() override;

    //GPUAllocator* MakeAllocator(std::string const& aAllocatorType, size_t aBlockSize) override;
    void ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight) override;
    void ClearRenderTarget(glm::vec4 aClearColor) override;

    void RenderImguiData() override;
    void Present() override;
    void ExecuteCommandList(GPUCommandList& aList);

    void UploadThread();

    std::future<std::unique_ptr<Texture>> LoadTextureFromDataAsync(unsigned char* data, TextureLayout format, int w, int h, int pitch) override;

  private:
    struct CommandVisitor
    {
      CommandVisitor(OpenGL45Renderer* aRenderer)
        : mRenderer{ aRenderer }
      {

      }

      void operator()(RenderStateCommand& aJob);
      void operator()(BindVertexBufferCommand& aJob);
      void operator()(BindIndexBufferCommand& aJob);
      void operator()(BindPipelineCommand& aJob);
      void operator()(DrawCommand& aJob);


      OpenGL45Renderer* mRenderer;
      gl::GLuint mCurrentVertexArrayObject = 0;
    };


    struct UploadJob
    {
      std::promise<std::unique_ptr<SOIS::Texture>> mTexturePromise;
      std::vector<unsigned char> mStoredTextureData;
      TextureLayout mFormat;
      int mWidth;
      int mHeight;
    };

    std::vector<UploadJob> mUploadJobs;


    SDL_GLContext mContext;
    SDL_GLContext mUploadContext;
    SDL_Window* mWindow = nullptr;
  };

  //class OpenGL41GPUAllocator : public GPUAllocator
  //{
  //public:
  //  OpenGL41GPUAllocator(std::string const& aAllocatorType, size_t aBlockSize, OpenGL45Renderer* aRenderer);
  //  std::unique_ptr<GPUBufferBase> CreateBufferInternal(
  //    size_t aSize,
  //    GPUAllocation::BufferUsage aUse,
  //    GPUAllocation::MemoryProperty aProperties) override;
  //};
}
