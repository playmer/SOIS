#pragma once
#include <queue>

#include "vulkan/vulkan.h"

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

    VkCommandBuffer mBuffer;
    VkFence mFence;
    VkSemaphore mAvailableSemaphore;
    VkSemaphore mFinishedSemaphore;
  };



  class VulkanQueue
  {
  public:
    VulkanQueue();

    void Initialize(vkb::Device aDevice, vkb::QueueType aType, size_t aNumberOfBuffers);


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
    void RenderImguiData() override;
    void Present() override;

    SDL_WindowFlags GetAdditionalWindowFlags() override;

    std::unique_ptr<Texture> LoadTextureFromData(unsigned char* data, TextureLayout format, int w, int h, int pitch) override;


    void RecreateSwapchain();

    VkRenderPass CreateRenderPass();


    VkFence TransitionTextures();

    std::vector<VkImage> mTexturesCreatedThisFrame;


    //void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height);
    //void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data);
    //void FramePresent(ImGui_ImplVulkanH_Window* wd);
    //void CleanupVulkan();
    //void CleanupVulkanWindow();


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

    VkSampler mFontSampler;
    VkDescriptorSetLayout mDescriptorSetLayout;

    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    std::vector<VkFramebuffer> mFramebuffers;

    VkRenderPass mRenderPass;

    SDL_Window* mWindow;

    //VkCommandPool command_pool;
    //std::vector<VkCommandBuffer> command_buffers;

    //std::vector<VkSemaphore> available_semaphores;
    //std::vector<VkSemaphore> finished_semaphore;
    //std::vector<VkFence> in_flight_fences;
    //std::vector<VkFence> image_in_flight;

    size_t mCurrentFrame = 0;
    uint32_t mImageIndex = 0;


    bool mLoadedFontTexture = false;
  };
}
