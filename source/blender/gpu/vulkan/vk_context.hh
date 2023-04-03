/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_context_private.hh"

#include "vk_command_buffer.hh"
#include "vk_descriptor_pools.hh"
#include "vk_state_manager.hh"

namespace blender::gpu {
class VKFrameBuffer;

class VKContext : public Context {
 private:
  /** Copies of the handles owned by the GHOST context. */
  VkInstance vk_instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice vk_physical_device_ = VK_NULL_HANDLE;
  VkDevice vk_device_ = VK_NULL_HANDLE;
  VKCommandBuffer command_buffer_;
  uint32_t vk_queue_family_ = 0;
  VkQueue vk_queue_ = VK_NULL_HANDLE;

  /** Allocator used for texture and buffers and other resources. */
  //VmaAllocator mem_allocator_ = VK_NULL_HANDLE;
  VKDescriptorPools descriptor_pools_;

  /** Limits of the device linked to this context. */
  VkPhysicalDeviceLimits vk_physical_device_limits_;

  void *ghost_context_;

  Vector<SafeImage> vk_swap_chain_images_;

  bool vk_in_frame_;

  uint32_t vk_fb_id_ = 0;

 public:
  VKContext(void *ghost_window, void *ghost_context);
  virtual ~VKContext();

  void activate() override;
  void deactivate() override;
  void begin_frame() override;
  void end_frame() override;

  void flush() override;
  void finish() override;
  void flush(bool toggle, bool fin, bool activate);

  void memory_statistics_get(int *total_mem, int *free_mem) override;

  void debug_group_begin(const char *, int) override;
  void debug_group_end() override;
  bool debug_capture_begin() override;
  void debug_capture_end() override;
  void *debug_capture_scope_create(const char *name) override;
  bool debug_capture_scope_begin(void *scope) override;
  void debug_capture_scope_end(void *scope) override;

  bool has_active_framebuffer() const;
  void activate_framebuffer(VKFrameBuffer &framebuffer);
  void deactivate_framebuffer();
  VKFrameBuffer *active_framebuffer_get() const;

  void swapchains();

  bool validate_frame();

  bool validate_image();

  void bind_graphics_pipeline();

  static VKContext *get(void)
  {
    return static_cast<VKContext *>(Context::get());
  }

  VkInstance instance_get() const
  {
    return vk_instance_;
  }

  VkPhysicalDevice physical_device_get() const
  {
    return vk_physical_device_;
  }

  const VkPhysicalDeviceLimits &physical_device_limits_get() const
  {
    return vk_physical_device_limits_;
  }

  VkDevice device_get() const
  {
    return vk_device_;
  }

  VKCommandBuffer &command_buffer_get()
  {
    return command_buffer_;
  }

  VKStateManager &state_manager_get()
  {
    return *(VKStateManager *)state_manager;
  }

  VkQueue queue_get() const
  {
    return vk_queue_;
  }

  const uint32_t *queue_family_ptr_get() const
  {
    return &vk_queue_family_;
  }

  VKDescriptorPools &descriptor_pools_get()
  {
    return descriptor_pools_;
  }

  /*
  VmaAllocator mem_allocator_get() const
  {
    return mem_allocator_;
  }
  */

  SafeImage &sc_image_get(int i = -1)
  {
    i = (i == -1) ? (vk_fb_id_ & 1) : i;
    SafeImage &im = vk_swap_chain_images_[i];
    return im;
  }

  uint8_t semaphore_get(VkSemaphore &wait, VkSemaphore &finish);

 uint8_t current_image_id_get(){
   return 1&vk_fb_id_;
   }

  void swapbuffers();

 private:
  void init_physical_device_limits();
};

}  // namespace blender::gpu
