/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once
#pragma warning(push)
#pragma warning(disable : 5038)

#include "GHOST_Context.h"
#include "BKE_global.h"
#include <cassert>

#include <unordered_map>
#include <array>
#include <functional>
#ifdef _WIN32
#  include "GHOST_SystemWin32.h"
#elif defined(__APPLE__)
#  include "GHOST_SystemCocoa.h"
#else
#  include "GHOST_SystemX11.h"
#  ifdef WITH_GHOST_WAYLAND
#    include "GHOST_SystemWayland.h"
#  else
#    define wl_surface void
#    define wl_display void
#  endif
#endif


#include <vector>

#ifdef __APPLE__
#  include <MoltenVK/vk_mvk_moltenvk.h>
#else
#  include <vulkan/vulkan.h>
#endif
#define VK_ENABLE_DebugPrint
#define VULKAN_ERROR_CHECK








#define DEBUG_PRINTF(...) printf(__VA_ARGS__); 






#ifdef VULKAN_ERROR_CHECK
///   https:// registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_debug_report.html    ///Deprecated info
         enum VULKAN_DEBUG_TYPE {
  VULKAN_DEBUG_REPORT_ALL = 0,
  VULKAN_DEBUG_REPORT,
  VULKAN_DEBUG_UTILS,
  VULKAN_DEBUG_UTILS_ALL,
  VULKAN_DEBUG_BOTH ,
  VULKAN_DEBUG_NONE
};
#endif

 





class VKFrameBuffer;

#ifndef GHOST_OPENGL_VK_CONTEXT_FLAGS
/* leave as convenience define for the future */
#  define GHOST_OPENGL_VK_CONTEXT_FLAGS 0
#endif

#ifndef GHOST_OPENGL_VK_RESET_NOTIFICATION_STRATEGY
#  define GHOST_OPENGL_VK_RESET_NOTIFICATION_STRATEGY 0
#endif

    typedef enum {
      GHOST_kVulkanPlatformX11 = 0,
#ifdef WITH_GHOST_WAYLAND
      GHOST_kVulkanPlatformWayland,
#endif
    } GHOST_TVulkanPlatformType;

    const char *vulkan_error_as_string(VkResult result);
#define __STR(A) "" #A
#ifndef VK_CHECK
#define VK_CHECK(__expression) \
  do { \
    VkResult r = (__expression); \
    if (r != VK_SUCCESS) { \
      fprintf(stderr, \
              "Vulkan Error : %s:%d : %s failled with %s\n", \
              __FILE__, \
              __LINE__, \
              __STR(__expression), \
              vulkan_error_as_string(r)); \
      return GHOST_kFailure; \
    } \
  } while (0)
#endif
#ifndef VK_CHECK2
#define VK_CHECK2(__expression) \
  do { \
    VkResult r = (__expression); \
    if (r != VK_SUCCESS) { \
      fprintf(stderr, \
              "Vulkan Error : %s:%d : %s failled with %s\n", \
              __FILE__, \
              __LINE__, \
              __STR(__expression), \
              vulkan_error_as_string(r)); \
      exit(-1); \
    } \
  } while (0)
#endif

  class GHOST_ContextVK;


    uint32_t getMemoryType(uint32_t typeBits,
                           VkMemoryPropertyFlags properties,
                           VkBool32 *memTypeFound = nullptr);
    uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties);
bool MemoryTypeFromProperties(uint32_t nMemoryTypeBits,
                              VkMemoryPropertyFlags nMemoryProperties,
                              uint32_t *pTypeIndexOut);
