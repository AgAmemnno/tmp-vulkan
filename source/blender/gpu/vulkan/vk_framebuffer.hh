/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_math_vector.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "gpu_framebuffer_private.hh"

#include "vk_common.hh"

namespace blender::gpu {

class VKFrameBuffer : public FrameBuffer {
 private:
  /* Vulkan object handle. */
  VkFramebuffer vk_framebuffer_ = VK_NULL_HANDLE;
  /* Vulkan device who created the handle. */
  VkDevice vk_device_ = VK_NULL_HANDLE;
  /* Base render pass used for framebuffer creation. */
  VkRenderPass vk_render_pass_ = VK_NULL_HANDLE;
  VkImageLayout vk_render_pass_init_ = VK_IMAGE_LAYOUT_MAX_ENUM;
  /* Number of layers if the attachments are layered textures. */
  int depth_ = 1;
  /** Internal frame-buffers are immutable. */
  bool immutable_;

  VkClearAttachment immutable_attachment_;

  int image_id_ = -1;

 public:
  /**
   * Create a conventional framebuffer to attach texture to.
   **/
  VKFrameBuffer(const char *name);

  /**
   * Special frame-buffer encapsulating internal window frame-buffer.
   * This just act as a wrapper, the actual allocations are done by GHOST_ContextVK.
   **/
  VKFrameBuffer(const char *name,
                VkFramebuffer vk_framebuffer,
                VkRenderPass vk_render_pass,
                VkExtent2D vk_extent);

  ~VKFrameBuffer();

  void set_dirty()
  {
    dirty_state_ = true;
  };
  void bind(bool enabled_srgb) override;
  bool check(char err_out[256]) override;
  void clear(eGPUFrameBufferBits buffers,
             const float clear_color[4],
             float clear_depth,
             uint clear_stencil) override;
  void clear_multi(const float (*clear_color)[4]) override;
  void clear_attachment(GPUAttachmentType type,
                        eGPUDataFormat data_format,
                        const void *clear_value) override;

  void attachment_set_loadstore_op(GPUAttachmentType type,
                                   eGPULoadOp load_action,
                                   eGPUStoreOp store_action) override;

  void apply_state();

  void read(eGPUFrameBufferBits planes,
            eGPUDataFormat format,
            const int area[4],
            int channel_len,
            int slot,
            void *r_data) override;

  void blit_to(eGPUFrameBufferBits planes,
               int src_slot,
               FrameBuffer *dst,
               int dst_slot,
               int dst_offset_x,
               int dst_offset_y) override;

  VkFramebuffer vk_framebuffer_get() const
  {
    BLI_assert(vk_framebuffer_ != VK_NULL_HANDLE);
    return vk_framebuffer_;
  }

  VkRenderPass vk_render_pass_get() const
  {
    BLI_assert(vk_render_pass_ != VK_NULL_HANDLE);
    return vk_render_pass_;
  }
  VkViewport vk_viewport_get() const;
  VkRect2D vk_render_area_get() const;
  void set_image_id(int i)
  {
    image_id_ = i;
  }
  int get_image_id()
  {
    return image_id_;
  }
  VkImageLayout vk_render_pass_init_layout_get() const
  {
    return vk_render_pass_init_;
  }
  bool is_immutable()
  {
    return immutable_;
  };

  VkFormat is_color(int slot) const;

 private:
  void update_attachments();
  void render_pass_free();
  void render_pass_create();

  /* Clearing attachments */
  void build_clear_attachments_depth_stencil(eGPUFrameBufferBits buffers,
                                             float clear_depth,
                                             uint32_t clear_stencil,
                                             Vector<VkClearAttachment> &r_attachments) const;
  void build_clear_attachments_color(const float (*clear_colors)[4],
                                     const bool multi_clear_colors,
                                     Vector<VkClearAttachment> &r_attachments) const;
  void clear(const Vector<VkClearAttachment> &attachments);
};

static inline VKFrameBuffer *unwrap(FrameBuffer *framebuffer)
{
  return static_cast<VKFrameBuffer *>(framebuffer);
}

}  // namespace blender::gpu
