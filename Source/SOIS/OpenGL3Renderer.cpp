

#include <vector>

#include <glbinding/gl45/gl.h>
#include <glbinding/glbinding.h>


#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include <stb_image.h>

#include "SOIS/OpenGL3Renderer.hpp"

namespace SOIS
{
  struct OpenGL45GPUPiplineData
  {
    gl::GLuint mShaderProgram;
    gl::GLuint mVertexArrayObject;
  };

  struct OpenGL45GPUBufferData
  {
    gl::GLuint mBuffer;
  };

  static char const* Source(gl::GLenum source)
  {
    switch (source)
    {
    case gl::GL_DEBUG_SOURCE_API: return "DEBUG_SOURCE_API";
    case gl::GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "DEBUG_SOURCE_WINDOW_SYSTEM";
    case gl::GL_DEBUG_SOURCE_SHADER_COMPILER: return "DEBUG_SOURCE_SHADER_COMPILER";
    case gl::GL_DEBUG_SOURCE_THIRD_PARTY: return "DEBUG_SOURCE_THIRD_PARTY";
    case gl::GL_DEBUG_SOURCE_APPLICATION: return "DEBUG_SOURCE_APPLICATION";
    case gl::GL_DEBUG_SOURCE_OTHER: return "DEBUG_SOURCE_OTHER";
    default: return "unknown";
    }
  }

  static char const* Severity(gl::GLenum severity)
  {
    switch (severity)
    {
    case gl::GL_DEBUG_SEVERITY_HIGH: return "DEBUG_SEVERITY_HIGH";
    case gl::GL_DEBUG_SEVERITY_MEDIUM: return "DEBUG_SEVERITY_MEDIUM";
    case gl::GL_DEBUG_SEVERITY_LOW: return "DEBUG_SEVERITY_LOW";
    case gl::GL_DEBUG_SEVERITY_NOTIFICATION: return "DEBUG_SEVERITY_NOTIFICATION";
    default: return "unknown";
    }
  }


  static char const* Type(gl::GLenum type)
  {
    switch (type)
    {
    case gl::GL_DEBUG_TYPE_ERROR: return "DEBUG_TYPE_ERROR";
    case gl::GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "DEBUG_TYPE_DEPRECATED_BEHAVIOR";
    case gl::GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "DEBUG_TYPE_UNDEFINED_BEHAVIOR";
    case gl::GL_DEBUG_TYPE_PORTABILITY: return "DEBUG_TYPE_PORTABILITY";
    case gl::GL_DEBUG_TYPE_PERFORMANCE: return "DEBUG_TYPE_PERFORMANCE";
    case gl::GL_DEBUG_TYPE_MARKER: return "DEBUG_TYPE_MARKER";
    case gl::GL_DEBUG_TYPE_PUSH_GROUP: return "DEBUG_TYPE_PUSH_GROUP";
    case gl::GL_DEBUG_TYPE_POP_GROUP: return "DEBUG_TYPE_POP_GROUP";
    case gl::GL_DEBUG_TYPE_OTHER: return "DEBUG_TYPE_OTHER";
    default: return "unknown";
    }
  }

  static
    void GL_APIENTRY messageCallback(gl::GLenum source,
      gl::GLenum type,
      gl::GLuint id,
      gl::GLenum severity,
      gl::GLsizei length,
      const gl::GLchar* message,
      const void* userParam)
  {
    (void)id; (void)length; (void)userParam;

    if (gl::GL_DEBUG_SEVERITY_NOTIFICATION == severity)
    {
      return;
    }

    printf("GL DEBUG CALLBACK:\n    Source = %s\n    type = %s\n    severity = %s\n    message = %s\n",
      Source(source),
      Type(type),
      Severity(severity),
      message);
  }

