/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once
#include "MEM_guardedalloc.h"
#include "gpu_framebuffer_private.hh"
#include "vk_context.hh"
#include "vk_texture.hh"

namespace blender::gpu {

class VKStateManager;

/**
 * Implementation of FrameBuffer object using OpenGL.
 */
class VKFrameBuffer : public FrameBuffer {
  /* For debugging purpose. */
  friend class VKTexture;

 private:
  /** OpenGL handle. */
  GLuint fbo_id_ = 0;
  /** Context the handle is from. Frame-buffers are not shared across contexts. */
  VKContext *context_ = nullptr;
  /** State Manager of the same contexts. */
  VKStateManager *state_manager_ = nullptr;

  /** Internal frame-buffers are immutable. */
  bool immutable_;
  /** True is the frame-buffer has its first color target using the GPU_SRGB8_A8 format. */
  bool srgb_;
  /** True is the frame-buffer has been bound using the GL_FRAMEBUFFER_SRGB feature. */
  bool enabled_srgb_ = false;

  int flight_ticket_ = -1;
  bool is_render_begin_ = false;
  bool is_command_begin_ = false;
  Vector<VkSemaphore> submit_signal_;
  bool is_offscreen_signaled_ = false;

 public:
  bool is_dirty_render_ = false;
  /**
   * Create a conventional frame-buffer to attach texture to.
   */
  VKFrameBuffer(const char *name, VKContext *ctx);

  /**
   * Special frame-buffer encapsulating internal window frame-buffer.
   *  (i.e.: #GL_FRONT_LEFT, #GL_BACK_RIGHT, ...)
   * \param ctx: Context the handle is from.
   * \param target: The internal GL name (i.e: #GL_BACK_LEFT).
   * \param fbo: The (optional) already created object for some implementation. Default is 0.
   * \param w: Buffer width.
   * \param h: Buffer height.
   */
  VKFrameBuffer(const char *name, VKContext *ctx, int w, int h);

  ~VKFrameBuffer();

  void bind(bool enabled_srgb) override;

  /**
   * This is a rather slow operation. Don't check in normal cases.
   */
  bool check(char err_out[256]) override;

  void clear(eGPUFrameBufferBits buffers,
             const float clear_col[4],
             float clear_depth,
             uint clear_stencil) override;
  void clear_multi(const float (*clear_cols)[4]) override;
  void clear_attachment(GPUAttachmentType type,
                        eGPUDataFormat data_format,
                        const void *clear_value) override;

  /* Attachment load-stores are currently no-op's in OpenGL. */
  void attachment_set_loadstore_op(GPUAttachmentType /*type*/,
                                   eGPULoadOp /*load_action*/,
                                   eGPUStoreOp /*store_action*/) override;

  void read(eGPUFrameBufferBits planes,
            eGPUDataFormat format,
            const int area[4],
            int channel_len,
            int slot,
            void *r_data) override;

  /**
   * Copy \a src at the give offset inside \a dst.
   */
  void blit(uint read_slot,
            uint src_x_offset,
            uint src_y_offset,
            VKFrameBuffer *metal_fb_write,
            uint write_slot,
            uint dst_x_offset,
            uint dst_y_offset,
            uint width,
            uint height,
            eGPUFrameBufferBits blit_buffers);

  void blit_to(eGPUFrameBufferBits planes,
               int src_slot,
               FrameBuffer *dst,
               int dst_slot,
               int dst_offset_x,
               int dst_offset_y) override;

  void apply_state();

  int get_width();

  int get_height();

  bool get_srgb_enabled()
  {
    return srgb_enabled_;
  };

  bool get_is_srgb()
  {
    return is_srgb_;
  };

  void set_srgb(bool t)
  {
    srgb_ = t;
  };

  VkCommandBuffer render_begin(VkCommandBuffer cmd,
                               VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
                               VkClearValue *clearValues = nullptr,
                               bool blit = false);
  void render_end();
  bool is_render_begin()
  {
    return is_render_begin_;
  }

  VkSemaphore get_signal()
  {
    return submit_signal_[signal_index_];
  }

  void create_swapchain_frame_buffer(int);
  VkCommandBuffer get_command()
  {

    if (is_command_begin_) {
      cmd_refs++;
      return vk_cmd;
    }

    return VK_NULL_HANDLE;
  };
  VkRenderPass get_render_pass()
  {
    return vk_attachments_.renderpass_;
  }
  VkImage get_swapchain_image()
  {
    BLI_assert(is_swapchain_);
    return context_->get_current_image();
  }
  VkImageLayout get_swapchain_image_layout()
  {
    BLI_assert(is_swapchain_);
    return context_->get_current_image_layout();
  }
  void set_swapchain_image_layout(VkImageLayout layout)
  {
    context_->set_current_image_layout(layout);
  };

  void append_wait_semaphore(VkSemaphore sema)
  {

    wait_sema.push_back(sema);
  };

  void set_dirty_render(bool t)
  {
    is_dirty_render_ = t;
  };

  void save_current_frame(const char *filename);

 private:
  VkCommandBuffer vk_cmd = VK_NULL_HANDLE;
  int cmd_refs = 0;
  VKAttachment vk_attachments_;
  bool is_nocolor_ = true;
  bool is_init_ = false;
  bool is_swapchain_ = false;
  bool is_blit_begin_ = false;
  int signal_index_ = -1;
  int offscreen_render_times_ = 0;

  std::vector<VkSemaphore> wait_sema;

  bool use_multilayered_rendering_ = false;

  /** Whether SRGB is enabled for this frame-buffer configuration. */
  bool srgb_enabled_;
  /** Whether the primary Frame-buffer attachment is an SRGB target or not. */
  bool is_srgb_;

  void init(VKContext *ctx);
  void update_attachments();
  void force_clear();

  VKContext *dirty_state_ctx_ = nullptr;

  MEM_CXX_CLASS_ALLOC_FUNCS("VKFrameBuffer");
};

/* -------------------------------------------------------------------- */
/** \name Enums Conversion
 * \{ */

template<typename T = GPUAttachmentType> VkImageLayout to_vk(const T type)
{
  switch (type) {
    case GPU_FB_DEPTH_ATTACHMENT:
      return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    case GPU_FB_DEPTH_STENCIL_ATTACHMENT:
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case GPU_FB_COLOR_ATTACHMENT0:
    case GPU_FB_COLOR_ATTACHMENT1:
    case GPU_FB_COLOR_ATTACHMENT2:
    case GPU_FB_COLOR_ATTACHMENT3:
    case GPU_FB_COLOR_ATTACHMENT4:
    case GPU_FB_COLOR_ATTACHMENT5:
    case GPU_FB_COLOR_ATTACHMENT6:
    case GPU_FB_COLOR_ATTACHMENT7:
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    default:
      BLI_assert(0);
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  return VK_IMAGE_LAYOUT_UNDEFINED;
}

static inline VkImageAspectFlags to_vk(const eGPUFrameBufferBits bits)
{
  VkImageAspectFlags mask = 0x00;
  mask |= (bits & GPU_DEPTH_BIT) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
  mask |= (bits & GPU_STENCIL_BIT) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
  mask |= (bits & GPU_COLOR_BIT) ? VK_IMAGE_ASPECT_COLOR_BIT : 0;
  return mask;
}

/** \} */

}  // namespace blender::gpu
