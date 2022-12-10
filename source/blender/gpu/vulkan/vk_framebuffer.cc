/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_texture.hh"
#include "vk_framebuffer.hh"

namespace blender::gpu {
 #define VK_PNEXT_INIT_VAL 0x7777777
#define SET_VK_PNEXT_INIT(str) str.pNext = (const void*)VK_PNEXT_INIT_VAL;
#define IS_VK_PNEXT_INIT(str) (VK_PNEXT_INIT_VAL==(uint64_t)str.pNext)
#define SET_VK_PNEXT(str) str.pNext = NULL;
int VKFrameBuffer::get_width()
{
  return width_;
}
int VKFrameBuffer::get_height()
{
  return height_;
}
/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

VKFrameBuffer::VKFrameBuffer(const char *name) : FrameBuffer(name)
{
  /* Just-In-Time init. See #GLFrameBuffer::init(). */
  immutable_ = false;
  fbo_id_ = 0;
}


VKFrameBuffer::VKFrameBuffer(VKContext *ctx, const char *name)
    ///const char *name, VKContext *ctx, GLenum target, GLuint fbo, int w, int h)
    : FrameBuffer(name)
{
 

  context_ = ctx;
  is_dirty_ = true;
  is_loadstore_dirty_ = true;
  dirty_state_ctx_ = nullptr;
  has_pending_clear_ = false;
  colour_attachment_count_ = 0;
  srgb_enabled_ = false;
  is_srgb_ = false;

  for (int i = 0; i < GPU_FB_MAX_COLOR_ATTACHMENT; i++) {
    vk_color_attachments_[i].used = false;
  }
  vk_depth_attachment_.used = false;
  vk_stencil_attachment_.used = false;

  for (int i = 0; i < VK_FB_CONFIG_MAX; i++) {
    framebuffer_descriptor_[i].sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_descriptor_[i].pNext = NULL;
    descriptor_dirty_[i] = true;
  };

   


  /* Initial state. */
  this->size_set(0, 0);
  this->viewport_reset();
  this->scissor_reset();
}
VKFrameBuffer::~VKFrameBuffer()
{

  /* If FrameBuffer is associated with a currently open RenderPass, end. 
  if (context_->main_command_buffer.get_active_framebuffer() == this) {
    context_->main_command_buffer.end_active_command_encoder();
  }
  */

  /* Restore default frame-buffer if this frame-buffer was bound. */
  if (context_->active_fb == this && context_->back_left != this) {
    /* If this assert triggers it means the frame-buffer is being freed while in use by another
     * context which, by the way, is TOTALLY UNSAFE!!!  (Copy from GL behavior). */
    BLI_assert(context_ == static_cast<VKContext *>(unwrap(GPU_context_active_get())));
    GPU_framebuffer_restore();
  }

  this->remove_all_attachments();

  if (context_ == nullptr) {
    return;
  }
  
}

void VKFrameBuffer::init()
{
  context_ = VKContext::get();
  state_manager_ = (VKStateManager *)(context_->state_manager);
  for (int i = 0; i < VK_FB_CONFIG_MAX; i++) {
    framebuffer_descriptor_[i].sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    SET_VK_PNEXT_INIT(framebuffer_descriptor_[i]);
  };
}


/** \} */


void VKFrameBuffer::update_attachments()
{
}




void VKFrameBuffer::mark_dirty()
{
  is_dirty_ = true;
  is_loadstore_dirty_ = true;
}



void VKFrameBuffer::mark_loadstore_dirty()
{
  is_loadstore_dirty_ = true;
}

void VKFrameBuffer::mark_cleared()
{
  has_pending_clear_ = false;
}

void VKFrameBuffer::mark_do_clear()
{
  has_pending_clear_ = true;
}

uint VKFrameBuffer::get_attachment_count()
{
  BLI_assert(this);
  return colour_attachment_count_;
}
bool VKFrameBuffer::has_depth_attachment()
{
  BLI_assert(this);
  return vk_depth_attachment_.used;
}




bool VKFrameBuffer::add_color_attachment(VKTexture *texture,
                                          uint slot,
                                          int miplevel,
                                          int layer)
{
  BLI_assert(this);
  BLI_assert(slot >= 0 && slot < this->get_attachment_limit());


  return true;
}

bool VKFrameBuffer::add_depth_attachment(gpu::VKTexture *texture, int miplevel, int layer)
{
  BLI_assert(this);

  return true;
}

bool VKFrameBuffer::add_stencil_attachment(gpu::VKTexture *texture, int miplevel, int layer)
{
  BLI_assert(this);


  return true;
}


bool VKFrameBuffer::remove_color_attachment(uint slot)
{
  BLI_assert(this);
  BLI_assert(slot >= 0 && slot < this->get_attachment_limit());


  return false;
}

bool VKFrameBuffer::remove_depth_attachment()
{
  BLI_assert(this);


  return true;
}

bool VKFrameBuffer::remove_stencil_attachment()
{
  BLI_assert(this);



  return true;
}


void VKFrameBuffer::ensure_render_target_size()
{
  /* If we have no attachments, reset width and height to zero. */
  if (colour_attachment_count_ == 0 && !this->has_depth_attachment() &&
      !this->has_stencil_attachment()) {

    /* Reset Viewport and Scissor for NULL framebuffer. */
    this->size_set(0, 0);
    this->scissor_reset();
    this->viewport_reset();
  }
}

void VKFrameBuffer::remove_all_attachments()
{
  BLI_assert(this);

  for (int attachment = 0; attachment < GPU_FB_MAX_COLOR_ATTACHMENT; attachment++) {
    this->remove_color_attachment(attachment);
  }
  this->remove_depth_attachment();
  this->remove_stencil_attachment();
  colour_attachment_count_ = 0;
  this->mark_dirty();

  /* Verify height. */
  this->ensure_render_target_size();

  /* Flag attachments as no longer being dirty. */
  dirty_attachments_ = false;
}

bool VKFrameBuffer::has_stencil_attachment()
{
  BLI_assert(this);
  return vk_stencil_attachment_.used;
}
/** \} */
void VKFrameBuffer::update_attachments(bool update_viewport)
{
  if (!dirty_attachments_) {
    return;
  }




  }


/* -------------------------------------------------------------------- */


void VKFrameBuffer::attachment_set_loadstore_op(GPUAttachmentType type,
                                                 eGPULoadOp load_action,
                                                 eGPUStoreOp store_action)
{
  if (type >= GPU_FB_COLOR_ATTACHMENT0) {
    int slot = type - GPU_FB_COLOR_ATTACHMENT0;
    this->set_color_loadstore_op(slot, load_action, store_action);
  }
  else if (type == GPU_FB_DEPTH_STENCIL_ATTACHMENT) {
    this->set_depth_loadstore_op(load_action, store_action);
    this->set_stencil_loadstore_op(load_action, store_action);
  }
  else if (type == GPU_FB_DEPTH_ATTACHMENT) {
    this->set_depth_loadstore_op(load_action, store_action);
  }
}

bool VKFrameBuffer::set_color_attachment_clear_color(uint slot, const float clear_color[4])
{
  BLI_assert(this);
  BLI_assert(slot >= 0 && slot < this->get_attachment_limit());


  return true;
}

bool VKFrameBuffer::set_depth_attachment_clear_value(float depth_clear)
{
  BLI_assert(this);

  if (vk_depth_attachment_.clear_value.depth != depth_clear ||
      vk_depth_attachment_.load_action != GPU_LOADACTION_CLEAR) {
    vk_depth_attachment_.clear_value.depth = depth_clear;
    vk_depth_attachment_.load_action = GPU_LOADACTION_CLEAR;
    this->mark_loadstore_dirty();
  }
  return true;
}

bool VKFrameBuffer::set_stencil_attachment_clear_value(uint stencil_clear)
{
  BLI_assert(this);

  if (vk_stencil_attachment_.clear_value.stencil != stencil_clear ||
      vk_stencil_attachment_.load_action != GPU_LOADACTION_CLEAR) {
    vk_stencil_attachment_.clear_value.stencil = stencil_clear;
    vk_stencil_attachment_.load_action = GPU_LOADACTION_CLEAR;
    this->mark_loadstore_dirty();
  }
  return true;
}

bool VKFrameBuffer::set_color_loadstore_op(uint slot,
                                            eGPULoadOp load_action,
                                            eGPUStoreOp store_action)
{
  BLI_assert(this);
  eGPULoadOp prev_load_action = vk_color_attachments_[slot].load_action;
  eGPUStoreOp prev_store_action = vk_color_attachments_[slot].store_action;
  vk_color_attachments_[slot].load_action = load_action;
  vk_color_attachments_[slot].store_action = store_action;

  bool changed = (vk_color_attachments_[slot].load_action != prev_load_action ||
                  vk_color_attachments_[slot].store_action != prev_store_action);
  if (changed) {
    this->mark_loadstore_dirty();
  }

  return changed;
}

bool VKFrameBuffer::set_depth_loadstore_op(eGPULoadOp load_action, eGPUStoreOp store_action)
{
  BLI_assert(this);
  eGPULoadOp prev_load_action = vk_depth_attachment_.load_action;
  eGPUStoreOp prev_store_action = vk_depth_attachment_.store_action;
  vk_depth_attachment_.load_action = load_action;
  vk_depth_attachment_.store_action = store_action;

  bool changed = (vk_depth_attachment_.load_action != prev_load_action ||
                  vk_depth_attachment_.store_action != prev_store_action);
  if (changed) {
    this->mark_loadstore_dirty();
  }

  return changed;
}

bool VKFrameBuffer::set_stencil_loadstore_op(eGPULoadOp load_action, eGPUStoreOp store_action)
{
  BLI_assert(this);
  eGPULoadOp prev_load_action = vk_stencil_attachment_.load_action;
  eGPUStoreOp prev_store_action = vk_stencil_attachment_.store_action;
  vk_stencil_attachment_.load_action = load_action;
  vk_stencil_attachment_.store_action = store_action;

  bool changed = (vk_stencil_attachment_.load_action != prev_load_action ||
                  vk_stencil_attachment_.store_action != prev_store_action);
  if (changed) {
    this->mark_loadstore_dirty();
  }

  return changed;
}





bool VKFrameBuffer::reset_clear_state()
{
  for (int slot = 0; slot < colour_attachment_count_; slot++) {
    this->set_color_loadstore_op(slot, GPU_LOADACTION_LOAD, GPU_STOREACTION_STORE);
  }
  this->set_depth_loadstore_op(GPU_LOADACTION_LOAD, GPU_STOREACTION_STORE);
  this->set_stencil_loadstore_op(GPU_LOADACTION_LOAD, GPU_STOREACTION_STORE);
  return true;
}

void VKFrameBuffer::bind(bool enabled_srgb)
{

  /* Verify Context is valid. */
  if (context_ != static_cast<VKContext *>(unwrap(GPU_context_active_get()))) {
    BLI_assert(false && "Trying to use the same frame-buffer in multiple context's.");
    return;
  }

  /* Ensure SRGB state is up-to-date and valid. */
  bool srgb_state_changed = srgb_enabled_ != enabled_srgb;
  if (context_->active_fb != this || srgb_state_changed) {
    if (srgb_state_changed) {
      this->mark_dirty();
    }
    srgb_enabled_ = enabled_srgb;
    GPU_shader_set_framebuffer_srgb_target(srgb_enabled_ && is_srgb_);
  }

  /* Ensure local MTLAttachment data is up to date. */
  this->update_attachments(true);

  /* Reset clear state on bind -- Clears and load/store ops are set after binding. */
  this->reset_clear_state();

  /* Bind to active context. */
  VKContext *vk_context = reinterpret_cast<VKContext *>(GPU_context_active_get());
  if (vk_context) {
    vk_context->framebuffer_bind(this);
    dirty_state_ = true;
  }
  else {
    GPU_VK_DEBUG_PRINTF("Attempting to bind FrameBuffer, but no context is active\n");
  }

}
bool VKFrameBuffer::check(char err_out[256])
{
  /* Ensure local MTLAttachment data is up to date. */
  this->update_attachments(true);

  /* Ensure there is at least one attachment. */
  bool valid = (this->get_attachment_count() > 0 ||
                this->has_depth_attachment() | this->has_stencil_attachment());
  if (!valid) {
    const char *format = "Framebuffer %s does not have any attachments.\n";
    if (err_out) {
      BLI_snprintf(err_out, 256, format, name_);
    }
    else {
      GPU_VK_DEBUG_PRINTF(format, name_);
    }
    return false;
  }

  /* Ensure all attachments have identical dimensions. */
  /* Ensure all attachments are render-targets. */
  bool first = true;
  uint dim_x = 0;
  uint dim_y = 0;
  for (int col_att = 0; col_att < this->get_attachment_count(); col_att++) {
    VKAttachment att = this->get_color_attachment(col_att);
    if (att.used) {
      if (att.texture->gpu_image_usage_flags_ & GPU_TEXTURE_USAGE_ATTACHMENT) {
        if (first) {
          dim_x = att.texture->width_get();
          dim_y = att.texture->height_get();
          first = false;
        }
        else {
          if (dim_x != att.texture->width_get() || dim_y != att.texture->height_get()) {
            const char *format =
                "Framebuffer %s: Color attachment dimensions do not match those of previous "
                "attachment\n";
            if (err_out) {
              BLI_snprintf(err_out, 256, format, name_);
            }
            else {
              fprintf(stderr, format, name_);
             
            }
            return false;
          }
        }
      }
      else {
        const char *format =
            "Framebuffer %s: Color attachment texture does not have usage flag "
            "'GPU_TEXTURE_USAGE_ATTACHMENT'\n";
        if (err_out) {
          BLI_snprintf(err_out, 256, format, name_);
        }
        else {
          fprintf(stderr, format, name_);
          ///MTL_LOG_ERROR(format, name_);
        }
        return false;
      }
    }
  }
  VKAttachment depth_att = this->get_depth_attachment();
  VKAttachment stencil_att = this->get_stencil_attachment();
  if (depth_att.used) {
    if (first) {
      dim_x = depth_att.texture->width_get();
      dim_y = depth_att.texture->height_get();
      first = false;
      valid = (depth_att.texture->gpu_image_usage_flags_ & GPU_TEXTURE_USAGE_ATTACHMENT);

      if (!valid) {
        const char *format =
            "Framebuffer %n: Depth attachment does not have usage "
            "'GPU_TEXTURE_USAGE_ATTACHMENT'\n";
        if (err_out) {
          BLI_snprintf(err_out, 256, format, name_);
        }
        else {
          fprintf(stderr, format, name_);
          ///MTL_LOG_ERROR(format, name_);
        }
        return false;
      }
    }
    else {
      if (dim_x != depth_att.texture->width_get() || dim_y != depth_att.texture->height_get()) {
        const char *format =
            "Framebuffer %n: Depth attachment dimensions do not match that of previous "
            "attachment\n";
        if (err_out) {
          BLI_snprintf(err_out, 256, format, name_);
        }
        else {
          fprintf(stderr, format, name_);

        }
        return false;
      }
    }
  }
  if (stencil_att.used) {
    if (first) {
      dim_x = stencil_att.texture->width_get();
      dim_y = stencil_att.texture->height_get();
      first = false;
      valid = (stencil_att.texture->gpu_image_usage_flags_ & GPU_TEXTURE_USAGE_ATTACHMENT);
      if (!valid) {
        const char *format =
            "Framebuffer %s: Stencil attachment does not have usage "
            "'GPU_TEXTURE_USAGE_ATTACHMENT'\n";
        if (err_out) {
          BLI_snprintf(err_out, 256, format, name_);
        }
        else {
          fprintf(stderr, format, name_);
          ///MTL_LOG_ERROR(format, name_);
        }
        return false;
      }
    }
    else {
      if (dim_x != stencil_att.texture->width_get() ||
          dim_y != stencil_att.texture->height_get()) {
        const char *format =
            "Framebuffer %s: Stencil attachment dimensions do not match that of previous "
            "attachment";
        if (err_out) {
          BLI_snprintf(err_out, 256, format, name_);
        }
        else {
          fprintf(stderr, format, name_);
          ///MTL_LOG_ERROR(format, name_);
        }
        return false;
      }
    }
  }

  BLI_assert(valid);
  return valid;
}
void VKFrameBuffer::force_clear()
{

}

void VKFrameBuffer::clear(eGPUFrameBufferBits buffers,
                           const float clear_col[4],
                           float clear_depth,
                           uint clear_stencil)
{



}


void VKFrameBuffer::apply_state()
{
  VKContext *vk_ctx = static_cast<VKContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(vk_ctx);
  if (vk_ctx->active_fb == this) {
    if (dirty_state_ == false && dirty_state_ctx_ == vk_ctx) {
      return;
    }

    /* Ensure viewport has been set. NOTE: This should no longer happen, but kept for safety to
     * track bugs. */
    if (viewport_[2] == 0 || viewport_[3] == 0) {
      GPU_VK_DEBUG_PRINTF(
          "Viewport had width and height of (0,0) -- Updating -- DEBUG Safety check\n");
      viewport_reset();
    }



    /// Update Context State.
    vk_ctx->set_viewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
    vk_ctx->set_scissor(scissor_[0], scissor_[1], scissor_[2], scissor_[3]);
    vk_ctx->set_scissor_enabled(scissor_test_);

    dirty_state_ = false;
    dirty_state_ctx_ = vk_ctx;
  }
  else {
    GPU_VK_DEBUG_PRINTF(
        "Attempting to set FrameBuffer State (VIEWPORT, SCISSOR), But FrameBuffer is not bound to "
        "current Context.\n");
  }
}


void VKFrameBuffer::clear_attachment(GPUAttachmentType type,
                                     eGPUDataFormat data_format,
                                     const void *clear_value)
{
}


void VKFrameBuffer::clear_multi(const float (*clear_cols)[4])
{

}



void VKFrameBuffer::read(eGPUFrameBufferBits planes,
                          eGPUDataFormat format,
                          const int area[4],
                          int channel_len,
                          int slot,
                          void *r_data)
{

  BLI_assert((planes & GPU_STENCIL_BIT) == 0);
  BLI_assert(area[2] > 0);
  BLI_assert(area[3] > 0);

  switch (planes) {
    case GPU_DEPTH_BIT: {
      if (this->has_depth_attachment()) {
        VKAttachment depth = this->get_depth_attachment();
        gpu::VKTexture *tex = depth.texture;
        if (tex) {
          size_t sample_len = area[2] * area[3];
          size_t sample_size = to_bytesize(tex->format_, format);
          int debug_data_size = sample_len * sample_size;
          tex->read_internal(0,
                             area[0],
                             area[1],
                             0,
                             area[2],
                             area[3],
                             1,
                             format,
                             channel_len,
                             debug_data_size,
                             r_data);
        }
      }
      else {
        GPU_VK_DEBUG_PRINTF(
            "Attempting to read depth from a framebuffer which does not have a depth "
            "attachment!\n");
      }
    }
      return;

    case GPU_COLOR_BIT: {
      if (this->has_attachment_at_slot(slot)) {
        VKAttachment color = this->get_color_attachment(slot);
        gpu::VKTexture *tex = color.texture;
        if (tex) {
          size_t sample_len = area[2] * area[3];
          size_t sample_size = to_bytesize(tex->format_, format);
          int debug_data_size = sample_len * sample_size * channel_len;
          tex->read_internal(0,
                             area[0],
                             area[1],
                             0,
                             area[2],
                             area[3],
                             1,
                             format,
                             channel_len,
                             debug_data_size,
                             r_data);
        }
      }
    }
      return;

    case GPU_STENCIL_BIT:
      GPU_VK_DEBUG_PRINTF("GPUFramebuffer: Error: Trying to read stencil bit. Unsupported.\n");
      return;
  }
}



void VKFrameBuffer::blit(uint read_slot,
                         uint src_x_offset,
                         uint src_y_offset,
                         VKFrameBuffer *metal_fb_write,
                         uint write_slot,
                         uint dst_x_offset,
                         uint dst_y_offset,
                         uint width,
                         uint height,
                         eGPUFrameBufferBits blit_buffers){};


void VKFrameBuffer::blit_to(eGPUFrameBufferBits planes,
                            int src_slot,
                            FrameBuffer *dst,
                            int dst_slot,
                            int dst_offset_x,
                            int dst_offset_y){};





bool VKFrameBuffer::has_attachment_at_slot(uint slot)
{
  BLI_assert(this);

  if (slot >= 0 && slot < this->get_attachment_limit()) {
    return vk_color_attachments_[slot].used;
  }
  return false;
}

VKAttachment VKFrameBuffer::get_color_attachment(uint slot)
{
  BLI_assert(this);
  if (slot >= 0 && slot < GPU_FB_MAX_COLOR_ATTACHMENT) {
    return vk_color_attachments_[slot];
  }
  VKAttachment null_attachment;
  null_attachment.used = false;
  return null_attachment;
};
VKAttachment VKFrameBuffer::get_depth_attachment()
{
  BLI_assert(this);
  return vk_depth_attachment_;
};

VKAttachment VKFrameBuffer::get_stencil_attachment()
{
  BLI_assert(this);
  return vk_stencil_attachment_;
};
VkAttachmentLoadOp  vk_load_action_from_gpu(eGPULoadOp action)
{
  return (action == GPU_LOADACTION_LOAD) ?
             VK_ATTACHMENT_LOAD_OP_LOAD :
             ((action == GPU_LOADACTION_CLEAR) ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                                                 VK_ATTACHMENT_LOAD_OP_DONT_CARE);
}

VkAttachmentStoreOp  vk_store_action_from_gpu(eGPUStoreOp action)
{
  return (action == GPU_STOREACTION_STORE) ? VK_ATTACHMENT_STORE_OP_STORE :
                                             VK_ATTACHMENT_STORE_OP_DONT_CARE;
}



   
}  // namespace blender::gpu