  static gl::GLenum FromSOIS(TextureLayout aLayout)
  {
    switch (aLayout)
    {
    case TextureLayout::RGBA_Unorm: return gl::GLenum::GL_RGBA;
    case TextureLayout::RGBA_Srgb: return gl::GLenum::GL_RGBA;
    case TextureLayout::Bc1_Rgba_Srgb: return gl::GLenum::GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
    case TextureLayout::Bc3_Srgb: return gl::GLenum::GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;

    case TextureLayout::Bc1_Rgba_Unorm: //return DXGI_FORMAT_A8_UNORM;
    case TextureLayout::Bc3_Unorm: //return DXGI_FORMAT_BC3_UNORM;
    case TextureLayout::Bc7_Unorm: //return DXGI_FORMAT_BC7_UNORM;
    case TextureLayout::Bc7_Srgb: //return DXGI_FORMAT_BC7_UNORM_SRGB;
    default:
    case TextureLayout::InvalidLayout: return (gl::GLenum)0;
    }
  }

  glbinding::ProcAddress GLFunctionLoader(const char* aName)
  {
    return reinterpret_cast<glbinding::ProcAddress>(SDL_GL_GetProcAddress(aName));
  }

  class OpenGL3Texture : public Texture
  {
  public:
    OpenGL3Texture(gl::GLuint aTextureHandle, int aWidth, int aHeight)
      : Texture{ aWidth, aHeight }
      , mTextureHandle{ aTextureHandle }
    {

    }

    ~OpenGL3Texture() override
    {
    }

    virtual ImTextureID GetTextureId()
    {
      return ImTextureID{ (void*)mTextureHandle };
    };

    gl::GLuint mTextureHandle;
  };

    // GL 4.1 + GLSL 400
  static const char* gGlslVersion = "#version 400";

  OpenGL45Renderer::OpenGL45Renderer()
    : Renderer{}
  {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  }


  SDL_WindowFlags OpenGL45Renderer::GetAdditionalWindowFlags()
  {
    return SDL_WINDOW_OPENGL;
  }


  void OpenGL45Renderer::Initialize(SDL_Window* aWindow, char8_t const* /*aPreferredGpu*/)
  {
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    mWindow = aWindow;

    mContext = SDL_GL_CreateContext(mWindow);
    mUploadContext = SDL_GL_CreateContext(mWindow);
    SDL_GL_MakeCurrent(mWindow, mContext);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Initialize OpenGL loader
    glbinding::initialize(GLFunctionLoader);

    gl::glEnable(gl::GL_DEBUG_OUTPUT);

    // FIXME: This doesn't work on Apple when I tested it, need to look into this more on 
    // other platforms, and maybe only enable it in dev builds.
    #if defined(_WIN32)
        gl::glDebugMessageCallback(messageCallback, this);
    #endif

    ImGui_ImplSDL2_InitForOpenGL(mWindow, mContext);
    ImGui_ImplOpenGL3_Init(gGlslVersion);

    mUploadThread = std::thread([this]()
      {
        UploadThread();
      });
  }



  OpenGL45Renderer::~OpenGL45Renderer()
  {
    mShouldJoin = true;
    mUploadJobsWakeUp.release();
    mUploadThread.join();

    ImGui_ImplOpenGL3_Shutdown();
  }


  void OpenGL45Renderer::UploadThread()
  {
    struct UploadedJob
    {
      std::promise<std::unique_ptr<SOIS::Texture>> mTexturePromise;
      gl::GLsync mSync;
      gl::GLuint mTexture;
      int mWidth;
      int mHeight;
    };

    SDL_GL_MakeCurrent(mWindow, mUploadContext);
    std::vector<UploadJob> uploads;
    std::vector<UploadedJob> uploadedJobs;
    //std::vector<UploadJob> transitions;

    while (!mShouldJoin)
    {
      //// Aquire Job
      mUploadJobsMutex.lock();
      std::swap(uploads, mUploadJobs);
      mUploadJobsMutex.unlock();

      for (auto& upload : uploads)
      {

        // Create a OpenGL texture identifier
        gl::GLuint image_texture;
        gl::glGenTextures(1, &image_texture);
        gl::glBindTexture(gl::GL_TEXTURE_2D, image_texture);

        // Setup filtering parameters for display
        gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR);
        gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MAG_FILTER, gl::GL_LINEAR);
        gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
        gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE); // Same

        // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
        auto glFormat = FromSOIS(upload.mFormat);

        gl::glTexImage2D(
          gl::GLenum::GL_TEXTURE_2D, 
          0, 
          glFormat, 
          upload.mWidth, 
          upload.mHeight, 
          0, 
          glFormat, 
          gl::GLenum::GL_UNSIGNED_BYTE,
          static_cast<void*>(upload.mStoredTextureData.data()));
        

        uploadedJobs.emplace_back(
          std::move(upload.mTexturePromise),
          gl::glFenceSync(gl::GL_SYNC_GPU_COMMANDS_COMPLETE, 0),
          image_texture,
          upload.mWidth,
          upload.mHeight
        );
      }

      uploads.clear();

      // When calling glClientWaitSync, we need to ensure that the sync object has been placed into the GPU Command Queue,
      // otherwise we may end up in an infinite loop. glFlush ensures it gets there. We could potentially pass 
      // GL_SYNC_FLUSH_COMMANDS_BIT to glClientWaitSync, but as we're about to enter a waiting loop, it's easier to 
      // simply flush all the sync commands to the GPU and then wait on them without needed to carefully juggle if we've
      // passed that flag to glClientWaitSync _only_ the first time.
      // Reference Note: https://www.khronos.org/opengl/wiki/Sync_Object#Flushing_and_contexts

      gl::glFlush();

      while (uploadedJobs.size())
      {
        for (auto uploadedIt = uploadedJobs.begin(); uploadedIt < uploadedJobs.end();)
        {
          gl::GLenum signalStatus = gl::glClientWaitSync(uploadedIt->mSync, gl::SyncObjectMask::GL_NONE_BIT, 1000000);
          if ((gl::GL_ALREADY_SIGNALED  == signalStatus)
              || (gl::GL_CONDITION_SATISFIED == signalStatus))
          {
            auto texture = std::make_unique<OpenGL3Texture>(uploadedIt->mTexture, uploadedIt->mWidth, uploadedIt->mHeight);

            uploadedIt->mTexturePromise.set_value(std::unique_ptr<Texture>(texture.release()));
            uploadedIt = uploadedJobs.erase(uploadedIt);
          }
          else
          {
            ++uploadedIt;
          }
        }
      }

      mUploadJobsWakeUp.acquire();
    }
  }

  void OpenGL45Renderer::NewFrame()
  {
    ImGui_ImplOpenGL3_NewFrame();
  }

  void OpenGL45Renderer::ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight)
  {
    (void)aWidth; (void)aHeight;
    // Nothing special here, taken care of when we clear the render target.
  }

  void OpenGL45Renderer::ClearRenderTarget(glm::vec4 aClearColor)
  {
    // Clear the viewport to prepare for user rendering.
    SDL_GL_MakeCurrent(mWindow, mContext);
    ImGuiIO& io = ImGui::GetIO();
    gl::glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    gl::glClearColor(aClearColor.x, aClearColor.y, aClearColor.z, aClearColor.w);
    gl::glClear(gl::GL_COLOR_BUFFER_BIT);
  }

  void OpenGL45Renderer::RenderImguiData()
  {
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  }

  void OpenGL45Renderer::Present()
  {
    // Swap the buffers and prepare for next frame.
    SDL_GL_MakeCurrent(mWindow, mContext);
    SDL_GL_SwapWindow(mWindow);

  } 

  std::future<std::unique_ptr<Texture>> OpenGL45Renderer::LoadTextureFromDataAsync(unsigned char* data, TextureLayout format, int aWidth, int aHeight, int pitch)
  {
    std::vector<unsigned char> storedData;
    storedData.resize(aHeight * pitch);
    memcpy(storedData.data(), data, storedData.size());

    std::promise<std::unique_ptr<SOIS::Texture>> texturePromise;
    std::future<std::unique_ptr<SOIS::Texture>> textureFuture = texturePromise.get_future();

    {
      std::unique_lock lock{ mUploadJobsMutex };
      mUploadJobs.emplace_back(
        std::move(texturePromise),
        std::move(storedData),
        format,
        aWidth,
        aHeight
      );
    }
    mUploadJobsWakeUp.release(1);

    return textureFuture;
  }


  void OpenGL45Renderer::CommandVisitor::operator()(RenderStateCommand& aJob)
  {
    //gl::glViewport(0, 0, width, height);
    //gl::glClearColor(mClearColor.r * mClearColor.a, mClearColor.g * mClearColor.a, mClearColor.b * mClearColor.a, mClearColor.a);
    //gl::glClear(GL_COLOR_BUFFER_BIT);



    //glDrawArrays(GL_TRIANGLES, 0, 3);
  }
  void OpenGL45Renderer::CommandVisitor::operator()(BindVertexBufferCommand& aJob)
  {
    for (auto& buffer : aJob.mGPUBuffers)
    {
      auto bufferId = GetDataFromGPUObject(buffer).Get<OpenGL45GPUBufferData>()->mBuffer;
      gl::glVertexArrayVertexBuffer(mCurrentVertexArrayObject, 0, bufferId, 0, 32);
    }
  }

  void OpenGL45Renderer::CommandVisitor::operator()(BindIndexBufferCommand& aJob)
  {
    auto pipelineData = GetDataFromGPUObject(aJob.mGPUBuffer).Get<OpenGL45GPUBufferData>();
    gl::glVertexArrayElementBuffer(mCurrentVertexArrayObject, pipelineData->mBuffer);
  }

  void OpenGL45Renderer::CommandVisitor::operator()(BindPipelineCommand& aJob)
  {
    auto pipelineData = GetDataFromGPUObject(aJob.mPipeline).Get<OpenGL45GPUPiplineData>();
    gl::glUseProgram(pipelineData->mShaderProgram);
    gl::glBindVertexArray(pipelineData->mVertexArrayObject);
    mCurrentVertexArrayObject = pipelineData->mVertexArrayObject;
  }

  void OpenGL45Renderer::CommandVisitor::operator()(DrawCommand& aJob)
  {
    gl::glDrawElements(gl::GL_TRIANGLES, aJob.mIndexCount, gl::GL_UNSIGNED_INT, nullptr);
  }

  void OpenGL45Renderer::ExecuteCommandList(GPUCommandList& aList)
  {
    CommandVisitor visitor{ this };
    for (auto& command : GetCommandsFromList(aList))
    {
      std::visit(visitor, command);
    }
  }

  //GPUAllocator* OpenGL45Renderer::MakeAllocator(std::string const& aAllocatorType, size_t aBlockSize)
  //{
  //  auto it = mAllocators.find(aAllocatorType);
  //
  //  if (it != mAllocators.end())
  //  {
  //    return it->second.get();
  //  }
  //
  //  auto allocator = std::make_unique<OpenGL41GPUAllocator>(aAllocatorType, aBlockSize, this);
  //
  //  auto ptr = allocator.get();
  //
  //  mAllocators.emplace(aAllocatorType, std::move(allocator));
  //
  //  return ptr;
  //}
  //
  //std::unique_ptr<GPUBufferBase> OpenGL41GPUAllocator::CreateBufferInternal(
  //  size_t aSize,
  //  GPUAllocation::BufferUsage aUse,
  //  GPUAllocation::MemoryProperty aProperties)
  //{
  //
  //}
}
