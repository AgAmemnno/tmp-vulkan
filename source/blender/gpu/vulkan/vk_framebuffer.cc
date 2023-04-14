/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "vk_framebuffer.hh"
#include "vk_memory.hh"
#include "vk_texture.hh"
#include "vk_memory.hh"
#include "vk_texture.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

VKFrameBuffer::VKFrameBuffer(const char *name) : FrameBuffer(name)
{
  immutable_ = false;
  size_set(1, 1);
}

VKFrameBuffer::VKFrameBuffer(const char *name,
                             VkFramebuffer vk_framebuffer,
                             VkRenderPass vk_render_pass,
                             VkExtent2D vk_extent)
    : FrameBuffer(name)
{
  immutable_ = true;
  vk_render_pass_init_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  /* Never update an internal frame-buffer. */
  dirty_attachments_ = false;
  vk_framebuffer_ = vk_framebuffer;
  vk_render_pass_ = vk_render_pass;

  immutable_attachment_ = {VK_IMAGE_ASPECT_COLOR_BIT, 0, {}};
  image_id_ = -1;

  size_set(vk_extent.width, vk_extent.height);
  viewport_reset();
  scissor_reset();
}

VKFrameBuffer::~VKFrameBuffer()
{
  if (!immutable_) {
    render_pass_free();
  }
}

void VKFrameBuffer::apply_state()
{

  VKContext *vk_ctx = static_cast<VKContext *>(unwrap(GPU_context_active_get()));

  BLI_assert(vk_ctx);

  if (vk_ctx->active_fb == this) {
    if (dirty_state_ == false) {
      return;
    }

    /* Ensure viewport has been set. NOTE: This should no longer happen, but
     * kept for safety to track bugs. */
    if (viewport_[2] == 0 || viewport_[3] == 0) {
      viewport_reset();
    }

    VKCommandBuffer &cmd = vk_ctx->command_buffer_get();
    VkViewport vp = vk_viewport_get();
    cmd.viewport(vp);
    auto scissor = vk_render_area_get();
    cmd.scissor(scissor);

    GPU_scissor_test(scissor_test_);

    dirty_state_ = false;
  }
  else {
    printf(
        "Attempting to set FrameBuffer State (VIEWPORT, "
        "SCISSOR), But FrameBuffer is not bound to "
        "current Context.\n");
  }
}
/** \} */

void VKFrameBuffer::bind(bool /*enabled_srgb*/)
{
  VKContext &context = *VKContext::get();
  /* Updating attachments can issue pipeline barriers, this should be done outside the render pass.
   * When done inside a render pass there should be a self-dependency between sub-passes on the
   * active render pass. As the active render pass isn't aware of the new render pass (and should
   * not) it is better to deactivate it before updating the attachments. For more information check
   * `VkSubpassDependency`. */
  if (context.has_active_framebuffer()) {
    context.deactivate_framebuffer();
  }

  update_attachments();

  context.activate_framebuffer(*this);
  context.state_manager_get().active_fb = this;
}

VkViewport VKFrameBuffer::vk_viewport_get() const
{
  VkViewport viewport;
  int viewport_rect[4];
  viewport_get(viewport_rect);

  viewport.x = viewport_rect[0];
  viewport.y = viewport_rect[1];
  viewport.width = viewport_rect[2];
  viewport.height = viewport_rect[3];
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  return viewport;
}

VkRect2D VKFrameBuffer::vk_render_area_get() const
{
  VkRect2D render_area = {};

  if (scissor_test_get()) {
    int scissor_rect[4];
    scissor_get(scissor_rect);
    render_area.offset.x = scissor_rect[0];
    render_area.offset.y = scissor_rect[1];
    render_area.extent.width = scissor_rect[2];
    render_area.extent.height = scissor_rect[3];
  }
  else {
    render_area.offset.x = 0;
    render_area.offset.y = 0;
    render_area.extent.width = width_;
    render_area.extent.height = height_;
  }

  return render_area;
}

bool VKFrameBuffer::check(char /*err_out*/[256])
{
  return true;
}