static void setImageLayout(VkCommandBuffer cmdbuffer,
                           VkImage image,
                           VkImageLayout oldImageLayout,
                           VkImageLayout newImageLayout,
                           VkImageSubresourceRange subresourceRange,
                           VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                           VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
{
  // Create an image barrier object
  VkImageMemoryBarrier imageMemoryBarrier{};
  imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.oldLayout = oldImageLayout;
  imageMemoryBarrier.newLayout = newImageLayout;
  imageMemoryBarrier.image = image;
  imageMemoryBarrier.subresourceRange = subresourceRange;

  // Source layouts (old)
  // Source access mask controls actions that have to be finished on the old layout
  // before it will be transitioned to the new layout
  switch (oldImageLayout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      // Image layout is undefined (or does not matter)
      // Only valid as initial layout
      // No flags required, listed only for completeness
      imageMemoryBarrier.srcAccessMask = 0;
      break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      // Image is preinitialized
      // Only valid as initial layout for linear images, preserves memory contents
      // Make sure host writes have been finished
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      // Image is a color attachment
      // Make sure any writes to the color buffer have been finished
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      // Image is a depth/stencil attachment
      // Make sure any writes to the depth/stencil buffer have been finished
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      // Image is a transfer source
      // Make sure any reads from the image have been finished
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      // Image is a transfer destination
      // Make sure any writes to the image have been finished
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      // Image is read by a shader
      // Make sure any shader reads from the image have been finished
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      break;
    default:
      // Other source layouts aren't handled (yet)
      break;
  }

  // Target layouts (new)
  // Destination access mask controls the dependency for the new image layout
  switch (newImageLayout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      // Image will be used as a transfer destination
      // Make sure any writes to the image have been finished
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      // Image will be used as a transfer source
      // Make sure any reads from the image have been finished
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      // Image will be used as a color attachment
      // Make sure any writes to the color buffer have been finished
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      // Image layout will be used as a depth/stencil attachment
      // Make sure any writes to depth/stencil buffer have been finished
      imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask |
                                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      // Image will be read in a shader (sampler, input attachment)
      // Make sure any writes to the image have been finished
      if (imageMemoryBarrier.srcAccessMask == 0) {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
      }
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      break;
    default:
      // Other source layouts aren't handled (yet)
      break;
  }

  // Put barrier inside setup command buffer
  vkCmdPipelineBarrier(
      cmdbuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
}

static void setImageLayout(VkCommandBuffer cmdbuffer,
                           VkImage image,
                           VkImageAspectFlags aspectMask,
                           VkImageLayout oldImageLayout,
                           VkImageLayout newImageLayout,
                           VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                           VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
{
  VkImageSubresourceRange subresourceRange = {};
  subresourceRange.aspectMask = aspectMask;
  subresourceRange.baseMipLevel = 0;
  subresourceRange.levelCount = 1;
  subresourceRange.layerCount = 1;
  setImageLayout(cmdbuffer,
                 image,
                 oldImageLayout,
                 newImageLayout,
                 subresourceRange,
                 srcStageMask,
                 dstStageMask);
}




struct ExtensionEntry {
  ExtensionEntry(const char *entryName,
                 bool isOptional = false,
                 void *pointerFeatureStruct = nullptr,
                 uint32_t checkVersion = 0)
      : name(entryName),
        optional(isOptional),
        pFeatureStruct(pointerFeatureStruct),
        version(checkVersion)
  {
  }
  const char *name{nullptr};
  bool optional{false};
  void *pFeatureStruct{nullptr};
  uint32_t version{0};
};

#include <sstream>
#include <unordered_set>
struct DebugMaster {

  PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessengerEXT = nullptr;
  PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessengerEXT = nullptr;
  VkDebugUtilsMessengerEXT dbgMessenger = nullptr;
  std::unordered_set<int32_t> dbgIgnoreMessages;
  VkDevice device;
  void ignoreDebugMessage(int32_t msgID);

  void _setObjectName(uint64_t object, const std::string &name, VkObjectType t);

#if VK_NV_ray_tracing
  void setObjectName(VkAccelerationStructureNV object, const std::string &name);
#endif
  void setObjectName(VkBuffer object, const std::string &name);

  void beginLabel(VkCommandBuffer cmdBuf, const std::string &label);
  void endLabel(VkCommandBuffer cmdBuf);
  void insertLabel(VkCommandBuffer cmdBuf, const std::string &label);

#if 0
  struct ScopedCmdLabel {
    ScopedCmdLabel(VkCommandBuffer cmdBuf, const std::string &label);
    ~ScopedCmdLabel();
    void setLabel(const std::string &label);

   private:
    VkCommandBuffer m_cmdBuf;
  };

  ScopedCmdLabel scopeLabel(VkCommandBuffer cmdBuf, const std::string &label);
 #endif

 private:
  static bool s_enabled;
};



class GHOST_ContextVK : public GHOST_Context {
 public:
  /**
   * Constructor.
   */
  GHOST_ContextVK(bool stereoVisual,
#ifdef _WIN32
                  HWND hwnd,
#elif defined(__APPLE__)
                  /* FIXME CAMetalLayer but have issue with linking. */
                  void *metal_layer,
#else
                  GHOST_TVulkanPlatformType platform,
                  /* X11 */
                  Window window,
                  Display *display,
                  /* Wayland */
                  wl_surface *wayland_surface,
                  wl_display *wayland_display,
#endif
                  int contextMajorVersion,
                  int contextMinorVersion,
                  int m_debug
#ifdef VULKAN_ERROR_CHECK
                  ,
                  VULKAN_DEBUG_TYPE m_dmode
#endif

  );

  /**
   * Destructor.
   */
  ~GHOST_ContextVK();
  DebugMaster deb;
  GHOST_TSuccess  init_image_layout();
  GHOST_TSuccess  finalize_image_layout();
  GHOST_TSuccess  fail_image_layout();
  std::function<void()> destroyer;
  VkClearValue clearValues_[2] = { { {0.025f, 0.025f, 0.025f, 1.0f} } ,  {1.0f, 0} };
  GHOST_TSuccess clear_color(VkCommandBuffer& cmd, const VkClearColorValue* clearValues) {
    VkImageSubresourceRange ImageSubresourceRange;
    ImageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ImageSubresourceRange.baseMipLevel = 0;
    ImageSubresourceRange.levelCount = 1;
    ImageSubresourceRange.baseArrayLayer = 0;
    ImageSubresourceRange.layerCount = 1;
    vkCmdClearColorImage(cmd, m_swapchain_images[m_currentImage], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, clearValues, 1, &ImageSubresourceRange);
    return GHOST_kSuccess;

  }
   GHOST_TSuccess begin_frame(VkCommandBuffer &cmd,int i=-1)
    {

#if 0
      if (i==-1)i = m_currentFrame;

      cmd = m_command_buffers[m_currentCommand];
      static bool ini = true;

      static VkCommandBufferBeginInfo cmdBufInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
      static VkRenderPassBeginInfo renderPassBeginInfo = {
          VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};


        renderPassBeginInfo.renderPass = m_render_pass;
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = m_render_extent.width;
        renderPassBeginInfo.renderArea.extent.height = m_render_extent.height;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = clearValues_;


      renderPassBeginInfo.framebuffer = m_swapchain_framebuffers[i];
      vkResetCommandBuffer(m_command_buffers[m_currentCommand] ,VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
      VK_CHECK(vkBeginCommandBuffer(m_command_buffers[m_currentCommand], &cmdBufInfo));

      vkCmdBeginRenderPass(m_command_buffers[m_currentCommand], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      VkViewport viewport = {};
      viewport.width = (float)m_render_extent.width;
      viewport.height = (float)m_render_extent.height;
      viewport.minDepth = 0.f;
      viewport.maxDepth = 1.f;

      VkRect2D scissor = {};
      scissor.extent.width = m_render_extent.width;
      scissor.extent.height = m_render_extent.height;
      scissor.offset.x = 0;
      scissor.offset.y = 0;
      vkCmdSetViewport(m_command_buffers[m_currentCommand], 0, 1, &viewport);
      vkCmdSetScissor(m_command_buffers[m_currentCommand], 0, 1, &scissor);
#endif
      return GHOST_kSuccess;
    };

  void set_layout(VkImageLayout layout) {
     m_current_layouts[m_currentImage] = layout;
   }
  
  VkImageLayout get_layout() {
    return m_current_layouts[m_currentImage];
  }

    GHOST_TSuccess end_frame()
    {
      VkCommandBuffer& cmd = m_command_buffers[m_currentCommand];
      vkCmdEndRenderPass(cmd);
      VK_CHECK(vkEndCommandBuffer(cmd));
      m_currentCommand = (m_currentCommand + 1) % m_command_buffers.size();
            return GHOST_kSuccess;

    };

    GHOST_TSuccess begin_submit_simple(VkCommandBuffer& cmd) {
        cmd = getCommandBuffers(m_currentCommand);
        VkCommandBufferBeginInfo cmdBufInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        vkResetCommandBuffer(cmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBufInfo));

        if (m_in_flight_fences.size() <=0 ) {

          VkFenceCreateInfo fence_info = {};
          fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
          fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
          m_in_flight_fences.resize(2);
          for (int i = 0; i < 2; i++) {
            VK_CHECK(vkCreateFence(m_device, &fence_info, NULL, &m_in_flight_fences[i]));
          }

        }

        vkResetFences(m_device, 1, &m_in_flight_fences[m_currentFence]);
        return GHOST_kSuccess;
    };
    GHOST_TSuccess end_submit_simple() {

        auto cmd = getCommandBuffers(m_currentCommand);
        VK_CHECK(vkEndCommandBuffer(cmd));
        m_currentCommand = (m_currentCommand + 1) % m_command_buffers.size();


        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitSemaphores = nullptr;
        submit_info.pWaitDstStageMask = nullptr;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;
        submit_info.signalSemaphoreCount = 0;
        submit_info.pSignalSemaphores = nullptr;
        VK_CHECK(vkQueueSubmit(m_graphic_queue, 1, &submit_info, m_in_flight_fences[m_currentFence]));


        waitCustom();


        return GHOST_kSuccess;

    };
    GHOST_TSuccess begin_offscreen_submit(VkCommandBuffer& cmd);
    GHOST_TSuccess end_offscreen_submit(VkCommandBuffer& cmd, VkSemaphore wait = VK_NULL_HANDLE, VkSemaphore signal = VK_NULL_HANDLE);
    int begin_onetime_submit(VkCommandBuffer cmd);
    GHOST_TSuccess end_onetime_submit(int registerID);

    GHOST_TSuccess begin_blit_submit(VkCommandBuffer& cmd);

    GHOST_TSuccess end_blit_submit(VkCommandBuffer& cmd, std::vector<VkSemaphore> batch_signal);

    VkCommandBuffer getCommandBuffers(int i);
    GHOST_TSuccess begin_submit(int N);
    GHOST_TSuccess end_submit();


   std::vector<VkCommandBuffer> onetime_commands_ ;

    GHOST_TSuccess  initialize_onetime_submit();
    GHOST_TSuccess  finalize_onetime_submit();
    int current_wait_sema_ = 0;
    int pipeline_sema_idx_ = 0;
    GHOST_TSuccess submit_nonblocking();

  /**
   * Swaps front and back buffers of a window.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess swapBuffers();

  /**
   * Activates the drawing context of this window.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess activateDrawingContext();

  /**
   * Release the drawing context of the calling thread.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess releaseDrawingContext();

  /**
   * Call immediately after new to initialize.  If this fails then immediately delete the object.
   * \return Indication as to whether initialization has succeeded.
   */
  GHOST_TSuccess initializeDrawingContext();

  /**
   * Removes references to native handles from this context and then returns
   * \return GHOST_kSuccess if it is OK for the parent to release the handles and
   * GHOST_kFailure if releasing the handles will interfere with sharing
   */
  GHOST_TSuccess releaseNativeHandles();


  GHOST_TSuccess getQueue(uint32_t qty,
                                           void *r_queue,
                                           uint32_t *r_graphic_queue_familly);
  uint32_t getQueueIndex(uint32_t i);


  /* Validation Error: [ VUID-vkCreateGraphicsPipelines-pipelineCache-parent ] Object 0xfc850b000000045c of type VkPipelineCache was not created, */
  /* allocated or retrieved from the correct device. The Vulkan spec states: If pipelineCache is a valid handle, it must have been created, allocated, or retrieved from device (https://vulkan.lunarg.com/doc/view/1.3.231.1/windows/1.3-extensions/vkspec.html#VUID-vkCreateGraphicsPipelines-pipelineCache-parent) */
  /*Pipelines are cached per logical device. So GHOST is responsible for the PipelineCache lifecycle.*/
  /*However, if we associate a VKContext with a logical device, it will be managed there.*/

  VkPipelineCache vkPC = VK_NULL_HANDLE;

  GHOST_TSuccess createPipelineCache();
  VkPipelineCache getPipelineCache();
  void destroyPipelineCache();


  int getCurrent(VkCommandBuffer& cmd)
  {
    cmd = m_command_buffers[m_currentCommand];
    return m_currentFrame;
  }
  VkDevice getDevice()
  {
    return m_device;
  };
  VkPhysicalDevice getPhysicalDevice()
  {
    return m_physical_device;
  };
  VkRenderPass getRenderPass()
  {
    return m_render_pass;
  };
int getFrame()
  {
    return m_currentFrame;
  };

void getImage(VkImage& image)
{
    image  =  m_swapchain_images[m_currentImage];
    return;
};

uint32_t   getCurrentImage()
{
  if (m_swapchain_images.size() > 0) {
    return m_currentImage;
  }
  return 0;
};


void getImageView(VkImageView& view,int i)
{
  view = m_swapchain_image_views[i];
  return;
};
uint32_t getImageCount()
{
  return static_cast<uint32_t>(m_swapchain_image_views.size());
};
VkFormat getImageFormat()
{
    return  m_image_format;
};

VkFormat getDepthFormat()
{
  return  m_depth_format;
};
 // bool getCommandBuffer(VkCommandBuffer &cmd, int i);
  /**
   * Gets the Vulkan context related resource handles.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess getVulkanHandles(void *r_instance,
                                  void *r_physical_device,
                                  void *r_device,
                                  uint32_t *r_graphic_queue_familly);
  /**
   * Gets the Vulkan framebuffer related resource handles associated with the Vulkan context.
   * Needs to be called after each swap events as the framebuffer will change.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess getVulkanBackbuffer(void *image,
                                     void *framebuffer,
                                     void *command_buffer,
                                     void *render_pass,
                                     void *extent,
                                     uint32_t *fb_id);
  void getRenderExtent(VkExtent2D& _render_extent);
  /**
   * Sets the swap interval for swapBuffers.
   * \param interval The swap interval to use.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess setSwapInterval(int /* interval */)
  {
    return GHOST_kFailure;
  }

  /**
   * Gets the current swap interval for swapBuffers.
   * \param intervalOut Variable to store the swap interval if it can be read.
   * \return Whether the swap interval can be read.
   */
  GHOST_TSuccess getSwapInterval(int &)
  {
    return GHOST_kFailure;
  };

  GHOST_TSuccess beginCustom(int i, VkCommandBuffer& cmd);


  GHOST_TSuccess acquireCustom();
  GHOST_TSuccess waitCustom();
  GHOST_TSuccess beginCustom(int i= -1);
  GHOST_TSuccess submitCustom2();
  GHOST_TSuccess submitCustom();
  GHOST_TSuccess presentCustom();


  void getCrrentCommandBuffer(VkCommandBuffer& cmd) {
    cmd = getCommandBuffers(m_currentCommand);
  }

  int     primary_index_ = 0;
  int     secondary_index_ = 0;
  template<typename T>
  GHOST_TSuccess requestCommandBuffer(T& cmd,bool secondary = false)
  {

    if (secondary)
    {
      int stored_nums = static_cast<int>(m_seco_command_buffers.size());
      if (secondary_index_ < stored_nums) {
        cmd = m_seco_command_buffers[secondary_index_];
      }
      else {

        BLI_assert(m_command_pool != VK_NULL_HANDLE);
        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = m_command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        alloc_info.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(m_device, &alloc_info, &cmd));

        m_seco_command_buffers.push_back(cmd);

      }
      secondary_index_++;
      return GHOST_kSuccess;
    }
   

    int stored_nums = static_cast<int>(m_prim_command_buffers.size());
    if (primary_index_ < stored_nums) {
      cmd = m_prim_command_buffers[primary_index_];
    }
    else {

      BLI_assert(m_command_pool != VK_NULL_HANDLE);
      VkCommandBufferAllocateInfo alloc_info = {};
      alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      alloc_info.commandPool = m_command_pool;
      alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      alloc_info.commandBufferCount = 1;

      VK_CHECK(vkAllocateCommandBuffers(m_device, &alloc_info, &cmd));

      m_prim_command_buffers.push_back(cmd);

    }
    primary_index_++;
    
    return GHOST_kSuccess;
  }
  void flush();

  void set_fb_cb(std::function<void(void)>& func) {
    fb_cb = func;
  }

  void set_fb_sb_cb(std::function<void(void)>& func) {
    fb_sb_cb = func;
  }
  void set_fb_sb2_cb(std::function<void(void)>& func) {
    fb_sb2_cb = func;
  }
  std::function<void(void)> fb_cb, fb_sb_cb, fb_sb2_cb;

 private:



#ifdef _WIN32
  HWND m_hwnd;
#elif defined(__APPLE__)
  CAMetalLayer *m_metal_layer;
#else /* Linux */
  GHOST_TVulkanPlatformType m_platform;
  /* X11 */
  Display *m_display;
  Window m_window;
  /* Wayland */
  wl_surface *m_wayland_surface;
  wl_display *m_wayland_display;
#endif

  const int m_context_major_version;
  const int m_context_minor_version;
  const int m_debug;

  VkInstance m_instance;
  VkPhysicalDevice m_physical_device;
  VkDevice m_device;
  VkCommandPool m_command_pool;

  uint32_t m_queue_family_graphic;
  uint32_t m_queue_family_present;
  uint32_t m_queue_family_transfer;

  VkQueue m_graphic_queue;
  VkQueue m_present_queue;
  VkQueue m_transfer_queue;

  /* For display only. */
  VkSurfaceKHR m_surface;
  VkSwapchainKHR m_swapchain;
  std::vector<VkImage> m_swapchain_images;
  bool                         m_swapchain_acquires;
  VkFormat                  m_image_format;
  VkFormat                  m_depth_format;
  std::vector < VkImageLayout>         m_current_layouts;
  std::vector<VkImageView> m_swapchain_image_views;

  //std::vector < VKFrameBuffer >    m_swapchain_framebuffers;
  std::vector<VkCommandBuffer> m_command_buffers;


  std::vector<VkCommandBuffer> m_seco_command_buffers;
  std::vector<VkCommandBuffer> m_prim_command_buffers;
  VkRenderPass m_render_pass;
  VkExtent2D m_render_extent;
  std::vector<VkSemaphore> m_image_available_semaphores;
  std::vector<VkSemaphore> m_render_finished_semaphores;
  std::vector<VkFence> m_in_flight_fences;
  std::vector<VkFence> m_in_flight_images;
  /** frame modulo swapchain_len. Used as index for sync objects. */
  int m_currentFrame = 0;
  int m_currentCommand = 0;
  int m_currentFence = 0;
  /** Image index in the swapchain. Used as index for render objects. */
  uint32_t m_currentImage = 0;
  /** Used to unique framebuffer ids to return when swapchain is recreated. */
  uint32_t m_swapchain_id = 0;
  bool is_initialized = false;

  const char *getPlatformSpecificSurfaceExtension() const;
  GHOST_TSuccess pickPhysicalDevice(std::vector<const char *> required_exts);

  GHOST_TSuccess createSwapchain(void);
  GHOST_TSuccess destroySwapchain(void);
  GHOST_TSuccess createCommandBuffers(void);
  GHOST_TSuccess recordCommandBuffers(void);



 GHOST_TSuccess pickPhysicalDevice2(std::vector<ExtensionEntry>& required_exts);

 std::vector<void *> featureStructs;
 std::vector<const char *> enabledDeviceExts;



 VkPhysicalDeviceMemoryProperties m_memoryProperties;
#ifdef VULKAN_ERROR_CHECK
  VULKAN_DEBUG_TYPE m_debugMode;
#endif
  bool m_is_destroyed = false;
 
};

namespace blender {
  namespace vulkan {
    
    void GHOST_ImageTransition(
      VkCommandBuffer cmd,
      VkImage image,
      VkFormat format,
      VkImageLayout dstLayout,  // How the image will be laid out in memory.
      VkAccessFlags dstAccesses,
      VkImageAspectFlags aspects = VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM,
      VkAccessFlags srcAccess = VK_ACCESS_FLAG_BITS_MAX_ENUM,
      VkImageLayout srcLayout =
      VK_IMAGE_LAYOUT_MAX_ENUM);  // The ways that the app will be able to access the image.
  };
};

void clear_draw_test(GHOST_ContextVK* context_);

#pragma warning(pop)
