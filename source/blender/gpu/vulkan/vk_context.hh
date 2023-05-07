/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_context_private.hh"

#include "vk_command_buffer.hh"
#include "vk_debug.hh"
#include "vk_descriptor_pools.hh"
#include "vk_state_manager.hh"

#define VK_MAX_TEXTURE_SLOTS 128
#define VK_MAX_SAMPLER_SLOTS VK_MAX_TEXTURE_SLOTS
/* Max limit without using bind-less for samplers. */
#define VK_MAX_DEFAULT_SAMPLERS 16
#define VK_MAX_UNIFORM_BUFFER_BINDINGS 31
#define VK_MAX_VERTEX_INPUT_ATTRIBUTES 31
#define VK_MAX_UNIFORMS_PER_BLOCK 64
namespace blender::gpu {
struct VKSamplerState {
  bool initialized;
  GPUSamplerState state;

  bool operator==(const VKSamplerState &other) const
  {
    /* Add other parameters as needed. */
    return (this->state == other.state);
  }

#if 0
  operator uint() const { return uint(state); }
  operator uint64_t() const { return uint64_t(state); }
#endif

  VKSamplerState()
  {
    initialized = false;
  };
  VKSamplerState(GPUSamplerState state_) : state(state_)
  {
    initialized = false;
  }
};

const VKSamplerState DEFAULT_SAMPLER_STATE =
    VKSamplerState();  // GPU_SAMPLER_DEFAULT /*, 0, 9999*/);
};                     // namespace blender::gpu

namespace blender::gpu {
class VKFrameBuffer;
class VKVertexAttributeObject;
class VKBatch;
class VKStateManager;

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
  // VmaAllocator mem_allocator_ = VK_NULL_HANDLE;
  //VKDescriptorPools descriptor_pools_;

  /** Limits of the device linked to this context. */
  VkPhysicalDeviceLimits vk_physical_device_limits_;

  void *ghost_context_;

  Vector<SafeImage> vk_swap_chain_images_;

  bool vk_in_frame_;

  uint32_t vk_fb_id_ = 0;
  debug::VKDebuggingTools debugging_tools_;

 public:
 VKVertexBuffer*  default_vbo_dummy;
 static bool base_instance_support;

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
  void debug_capture_title(const char* title);

  bool has_active_framebuffer() const;
  void activate_framebuffer(VKFrameBuffer &framebuffer);
  void deactivate_framebuffer();
  VKFrameBuffer *active_framebuffer_get() const;
  void bind_graphics_pipeline(const VKBatch &batch,
                              const VKVertexAttributeObject &vertex_attribute_object);
  void swapchains();

  bool validate_frame();

  bool validate_image();

  void bind_graphics_pipeline(GPUPrimType prim_type,
                              const VKVertexAttributeObject &vertex_attribute_object);

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

  const VKStateManager &state_manager_get2() const;

  VkQueue queue_get() const
  {
    return vk_queue_;
  }

  const uint32_t *queue_family_ptr_get() const
  {
    return &vk_queue_family_;
  }

  debug::VKDebuggingTools &debugging_tools_get()
  {
    return debugging_tools_;
  }

  const debug::VKDebuggingTools &debugging_tools_get() const
  {
    return debugging_tools_;
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

  uint8_t current_image_id_get()
  {
    return 1 & vk_fb_id_;
  }

  void swapbuffers();

 private:
  void init_physical_device_limits();
};

}  // namespace blender::gpu