void VKFrameBuffer::build_clear_attachments_depth_stencil(
    const eGPUFrameBufferBits buffers,
    float clear_depth,
    uint32_t clear_stencil,
    Vector<VkClearAttachment> &r_attachments) const
{
  VkClearAttachment clear_attachment = {};
  clear_attachment.aspectMask = (buffers & GPU_DEPTH_BIT ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
                                (buffers & GPU_STENCIL_BIT ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
  clear_attachment.clearValue.depthStencil.depth = clear_depth;
  clear_attachment.clearValue.depthStencil.stencil = clear_stencil;
  r_attachments.append(clear_attachment);
}

void VKFrameBuffer::build_clear_attachments_color(const float (*clear_colors)[4],
                                                  const bool multi_clear_colors,
                                                  Vector<VkClearAttachment> &r_attachments) const
{
  int color_index = 0;
  for (int color_slot = 0; color_slot < GPU_FB_MAX_COLOR_ATTACHMENT; color_slot++) {
    const GPUAttachment &attachment = attachments_[GPU_FB_COLOR_ATTACHMENT0 + color_slot];
    if (attachment.tex == nullptr) {
      continue;
    }
    VkClearAttachment clear_attachment = {};
    clear_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_attachment.colorAttachment = color_slot;
    eGPUDataFormat data_format = to_data_format(GPU_texture_format(attachment.tex));
    clear_attachment.clearValue.color = to_vk_clear_color_value(data_format,
                                                                &clear_colors[color_index]);
    r_attachments.append(clear_attachment);

    color_index += multi_clear_colors ? 1 : 0;
  }
}

VkFormat VKFrameBuffer::is_color(int slot) const
{
  if(immutable_){
    return  (slot == 0)? vk_im_prop.format :VK_FORMAT_UNDEFINED;
  }
  if(attachments_[GPU_FB_COLOR_ATTACHMENT0 + slot].tex != nullptr){
    VKTexture* texture  = reinterpret_cast<VKTexture*>(attachments_[GPU_FB_COLOR_ATTACHMENT0 + slot].tex);
    return  to_vk_format(texture->format_get());
  };
  return VK_FORMAT_UNDEFINED;
};

/* -------------------------------------------------------------------- */
/** \name Clear
 * \{ */

void VKFrameBuffer::clear(const Vector<VkClearAttachment> &attachments)
{
  VKContext &context = *VKContext::get();

  context.activate_framebuffer(*this);

  VkClearRect clear_rect = {};
  clear_rect.rect = vk_render_area_get();
  clear_rect.baseArrayLayer = 0;
  clear_rect.layerCount = 1;

  VKCommandBuffer &command_buffer = context.command_buffer_get();
  if (immutable_) {
    command_buffer.image_transition(
        &context.sc_image_get(), VkTransitionState::VK_BEFORE_RENDER_PASS, false);
  }
  else {
    VKTexture *tex = reinterpret_cast<VKTexture *>(color_tex(0));
    command_buffer.image_transition(tex, VkTransitionState::VK_BEFORE_RENDER_PASS, false);
  }

  command_buffer.clear(attachments, Span<VkClearRect>(&clear_rect, 1));

  if (immutable_) {
    context.flush(false, true, false);
  }
}

void VKFrameBuffer::clear(eGPUFrameBufferBits /*buffers*/,
                          const float /*clear_col*/[4],
                          float /*clear_depth*/,
                          uint /*clear_stencil*/)
{
}

void VKFrameBuffer::clear_multi(const float (*clear_color)[4])
{
  Vector<VkClearAttachment> attachments;
  build_clear_attachments_color(clear_color, true, attachments);
  clear(attachments);
}

void VKFrameBuffer::clear_attachment(GPUAttachmentType /*type*/,
                                     eGPUDataFormat /*data_format*/,
                                     const void * /*clear_value*/)
{
  /* Clearing of a single attachment was added to implement `clear_multi` in OpenGL. As
   * `clear_multi` is supported in Vulkan it isn't needed to implement this method.
   */
  BLI_assert_unreachable();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Load/Store operations
 * \{ */

void VKFrameBuffer::attachment_set_loadstore_op(GPUAttachmentType /*type*/,
                                                eGPULoadOp /*load_action*/,
                                                eGPUStoreOp /*store_action*/)
{
}

void VKFrameBuffer::read(eGPUFrameBufferBits /*planes*/,
                         eGPUDataFormat /*format*/,
                         const int /*area*/[4],
                         int /*channel_len*/,
                         int /*slot*/,
                         void * /*r_data*/)
{
  VKTexture *texture = nullptr;
  switch (plane) {
    case GPU_COLOR_BIT:
      texture = unwrap(unwrap(attachments_[GPU_FB_COLOR_ATTACHMENT0 + slot].tex));
      break;

    default:
      BLI_assert_unreachable();
      return;
  }

  BLI_assert_msg(texture,
                 "Trying to read back color texture from framebuffer, but no color texture is "
                 "available in requested slot.");
  void *data = texture->read(0, format);

  /*
   * TODO:
   * - Add support for area.
   * - Add support for channel_len.
   * Best option would be to add this to a specific interface in VKTexture so we don't
   * over-allocate and reduce number of times copies are made.
   */
  BLI_assert(format == GPU_DATA_FLOAT);
  BLI_assert(channel_len == 4);
  int mip_size[3] = {1, 1, 1};
  texture->mip_size_get(0, mip_size);
  const size_t mem_size = mip_size[0] * mip_size[1] * mip_size[2] * sizeof(float) * channel_len;
  memcpy(r_data, data, mem_size);
  MEM_freeN(data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blit operations
 * \{ */

void VKFrameBuffer::blit_to(eGPUFrameBufferBits /*planes*/,
                            int /*src_slot*/,
                            FrameBuffer * /*dst*/,
                            int /*dst_slot*/,
                            int /*dst_offset_x*/,
                            int /*dst_offset_y*/)
{
}

}  // namespace blender::gpu
