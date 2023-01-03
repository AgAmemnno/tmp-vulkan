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
  /** Copy of the GL state. Contains ONLY color attachments enums for slot binding. */
  VkImageLayout vk_attachments_layout_[GPU_FB_MAX_COLOR_ATTACHMENT] = {VK_IMAGE_LAYOUT_UNDEFINED};
  /** Internal frame-buffers are immutable. */
  bool immutable_;
  /** True is the frame-buffer has its first color target using the GPU_SRGB8_A8 format. */
  bool srgb_;
  /** True is the frame-buffer has been bound using the GL_FRAMEBUFFER_SRGB feature. */
  bool enabled_srgb_ = false;

public:
  /* State. */
  /* Flag MTLFramebuffer configuration as having changed. */
  void mark_dirty();
  void mark_loadstore_dirty();
  /* Mark that a pending clear has been performed. */
  void mark_cleared();
  /* Mark that we have a pending clear. */
  void mark_do_clear();

 void  update_attachments(bool update_viewport);


 /** \ Adding and Removing attachments
  * \{ */

 bool add_color_attachment(VKTexture *texture,
                                           uint slot,
                                           int miplevel,
                                           int layer);

 bool add_depth_attachment(VKTexture *texture, int miplevel, int layer);

 bool add_stencil_attachment(VKTexture *texture, int miplevel, int layer);


 /* Fetch values */
 bool has_attachment_at_slot(uint slot);
 bool has_color_attachment_with_texture(VKTexture *texture);
 bool has_depth_attachment();
 bool has_stencil_attachment();
 int get_color_attachment_slot_from_texture(VKTexture *texture);
 uint get_attachment_count();
 uint get_attachment_limit()
 {
   return GPU_FB_MAX_COLOR_ATTACHMENT;
 };
 VKAttachment get_color_attachment(uint slot);
 VKAttachment get_depth_attachment();
 VKAttachment get_stencil_attachment();

   bool remove_color_attachment(uint slot);
 bool remove_depth_attachment();
 bool remove_stencil_attachment();
 void remove_all_attachments();
 void ensure_render_target_size();

   /* Remove any pending clears - Ensure "load" configuration is used. */
 bool reset_clear_state();

 /* Clear values -> Load/store actions. */
 bool set_color_attachment_clear_color(uint slot, const float clear_color[4]);
 bool set_depth_attachment_clear_value(float depth_clear);
 bool set_stencil_attachment_clear_value(uint stencil_clear);
 bool set_color_loadstore_op(uint slot, eGPULoadOp load_action, eGPUStoreOp store_action);
 bool set_depth_loadstore_op(eGPULoadOp load_action, eGPUStoreOp store_action);
 bool set_stencil_loadstore_op(eGPULoadOp load_action, eGPUStoreOp store_action);


  /* Metal Attachment properties. */
  uint colour_attachment_count_;
  VKAttachment vk_color_attachments_[GPU_FB_MAX_COLOR_ATTACHMENT];
  VKAttachment vk_depth_attachment_;
  VKAttachment vk_stencil_attachment_;
  bool use_multilayered_rendering_ = false;

  /* State. */

  /**
   * Whether global frame-buffer properties have changed and require
   * re-generation of #MTLRenderPassDescriptor / #RenderCommandEncoders.
   */
  bool is_dirty_;

  /** Whether `loadstore` properties have changed (only affects certain cached configurations). */
  bool is_loadstore_dirty_;

  /**
   * Context that the latest modified state was last applied to.
   * If this does not match current ctx, re-apply state.
   */
  VKContext *dirty_state_ctx_;

  /**
   * Whether a clear is pending -- Used to toggle between clear and load FB configurations
   * (without dirtying the state) - Frame-buffer load config is used if no `GPU_clear_*` command
   * was issued after binding the #FrameBuffer.
   */
  bool has_pending_clear_;

  typedef enum {
    VK_FB_CONFIG_CLEAR = 0,
    VK_FB_CONFIG_LOAD = 1,
    VK_FB_CONFIG_CUSTOM = 2
  } VK_FB_CONFIG;
#define VK_FB_CONFIG_MAX (VK_FB_CONFIG_CUSTOM + 1)

  ///MTLRenderPassDescriptor *framebuffer_descriptor_[MTL_FB_CONFIG_MAX];
  VkFramebufferCreateInfo framebuffer_descriptor_[VK_FB_CONFIG_MAX];
  Vector<VkAttachmentDescription> attachment_descriptors_[VK_FB_CONFIG_MAX];
  

  Vector<VkImageView> attachments[VK_FB_CONFIG_MAX];
  Vector<VkAttachmentReference> attachment_references[VK_FB_CONFIG_MAX];

  VkRenderPassCreateInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
 
  /** Whether `MTLRenderPassDescriptor[N]` requires updating with latest state. */
  bool descriptor_dirty_[VK_FB_CONFIG_MAX];
  /** Whether SRGB is enabled for this frame-buffer configuration. */
  bool srgb_enabled_;
  /** Whether the primary Frame-buffer attachment is an SRGB target or not. */
  bool is_srgb_;

 public:
  /**
   * Create a conventional frame-buffer to attach texture to.
   */
  VKFrameBuffer(const char *name);

  /**
   * Special frame-buffer encapsulating internal window frame-buffer.
   *  (i.e.: #GL_FRONT_LEFT, #GL_BACK_RIGHT, ...)
   * \param ctx: Context the handle is from.
   * \param target: The internal GL name (i.e: #GL_BACK_LEFT).
   * \param fbo: The (optional) already created object for some implementation. Default is 0.
   * \param w: Buffer width.
   * \param h: Buffer height.
   */
  ///VKFrameBuffer(const char *name, VKContext *ctx, GLenum target, GLuint fbo, int w, int h);
  VKFrameBuffer(VKContext *ctx, const char *name);
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
  bool get_dirty()
  {
    return is_dirty_ || is_loadstore_dirty_;
  }


  bool get_pending_clear()
  {
    return has_pending_clear_;
  };

  bool get_srgb_enabled()
  {
    return srgb_enabled_;
  };

  bool get_is_srgb()
  {
    return is_srgb_;
  };

 private:
  void init();
  void update_attachments();
  void update_drawbuffers();
  void force_clear();
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


