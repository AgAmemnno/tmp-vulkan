/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_backend.hh"

#ifdef WITH_RENDERDOC
#  include "renderdoc_api.hh"
#endif

#include "vk_common.hh"
#include "vk_debug.hh"

#include "shaderc/shaderc.hpp"

namespace blender::gpu {

class VKContext;

class VKBackend : public GPUBackend {
 private:
  shaderc::Compiler shaderc_compiler_;
#ifdef WITH_RENDERDOC
  renderdoc::api::Renderdoc renderdoc_api_;
#endif
  /** Allocator used for texture and buffers and other resources. */
  static VmaAllocator mem_allocator_;
  static int context_ref_count_;
  static VkDevice mem_device_;

 public:
  VKBackend();

  virtual ~VKBackend()
  {
    BLI_assert(context_ref_count_ == 0);
    VKBackend::platform_exit();
  }

  void delete_resources() override;

  void samplers_update() override;
  void compute_dispatch(int groups_x_len, int groups_y_len, int groups_z_len) override;
  void compute_dispatch_indirect(StorageBuf *indirect_buf) override;

  Context *context_alloc(void *ghost_window, void *ghost_context) override;

  Batch *batch_alloc() override;
  DrawList *drawlist_alloc(int list_length) override;
  Fence *fence_alloc() override;
  FrameBuffer *framebuffer_alloc(const char *name) override;
  IndexBuf *indexbuf_alloc() override;
  PixelBuffer *pixelbuf_alloc(uint size) override;
  QueryPool *querypool_alloc() override;
  Shader *shader_alloc(const char *name) override;
  Texture *texture_alloc(const char *name) override;
  UniformBuf *uniformbuf_alloc(int size, const char *name) override;
  StorageBuf *storagebuf_alloc(int size, GPUUsageType usage, const char *name) override;
  VertBuf *vertbuf_alloc() override;
  VmaAllocator* vma_alloc(VKContext* context);
  /* Render Frame Coordination --
   * Used for performing per-frame actions globally */
  void render_begin() override;
  void render_end() override;
  void render_step() override;

  bool debug_capture_begin(VkInstance vk_instance);
  void debug_capture_end(VkInstance vk_instance);
  void debug_capture_title(const char* title);

  shaderc::Compiler &get_shaderc_compiler();

  VmaAllocator &mem_allocator_get();

  VkDevice &mem_device_get()
  {
    return mem_device_;
  };

  static void capabilities_init(VKContext &context);
  static bool device_extensions_support( const char * extension_needed,Vector<VkExtensionProperties>& vk_extension_properties_);

  static VKBackend &get()
  {
    return *static_cast<VKBackend *>(GPUBackend::get());
  }

  template<typename T> static void desable_gpuctx(VKContext *context, T &descriptor_pools_);
  static bool exist_window();
 private:
  static void init_platform();
  static void platform_exit();
  static VKContext *gpuctx_;
};

}  // namespace blender::gpu
