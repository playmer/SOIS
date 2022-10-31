#pragma once
#include <queue>

#include <functional>
#include "vulkan/vulkan.h"

#include "vk_mem_alloc.h"

#include "VkBootstrap.h"

#include "Renderer.hpp"

namespace SOIS
{
  struct VulkanCommandBuffer
  {
    auto operator->() const
    {
      return &mBuffer;
    }

    auto& operator&()
    {
      return mBuffer;
    }

    auto* operator&() const
    {
      return &mBuffer;
    }

    operator VkCommandBuffer()
    {
      return mBuffer;
    }

    void Begin();
    void End();

    VkCommandBuffer mBuffer;
    VkFence mFence;
    VkSemaphore mAvailableSemaphore;
    VkSemaphore mFinishedSemaphore;
  };



  class VulkanQueue
  {
  public:
    VulkanQueue();

    void Initialize(vkb::Device aDevice, vkb::QueueType aType, size_t aNumberOfBuffers, size_t aQueueIndex = 0);


    VulkanCommandBuffer WaitOnNextCommandList();
    VulkanCommandBuffer GetNextCommandList();
    VulkanCommandBuffer GetCurrentCommandList();

    auto* operator&()
    {
      return &mQueue;
    }

    auto* operator&() const
    {
      return &mQueue;
    }

    operator VkQueue()
    {
      return mQueue;
    }

    bool IsInitialized()
    {
      return (VK_NULL_HANDLE != mPool)
        && (VK_NULL_HANDLE != mCommandBuffers.back())
        && (VK_NULL_HANDLE != mFences.back())
        && (VK_NULL_HANDLE != mAvailableSemaphores.back())
        && (VK_NULL_HANDLE != mFinishedSemaphore.back());
    }

    uint32_t GetQueueFamily();

    void Submit(VulkanCommandBuffer aCommandList);

  private:

    vkb::Device mDevice;
    VkQueue mQueue = VK_NULL_HANDLE;
    VkCommandPool mPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> mCommandBuffers;
    std::vector<VkFence> mFences;
    std::vector<VkSemaphore> mAvailableSemaphores;
    std::vector<VkSemaphore> mFinishedSemaphore;
    std::vector<bool> mUsed; // lmao vector bool
    size_t mCurrentBuffer = 0;
    vkb::QueueType mType;
  };













  class VulkanRenderer : public Renderer
  {
  public:
    VulkanRenderer();
    ~VulkanRenderer() override;

    void Initialize(SDL_Window* aWindow, char8_t const* aPreferredGpu) override;

    void NewFrame() override;
    void ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight) override;

    void ClearRenderTarget(glm::vec4 aClearColor) override;

    void Upload() override {}
    void RenderImguiData() override;
    void Present() override;

    SDL_WindowFlags GetAdditionalWindowFlags() override;

    std::unique_ptr<Texture> LoadTextureFromData(unsigned char* data, TextureLayout format, int w, int h, int pitch) override { return nullptr;  };


    void RecreateSwapchain();

    VkRenderPass CreateRenderPass();


    VkFence TransitionTextures();

    //struct TextureTransferData
    //{
    //  VkImage mImage;
    //  VkBuffer mUploadBuffer;
    //  VmaAllocation mUploadBufferAllocation;
    //};
    //
    //std::vector<TextureTransferData> mTexturesCreatedThisFrame;


    struct TextureDestroyer
    {
      VkImage mImage;
      VkDescriptorSet mDescriptorSet;
      VmaAllocation mImageAllocation;
    };

    std::vector<TextureDestroyer> mTexturesToDestroyNextFrame;


    std::future<std::unique_ptr<Texture>> LoadTextureFromDataAsync(unsigned char* data, TextureLayout format, int w, int h, int pitch) override;
    void UploadThread();
    struct UploadJob
    {
      struct Texture
      {
        std::promise<std::unique_ptr<SOIS::Texture>> mTexturePromise;
        VkImage mImage;
        VkBuffer mUploadBuffer;
        VmaAllocation  mUploadBufferAllocation;
        VkDescriptorSet mDescriptorSet;
        VmaAllocation mImageAllocation;
        int mWidth;
        int mHeight;
      };

      struct TextureTransition
      {
        std::promise<std::unique_ptr<SOIS::Texture>> mTexturePromise;
        VkImage mImage;
        VkBuffer mUploadBuffer;
        VmaAllocation  mUploadBufferAllocation;
        VkDescriptorSet mDescriptorSet;
        VmaAllocation mImageAllocation;
        int mWidth;
        int mHeight;
      };

      struct Buffer
      {
        VkBuffer mUploadBuffer;
        VmaAllocation mUploadBufferAllocation;
      };

      UploadJob(
        VulkanRenderer* aRenderer,
        std::promise<std::unique_ptr<SOIS::Texture>> texturePromise,
        VkImage image,
        VkBuffer uploadBuffer,
        VmaAllocation  uploadBufferAllocation,
        VkDescriptorSet descriptorSet,
        VmaAllocation imageAllocation,
        int aWidth,
        int aHeight);

      UploadJob(
        VulkanRenderer* aRenderer,
        TextureTransition aTextureTransition);

      UploadJob(VulkanRenderer* aRenderer, VkBuffer aUploadBuffer, VmaAllocation aUploadBufferAllocation);

      UploadJob(UploadJob&&) = default;
      UploadJob(UploadJob&) = default;

      std::optional<UploadJob> operator()(VulkanCommandBuffer aCommandBuffer);
      void FulfillPromise();

      void TextureUpload(VulkanCommandBuffer aCommandBuffer, Texture* aTexture);
      void BufferUpload(VulkanCommandBuffer aCommandBuffer, Buffer* aBuffer);
      void TextureTransitionTask(VulkanCommandBuffer aCommandBuffer, TextureTransition* aTextureTransition);

      VulkanRenderer* mRenderer;
      std::variant<Texture, TextureTransition, Buffer> mVariant;
    };

    friend UploadJob;
    std::vector<UploadJob> mUploadJobs;
    std::mutex mUploadJobsMutex;
    std::counting_semaphore<std::numeric_limits<std::ptrdiff_t>::max()> mUploadJobsWakeUp;

    static constexpr uint32_t cMinImageCount = 3;

    vkb::Instance mInstance;
    VkSurfaceKHR mSurface;
    vkb::PhysicalDevice mPhysicalDevice;
    vkb::Device mDevice;
    VkPipelineCache mPipelineCache = VK_NULL_HANDLE;
    VulkanQueue mTransferQueue;
    VulkanQueue mTextureTransitionQueue;
    VulkanQueue mGraphicsQueue;
    VulkanQueue mPresentQueue;
    vkb::Swapchain mSwapchain;
    VkDescriptorPool mDescriptorPool;
    VkClearColorValue mClearColor;

    VmaAllocator mAllocator;

    VkSampler mFontSampler;
    VkDescriptorSetLayout mDescriptorSetLayout;

    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    std::vector<VkFramebuffer> mFramebuffers;

    VkRenderPass mRenderPass;

    SDL_Window* mWindow;

    size_t mCurrentFrame = 0;
    uint32_t mImageIndex = 0;


    bool mLoadedFontTexture = false;
  };
}
