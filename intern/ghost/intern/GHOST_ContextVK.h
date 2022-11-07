/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include "GHOST_Context.h"


#define VULKAN_ERROR_CHECK

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


uint32_t getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound= nullptr);
uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties);
bool MemoryTypeFromProperties(uint32_t nMemoryTypeBits,
                              VkMemoryPropertyFlags nMemoryProperties,
                              uint32_t *pTypeIndexOut);

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


  template<class M> GHOST_TSuccess commandLoop(M &mat)
  {
    uint32_t i = m_currentFrame;


    static bool ini = true;

    static VkCommandBufferBeginInfo cmdBufInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    static VkClearValue clearValues[2];
    static VkRenderPassBeginInfo renderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};



    if (ini) {

      clearValues[0].color = {{0.025f, 0.025f, 0.025f, 1.0f}};
      clearValues[1].depthStencil = {1.0f, 0};

      renderPassBeginInfo.renderPass = m_render_pass;
      renderPassBeginInfo.renderArea.offset.x = 0;
      renderPassBeginInfo.renderArea.offset.y = 0;
      renderPassBeginInfo.renderArea.extent.width = m_render_extent.width;
      renderPassBeginInfo.renderArea.extent.height = m_render_extent.height;
      renderPassBeginInfo.clearValueCount = 2;
      renderPassBeginInfo.pClearValues = clearValues;
    }

    renderPassBeginInfo.framebuffer = m_swapchain_framebuffers[i];

    VK_CHECK(vkBeginCommandBuffer(m_command_buffers[i], &cmdBufInfo));

    vkCmdBeginRenderPass(m_command_buffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

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
    vkCmdSetViewport(m_command_buffers[i], 0, 1, &viewport);
    vkCmdSetScissor(m_command_buffers[i], 0, 1, &scissor);

    mat.make(m_command_buffers[i], m_render_finished_semaphores[i]);

    vkCmdEndRenderPass(m_command_buffers[i]);
    VK_CHECK(vkEndCommandBuffer(m_command_buffers[i]));

    return GHOST_kSuccess;
  };



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

  VkDevice getDevice()
  {
    return m_device;
  };
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

   


   #ifdef VULKAN_ERROR_CHECK
    GHOST_TSuccess Check_Context(int mode = 0);
   #endif
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
  std::vector<VkImageView> m_swapchain_image_views;
  std::vector<VkFramebuffer> m_swapchain_framebuffers;
  std::vector<VkCommandBuffer> m_command_buffers;
  VkRenderPass m_render_pass;
  VkExtent2D m_render_extent;
  std::vector<VkSemaphore> m_image_available_semaphores;
  std::vector<VkSemaphore> m_render_finished_semaphores;
  std::vector<VkFence> m_in_flight_fences;
  std::vector<VkFence> m_in_flight_images;
  /** frame modulo swapchain_len. Used as index for sync objects. */
  int m_currentFrame = 0;
  /** Image index in the swapchain. Used as index for render objects. */
  uint32_t m_currentImage = 0;
  /** Used to unique framebuffer ids to return when swapchain is recreated. */
  uint32_t m_swapchain_id = 0;

  const char *getPlatformSpecificSurfaceExtension() const;
  GHOST_TSuccess pickPhysicalDevice(std::vector<const char *> required_exts);
  GHOST_TSuccess createSwapchain(void);
  GHOST_TSuccess destroySwapchain(void);
  GHOST_TSuccess createCommandBuffers(void);
  GHOST_TSuccess recordCommandBuffers(void);


  VkPhysicalDeviceMemoryProperties m_memoryProperties;
#ifdef VULKAN_ERROR_CHECK
  VULKAN_DEBUG_TYPE m_debugMode;
#endif

};

