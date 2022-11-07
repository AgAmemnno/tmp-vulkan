/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_framebuffer_private.hh"
#include "vk_context.hh"
namespace blender::gpu {

class VKFrameBuffer : public FrameBuffer {
  private:
  /** OpenGL handle. */
  //GLuint fbo_id_ = 0;
  /** Context the handle is from. Frame-buffers are not shared across contexts. */
  VKContext *context_ = nullptr;
  /** State Manager of the same contexts. */
  //GLStateManager *state_manager_ = nullptr;
  /** Copy of the GL state. Contains ONLY color attachments enums for slot binding. */
  //GLenum gl_attachments_[GPU_FB_MAX_COLOR_ATTACHMENT] = {0};
  /** Internal frame-buffers are immutable. */
  bool immutable_;
  /** True is the frame-buffer has its first color target using the GPU_SRGB8_A8 format. */
  bool srgb_;
  /** True is the frame-buffer has been bound using the GL_FRAMEBUFFER_SRGB feature. */
  bool enabled_srgb_ = false;

 public:
  VKFrameBuffer(const char *name) : FrameBuffer(name)
  {
  }

  VKFrameBuffer(
    const char *name, VKContext *ctx, GLenum target, GLuint fbo, int w, int h)
    : FrameBuffer(name)
  {

  context_ = ctx;
  //state_manager_ = static_cast<GLStateManager *>(ctx->state_manager);
  immutable_ = true;
  //fbo_id_ = fbo;
  //gl_attachments_[0] = target;
  /* Never update an internal frame-buffer. */
  dirty_attachments_ = false;
  width_ = w;
  height_ = h;
  srgb_ = false;
  
  viewport_[0] = scissor_[0] = 0;
  viewport_[1] = scissor_[1] = 0;
  viewport_[2] = scissor_[2] = w;
  viewport_[3] = scissor_[3] = h;

  //if (fbo_id_) {
  //  debug::object_label(GL_FRAMEBUFFER, fbo_id_, name_);
  //}
  }

  void bind(bool enabled_srgb) override;
  bool check(char err_out[256]) override;
  void clear(eGPUFrameBufferBits buffers,
             const float clear_col[4],
             float clear_depth,
             uint clear_stencil) override;
  void clear_multi(const float (*clear_col)[4]) override;
  void clear_attachment(GPUAttachmentType type,
                        eGPUDataFormat data_format,
                        const void *clear_value) override;

  void attachment_set_loadstore_op(GPUAttachmentType type,
                                   eGPULoadOp load_action,
                                   eGPUStoreOp store_action) override;

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
};

}  // namespace blender::gpu
