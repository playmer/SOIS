#include "SDL.h"
#include "SDL_vulkan.h"

#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"

#include "VkBootstrap.h"

#include "VulkanRenderer.hpp"

#ifdef WIN32
  #define DEBUG_BREAK __debugbreak
#elif defined(__APPLE__)
  #include <signal.h>
  #define raise(SIGTRAP)
#else
  #include <signal.h>
  #define raise(SIGTRAP)
#endif

namespace SOIS
{
  //////////////////////////////////////////////////////////////////////////////////////////////
  // Helpers:
  static void check_vk_result(VkResult err)
  {    
    if (err == 0)
      return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
      abort();
  }

  //////////////////////////////////////////////////////////////////////////////////////////////
  // Queue:
  VulkanQueue::VulkanQueue()
  {
  }

  void VulkanQueue::Initialize(vkb::Device aDevice, vkb::QueueType aType, size_t aNumberOfBuffers)
  {
    mDevice = aDevice;
    mCommandBuffers.resize(aNumberOfBuffers, VK_NULL_HANDLE);
    mFences.resize(aNumberOfBuffers, VK_NULL_HANDLE);
    mAvailableSemaphores.resize(aNumberOfBuffers, VK_NULL_HANDLE);
    mFinishedSemaphore.resize(aNumberOfBuffers, VK_NULL_HANDLE);
    mUsed.resize(aNumberOfBuffers, false);
    mType = aType;

    auto queue_ret = mDevice.get_queue(aType);
    if (!queue_ret) {
      printf("Failed to create Vulkan Queue. Error: %s\n", queue_ret.error().message().c_str());
    }
    mQueue = queue_ret.value();

    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = mDevice.get_queue_index(aType).value();
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    mPool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(mDevice.device, &pool_info, mDevice.allocation_callbacks, &mPool) != VK_SUCCESS) {
      printf("failed to create command pool\n");
      return;
    }

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = mPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)mCommandBuffers.size();

    if (vkAllocateCommandBuffers(mDevice.device, &allocInfo, mCommandBuffers.data()) != VK_SUCCESS) {
      printf("Failed to allocate command buffers\n");
      return;
    }

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& fence : mFences)
    {
      if (vkCreateFence(mDevice.device, &fence_info, nullptr, &fence) != VK_SUCCESS) {
        printf("Failed to create synchronization objects\n");
        return;
      }
    }

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (auto& semaphore : mAvailableSemaphores)
    {
      if (vkCreateSemaphore(mDevice.device, &semaphore_info, nullptr, &semaphore) != VK_SUCCESS) {
        printf("Failed to create synchronization objects\n");
        return;
      }
    }

    for (auto& semaphore : mFinishedSemaphore)
    {
      if (vkCreateSemaphore(mDevice.device, &semaphore_info, nullptr, &semaphore) != VK_SUCCESS) {
        printf("Failed to create synchronization objects\n");
        return;
      }
    }
  }


  uint32_t VulkanQueue::GetQueueFamily()
  {
    return mDevice.get_queue_index(mType).value();
  }


  VulkanCommandBuffer VulkanQueue::WaitOnNextCommandList()
  {
    mCurrentBuffer = (1 + mCurrentBuffer) % mCommandBuffers.size();

    if (mUsed[mCurrentBuffer])
    {
      vkWaitForFences(mDevice.device, 1, &mFences[mCurrentBuffer], true, UINT64_MAX);
      vkResetFences(mDevice, 1, &mFences[mCurrentBuffer]);
      vkResetCommandBuffer(mCommandBuffers[mCurrentBuffer], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    }
    else
    {
      vkResetFences(mDevice, 1, &mFences[mCurrentBuffer]);
      mUsed[mCurrentBuffer] = true;
    }

    return VulkanCommandBuffer{ mCommandBuffers[mCurrentBuffer], mFences[mCurrentBuffer], mAvailableSemaphores[mCurrentBuffer], mFinishedSemaphore[mCurrentBuffer] };
  }


  VulkanCommandBuffer VulkanQueue::GetNextCommandList()
  {
    mCurrentBuffer = (1 + mCurrentBuffer) % mCommandBuffers.size();

    if (mUsed[mCurrentBuffer])
    {
      vkResetCommandBuffer(mCommandBuffers[mCurrentBuffer], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    }
    else
    {
      mUsed[mCurrentBuffer] = true;
    }

    return VulkanCommandBuffer{ mCommandBuffers[mCurrentBuffer], mFences[mCurrentBuffer], mAvailableSemaphores[mCurrentBuffer], mFinishedSemaphore[mCurrentBuffer]};
  }


  VulkanCommandBuffer VulkanQueue::GetCurrentCommandList()
  {
    return VulkanCommandBuffer{ mCommandBuffers[mCurrentBuffer], mFences[mCurrentBuffer], mAvailableSemaphores[mCurrentBuffer], mFinishedSemaphore[mCurrentBuffer] };
  }






  //////////////////////////////////////////////////////////////////////////////////////////////
  // Renderer
  VulkanRenderer::VulkanRenderer()
  {

  }

  VulkanRenderer::~VulkanRenderer()
  {
    auto err = vkDeviceWaitIdle(mDevice);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
  }

  SDL_WindowFlags VulkanRenderer::GetAdditionalWindowFlags()
  {
    return SDL_WINDOW_VULKAN;
  }

  VkBool32 DebugUtilsCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
  {
    auto severity = vkb::to_string_message_severity(messageSeverity);
    auto type = vkb::to_string_message_type(messageType);

    if ((VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT & messageSeverity)
      || (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT & messageSeverity))
    {
      printf("[%s: %s] %s\n", severity, type, pCallbackData->pMessage);
      DEBUG_BREAK();
    }

    return VK_FALSE;
  }

  void VulkanRenderer::Initialize(SDL_Window* aWindow)
  {
    mWindow = aWindow;

    ///////////////////////////////////////
    // Create Instance
    vkb::InstanceBuilder instance_builder;
    instance_builder
      .set_app_name("Application")
      .set_engine_name("SOIS")
      .require_api_version(1, 0, 0)
      .set_debug_callback(&DebugUtilsCallback);

    auto system_info_ret = vkb::SystemInfo::get_system_info();
    if (!system_info_ret) {
      printf("%s\n", system_info_ret.error().message().c_str());
      return;
    }

    auto system_info = system_info_ret.value();
    //if (system_info.is_layer_available("VK_LAYER_LUNARG_api_dump")) {
    //  instance_builder.enable_layer("VK_LAYER_LUNARG_api_dump");
    //}
    if (system_info.validation_layers_available) {
      instance_builder.enable_validation_layers();
    }

    auto instance_builder_return = instance_builder.build();

    if (!instance_builder_return) {
      printf("Failed to create Vulkan instance. Error: %s\n", instance_builder_return.error().message().c_str());
      return;
    }
    mInstance = instance_builder_return.value();

    ///////////////////////////////////////
    // Create Surface
    SDL_bool err = SDL_Vulkan_CreateSurface(aWindow, mInstance.instance, &mSurface);
    if (!err) {
      printf("Failed to create Vulkan Surface.\n");
      return;
    }

    ///////////////////////////////////////
    // Select Physical Device
    vkb::PhysicalDeviceSelector phys_device_selector(mInstance);
    auto physical_device_selector_return = phys_device_selector
      .set_surface(mSurface)
      .select();
    if (!physical_device_selector_return) {
      printf("Failed to create Vulkan Surface. Error: %s\n", physical_device_selector_return.error().message().c_str());
    }
    mPhysicalDevice = physical_device_selector_return.value();

    ///////////////////////////////////////
    // Create Logical Device
    vkb::DeviceBuilder device_builder{ mPhysicalDevice };
    auto dev_ret = device_builder.build();
    if (!dev_ret) {
      // error
    }
    mDevice = dev_ret.value();

    ///////////////////////////////////////
    // Create Queues
    mTransferQueue.Initialize(mDevice, vkb::QueueType::transfer, 30);
    mTextureTransitionQueue.Initialize(mDevice, vkb::QueueType::graphics, cMinImageCount);
    mGraphicsQueue.Initialize(mDevice, vkb::QueueType::graphics, cMinImageCount);
    mPresentQueue.Initialize(mDevice, vkb::QueueType::present, cMinImageCount);

    ///////////////////////////////////////
    // Create Swapchain
    vkb::SwapchainBuilder swapchain_builder{ mDevice, mSurface };
    swapchain_builder.set_desired_format(VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR });
    auto swap_ret = swapchain_builder.build();
    if (!swap_ret) {
      printf("Failed to create Vulkan Swapchain. Error: %s\n", swap_ret.error().message().c_str());
    }
    mSwapchain = swap_ret.value();

    ///////////////////////////////////////
    // Create Descriptor Pool
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    auto descriptorPoolError = vkCreateDescriptorPool(mDevice, &pool_info, mInstance.allocation_callbacks, &mDescriptorPool);
    check_vk_result(descriptorPoolError);

    ///////////////////////////////////////
    // Create Font Sampler:
    {
      VkSamplerCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      info.magFilter = VK_FILTER_LINEAR;
      info.minFilter = VK_FILTER_LINEAR;
      info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
      info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
      info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
      info.minLod = -1000;
      info.maxLod = 1000;
      info.maxAnisotropy = 1.0f;
      VkResult err = vkCreateSampler(mDevice.device, &info, NULL, &mFontSampler);
    }

    ///////////////////////////////////////
    // Create Descriptor Set Layout:
    {
      VkSampler sampler[1] = { mFontSampler };
      VkDescriptorSetLayoutBinding binding[1] = {};
      binding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      binding[0].descriptorCount = 1;
      binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
      binding[0].pImmutableSamplers = sampler;
      VkDescriptorSetLayoutCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
      info.bindingCount = 1;
      info.pBindings = binding;
      VkResult err = vkCreateDescriptorSetLayout(mDevice.device, &info, NULL, &mDescriptorSetLayout);
    }


    ///////////////////////////////////////
    // Create Render Pass
    mRenderPass = CreateRenderPass();

    ///////////////////////////////////////
    // Create Framebuffers
    int w, h;
    SDL_GetWindowSize(aWindow, &w, &h);

    swapchain_images = mSwapchain.get_images().value();
    swapchain_image_views = mSwapchain.get_image_views().value();

    mFramebuffers.resize(swapchain_image_views.size());

    for (size_t i = 0; i < swapchain_image_views.size(); i++) {
      VkImageView attachments[] = { swapchain_image_views[i] };

      VkFramebufferCreateInfo framebuffer_info = {};
      framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebuffer_info.renderPass = mRenderPass;
      framebuffer_info.attachmentCount = 1;
      framebuffer_info.pAttachments = attachments;
      framebuffer_info.width = w;
      framebuffer_info.height = h;
      framebuffer_info.layers = 1;

      if (vkCreateFramebuffer(mDevice, &framebuffer_info, nullptr, &mFramebuffers[i]) != VK_SUCCESS) {
        printf("failed to create framebuffer\n");
        return;
      }
    }
    
    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForVulkan(aWindow);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = mInstance;
    init_info.PhysicalDevice = mPhysicalDevice;
    init_info.Device = mDevice;
    init_info.QueueFamily = mGraphicsQueue.GetQueueFamily();
    init_info.Queue = mGraphicsQueue;
    init_info.PipelineCache = mPipelineCache;
    init_info.DescriptorPool = mDescriptorPool;
    init_info.Allocator = mInstance.allocation_callbacks;
    init_info.MinImageCount = cMinImageCount;
    init_info.ImageCount = cMinImageCount;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info, mRenderPass);
  }


  VkRenderPass VulkanRenderer::CreateRenderPass()
  {
    VkRenderPass renderPass;
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = mSwapchain.image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;// | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (vkCreateRenderPass(mDevice, &render_pass_info, nullptr, &renderPass) != VK_SUCCESS) {
      printf("failed to create render pass\n");
      return VK_NULL_HANDLE;;
    }

    return renderPass;
  }

  void VulkanRenderer::NewFrame()
  {
    ImGui_ImplVulkan_NewFrame();

    auto [commandBuffer, fence, waitSemaphore, signalSemphore] = mGraphicsQueue.WaitOnNextCommandList();

    // Wait on last frame/get next frame now, just in case we need to load the font textures.
    VkResult result = vkAcquireNextImageKHR(mDevice,
      mSwapchain,
      UINT64_MAX,
      waitSemaphore,
      VK_NULL_HANDLE,
      &mImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      return RecreateSwapchain();
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      printf("failed to acquire swapchain image.\n");
      return;
    }

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkClearValue clearColor{ { mClearColor } };
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = mRenderPass;
    info.framebuffer = mFramebuffers[mImageIndex];
    info.renderArea.extent = mSwapchain.extent;
    info.clearValueCount = 1;
    info.pClearValues = &clearColor;
    vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
  }

  void VulkanRenderer::RecreateSwapchain()
  {
    vkb::SwapchainBuilder swapchain_builder{ mDevice };
    swapchain_builder.set_desired_format(VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR });
    auto swap_ret = swapchain_builder.set_old_swapchain(mSwapchain)
      .build();
    if (!swap_ret) {
      // If it failed to create a swapchain, the old swapchain handle is invalid.
      mSwapchain.swapchain = VK_NULL_HANDLE;
    }
    // Even though we recycled the previous swapchain, we need to free its resources.
    vkb::destroy_swapchain(mSwapchain);
    // Get the new swapchain and place it in our variable
    mSwapchain = swap_ret.value();


    for (auto framebuffer : mFramebuffers)
    {
      vkDestroyFramebuffer(mDevice.device, framebuffer, mDevice.allocation_callbacks);
    }

    int w, h;
    SDL_GetWindowSize(mWindow, &w, &h);

    swapchain_images = mSwapchain.get_images().value();
    swapchain_image_views = mSwapchain.get_image_views().value();

    mFramebuffers.resize(swapchain_image_views.size());

    for (size_t i = 0; i < swapchain_image_views.size(); i++) {
      VkImageView attachments[] = { swapchain_image_views[i] };

      VkFramebufferCreateInfo framebuffer_info = {};
      framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebuffer_info.renderPass = mRenderPass;
      framebuffer_info.attachmentCount = 1;
      framebuffer_info.pAttachments = attachments;
      framebuffer_info.width = w;
      framebuffer_info.height = h;
      framebuffer_info.layers = 1;

      if (vkCreateFramebuffer(mDevice, &framebuffer_info, nullptr, &mFramebuffers[i]) != VK_SUCCESS) {
        printf("failed to create framebuffer\n");
        return;
      }
    }
  }

  void VulkanRenderer::ResizeRenderTarget(unsigned int aWidth, unsigned int aHeight)
  {
    ImGui_ImplVulkan_SetMinImageCount(cMinImageCount);
    RecreateSwapchain();
  }

  void VulkanRenderer::ClearRenderTarget(glm::vec4 aClearColor)
  {
    mClearColor.float32[0] = aClearColor.x * aClearColor.w;
    mClearColor.float32[1] = aClearColor.y * aClearColor.w;
    mClearColor.float32[2] = aClearColor.z * aClearColor.w;
    mClearColor.float32[3] = aClearColor.w;
  }

  void VulkanRenderer::RenderImguiData()
  {
    auto [commandList, fence, waitSemaphore, signalSemphore] = mGraphicsQueue.GetCurrentCommandList();

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandList);
  }

  void VulkanRenderer::Present()
  {
    auto [commandList, fence, waitSemaphore, signalSemphore] = mGraphicsQueue.GetCurrentCommandList();

    auto transferQueueCommandList = mTransferQueue.GetCurrentCommandList();
    vkWaitForFences(mDevice.device, 1, &transferQueueCommandList.mFence, true, UINT64_MAX);
    //vkQueueWaitIdle(mTransferQueue);
    auto transitionFence = TransitionTextures();

    vkCmdEndRenderPass(commandList);
    vkEndCommandBuffer(commandList);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = { waitSemaphore };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = wait_semaphores;
    submitInfo.pWaitDstStageMask = wait_stages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandList;

    VkSemaphore signal_semaphores[] = { signalSemphore };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signal_semaphores;

    vkResetFences(mDevice, 1, &fence);

    if (VK_NULL_HANDLE != transitionFence)
    {
      vkWaitForFences(mDevice.device, 1, &transitionFence, true, UINT64_MAX);
    }

    if (vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
      printf("failed to submit draw command buffer\n");
      return;
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swapChains[] = { mSwapchain };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapChains;

    present_info.pImageIndices = &mImageIndex;

    auto result = vkQueuePresentKHR(mPresentQueue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
      return RecreateSwapchain();
    }
    else if (result != VK_SUCCESS) {
      printf("failed to present swapchain image\n");
      return;
    }

    mCurrentFrame = (mCurrentFrame + 1) % cMinImageCount;
  }

  //////////////////////////////////////////////////////////////////////////////////////////////
  // Textures:
  class VulkanTexture : public Texture
  {
  public:
    VulkanTexture(
      VkImage aImage,
      VkDescriptorSet aDescriptorSet,
      int aWidth,
      int aHeight)
      : Texture{ aWidth, aHeight }
      , mImage{ aImage }
      , mDescriptorSet{ aDescriptorSet }
    {

    }

    ~VulkanTexture() override
    {
    }

    virtual ImTextureID GetTextureId()
    {
      return ImTextureID{(void*)mImage, (void*)mDescriptorSet};
    }

    VkImage mImage;
    VkDescriptorSet mDescriptorSet;
  };

  std::unique_ptr<Texture> VulkanRenderer::LoadTextureFromData(unsigned char* data, TextureLayout format, int aWidth, int aHeight, int pitch)
  {
    ImGuiIO& io = ImGui::GetIO();

    size_t upload_size = aWidth * aHeight * 4 * sizeof(char);

    VkResult err;

    VkImage image;
    VkImageView imageView;
    VkDeviceMemory fontMemory;
    VkDeviceMemory uploadBufferMemory;
    VkBuffer uploadBuffer;
    VkDescriptorSet descriptorSet;

    // Create Descriptor Set:
    {
      VkDescriptorSetAllocateInfo alloc_info = {};
      alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      alloc_info.descriptorPool = mDescriptorPool;
      alloc_info.descriptorSetCount = 1;
      alloc_info.pSetLayouts = &mDescriptorSetLayout;
      err = vkAllocateDescriptorSets(mDevice.device, &alloc_info, &descriptorSet);
      check_vk_result(err);
    }

    // Create the Image:
    {
      VkImageCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      info.imageType = VK_IMAGE_TYPE_2D;
      info.format = VK_FORMAT_R8G8B8A8_UNORM;
      info.extent.width = aWidth;
      info.extent.height = aHeight;
      info.extent.depth = 1;
      info.mipLevels = 1;
      info.arrayLayers = 1;
      info.samples = VK_SAMPLE_COUNT_1_BIT;
      info.tiling = VK_IMAGE_TILING_OPTIMAL;
      info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      err = vkCreateImage(mDevice.device, &info, mDevice.allocation_callbacks, &image);
      check_vk_result(err);
      VkMemoryRequirements req;
      vkGetImageMemoryRequirements(mDevice.device, image, &req);
      VkMemoryAllocateInfo alloc_info = {};
      alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      alloc_info.allocationSize = req.size;
      alloc_info.memoryTypeIndex = ImGui_ImplVulkan_MemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
      err = vkAllocateMemory(mDevice.device, &alloc_info, mDevice.allocation_callbacks, &fontMemory);
      check_vk_result(err);
      err = vkBindImageMemory(mDevice.device, image, fontMemory, 0);
      check_vk_result(err);
    }

    // Create the Image View:
    {
      VkImageViewCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      info.image = image;
      info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      info.format = VK_FORMAT_R8G8B8A8_UNORM;
      info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      info.subresourceRange.levelCount = 1;
      info.subresourceRange.layerCount = 1;
      err = vkCreateImageView(mDevice.device, &info, mDevice.allocation_callbacks, &imageView);
      check_vk_result(err);
    }

    // Update the Descriptor Set:
    {
      VkDescriptorImageInfo desc_image[1] = {};
      desc_image[0].sampler = mFontSampler;
      desc_image[0].imageView = imageView;
      desc_image[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      VkWriteDescriptorSet write_desc[1] = {};
      write_desc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write_desc[0].dstSet = descriptorSet;
      write_desc[0].descriptorCount = 1;
      write_desc[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      write_desc[0].pImageInfo = desc_image;
      vkUpdateDescriptorSets(mDevice.device, 1, write_desc, 0, NULL);
    }

    // Create the Upload Buffer:
    {
      VkBufferCreateInfo buffer_info = {};
      buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      buffer_info.size = upload_size;
      buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      err = vkCreateBuffer(mDevice.device, &buffer_info, mDevice.allocation_callbacks, &uploadBuffer);
      check_vk_result(err);
      VkMemoryRequirements req;
      vkGetBufferMemoryRequirements(mDevice.device, uploadBuffer, &req);
      VkMemoryAllocateInfo alloc_info = {};
      alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      alloc_info.allocationSize = req.size;
      alloc_info.memoryTypeIndex = ImGui_ImplVulkan_MemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
      err = vkAllocateMemory(mDevice.device, &alloc_info, mDevice.allocation_callbacks, &uploadBufferMemory);
      check_vk_result(err);
      err = vkBindBufferMemory(mDevice.device, uploadBuffer, uploadBufferMemory, 0);
      check_vk_result(err);
    }

    // Upload to Buffer:
    {
      char* map = NULL;
      err = vkMapMemory(mDevice.device, uploadBufferMemory, 0, upload_size, 0, (void**)(&map));
      check_vk_result(err);
      memcpy(map, data, upload_size);
      VkMappedMemoryRange range[1] = {};
      range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
      range[0].memory = uploadBufferMemory;
      range[0].size = upload_size;
      err = vkFlushMappedMemoryRanges(mDevice.device, 1, range);
      check_vk_result(err);
      vkUnmapMemory(mDevice.device, uploadBufferMemory);
    }

    // Create Command Buffer
    auto [commandList, fence, waitSemaphore, signalSemphore] = mTransferQueue.WaitOnNextCommandList();

    // Begin command buffer
    {
      VkCommandBufferBeginInfo begin_info = {};
      begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      err = vkBeginCommandBuffer(commandList, &begin_info);
      check_vk_result(err);
    }


    // Copy to Image:
    {
      VkImageMemoryBarrier copy_barrier[1] = {};
      copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      copy_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      copy_barrier[0].image = image;
      copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copy_barrier[0].subresourceRange.levelCount = 1;
      copy_barrier[0].subresourceRange.layerCount = 1;
      vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, copy_barrier);

      VkBufferImageCopy region = {};
      region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      region.imageSubresource.layerCount = 1;
      region.imageExtent.width = aWidth;
      region.imageExtent.height = aHeight;
      region.imageExtent.depth = 1;
      vkCmdCopyBufferToImage(commandList, uploadBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

      mTexturesCreatedThisFrame.emplace_back(image);
    }

    // Submit command buffer
    {
      VkSubmitInfo end_info = {};
      end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      end_info.commandBufferCount = 1;
      end_info.pCommandBuffers = &commandList;
      err = vkEndCommandBuffer(commandList);
      check_vk_result(err);
      err = vkQueueSubmit(mTransferQueue, 1, &end_info, fence);
      check_vk_result(err);
    }

    return std::make_unique<VulkanTexture>(image, descriptorSet, aWidth, aHeight);
  }

  VkFence VulkanRenderer::TransitionTextures()
  {
    if (mTexturesCreatedThisFrame.empty())
      return VK_NULL_HANDLE;

    auto [commandList, fence, waitSemaphore, signalSemphore] = mTextureTransitionQueue.WaitOnNextCommandList();

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    auto err = vkBeginCommandBuffer(commandList, &begin_info);
    check_vk_result(err);

    if (false == mLoadedFontTexture)
    {
      ImGui_ImplVulkan_CreateFontsTexture(commandList);
      mLoadedFontTexture = true;
    }

    for (auto& image : mTexturesCreatedThisFrame)
    {
      VkImageMemoryBarrier use_barrier[1] = {};
      use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      use_barrier[0].image = image;
      use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      use_barrier[0].subresourceRange.levelCount = 1;
      use_barrier[0].subresourceRange.layerCount = 1;
      vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, use_barrier);
    }

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &commandList;
    err = vkEndCommandBuffer(commandList);
    check_vk_result(err);

    err = vkQueueSubmit(mTextureTransitionQueue, 1, &end_info, fence);
    check_vk_result(err);

    mTexturesCreatedThisFrame.clear();

    return fence;
  }
}