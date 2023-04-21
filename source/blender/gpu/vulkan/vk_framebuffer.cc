/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_framebuffer.hh"
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
  dirty_attachments_ = false;
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
  if (immutable_) {
    return (slot == 0) ? vk_im_prop.format : VK_FORMAT_UNDEFINED;
  }
  if (attachments_[GPU_FB_COLOR_ATTACHMENT0 + slot].tex != nullptr) {
    VKTexture *texture = reinterpret_cast<VKTexture *>(
        attachments_[GPU_FB_COLOR_ATTACHMENT0 + slot].tex);
    return to_vk_format(texture->format_get());
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

void VKFrameBuffer::clear(const eGPUFrameBufferBits buffers,
                          const float clear_color[4],
                          float clear_depth,
                          uint clear_stencil)
{
  Vector<VkClearAttachment> attachments;
  if (buffers & (GPU_DEPTH_BIT | GPU_STENCIL_BIT)) {
    build_clear_attachments_depth_stencil(buffers, clear_depth, clear_stencil, attachments);
  }
  if (buffers & GPU_COLOR_BIT) {

    if (immutable_) {
      copy_v4_v4(immutable_attachment_.clearValue.color.float32, clear_color);
      clear({immutable_attachment_});
      return;
    }
    else {
      float clear_color_single[4];
      copy_v4_v4(clear_color_single, clear_color);
      build_clear_attachments_color(&clear_color_single, false, attachments);
    }
  }
  clear(attachments);
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read back
 * \{ */

void VKFrameBuffer::read(eGPUFrameBufferBits /*planes*/,
                         eGPUDataFormat /*format*/,
                         const int /*area*/[4],
                         int /*channel_len*/,
                         int /*slot*/,
                         void * /*r_data*/)
{
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update attachments
 * \{ */

void VKFrameBuffer::update_attachments()
{
  if (immutable_) {
    return;
  }
  if (!dirty_attachments_) {
    return;
  }

  render_pass_free();
  render_pass_create();

  dirty_attachments_ = false;
}

void VKFrameBuffer::render_pass_create()
{
  BLI_assert(!immutable_);
  BLI_assert(vk_render_pass_ == VK_NULL_HANDLE);
  BLI_assert(vk_framebuffer_ == VK_NULL_HANDLE);

  VK_ALLOCATION_CALLBACKS

  /* Track first attachment for size.*/
  GPUAttachmentType first_attachment = GPU_FB_MAX_ATTACHMENT;

  std::array<VkAttachmentDescription, GPU_FB_MAX_ATTACHMENT> attachment_descriptions;
  std::array<VkImageView, GPU_FB_MAX_ATTACHMENT> image_views;
  std::array<VkAttachmentReference, GPU_FB_MAX_ATTACHMENT> attachment_references;
  /*Vector<VkAttachmentReference> color_attachments;
  VkAttachmentReference depth_attachment = {};
  */
  bool has_depth_attachment = false;
  bool found_attachment = false;
  int depth_location = -1;

  for (int type = GPU_FB_MAX_ATTACHMENT - 1; type >= 0; type--) {
    GPUAttachment &attachment = attachments_[type];
    if (attachment.tex == nullptr && !found_attachment) {
      /* Move the depth texture to the next binding point after all color textures. The binding
       * location of the color textures should be kept in sync between ShaderCreateInfos and the
       * framebuffer attachments. The depth buffer should be the last slot. */
      depth_location = max_ii(type - GPU_FB_COLOR_ATTACHMENT0, 0);
      continue;
    }
    found_attachment |= attachment.tex != nullptr;

    /* Keep the first attachment to the first color attachment, or to the depth buffer when there
     * is no color attachment. */
    if (attachment.tex != nullptr &&
        (first_attachment == GPU_FB_MAX_ATTACHMENT || type >= GPU_FB_COLOR_ATTACHMENT0)) {
      first_attachment = static_cast<GPUAttachmentType>(type);
    }

    int attachment_location = type >= GPU_FB_COLOR_ATTACHMENT0 ? type - GPU_FB_COLOR_ATTACHMENT0 :
                                                                 depth_location;

    if (attachment.tex) {
      /* Ensure texture is allocated to ensure the image view.*/
      VKTexture &texture = *static_cast<VKTexture *>(unwrap(attachment.tex));
      texture.ensure_allocated();
      image_views[attachment_location] = texture.vk_image_view_handle();

      VkAttachmentDescription &attachment_description =
          attachment_descriptions[attachment_location];
      attachment_description.flags = 0;
      attachment_description.format = to_vk_format(texture.format_get());
      attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
      attachment_description.loadOp =
          VK_ATTACHMENT_LOAD_OP_LOAD;  // VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachment_description.initialLayout =
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;  // VK_IMAGE_LAYOUT_GENERAL;
      attachment_description.finalLayout =
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;  // VK_IMAGE_LAYOUT_GENERAL;
      vk_render_pass_init_ = attachment_description.initialLayout;
      /* Create the attachment reference. */
      const bool is_depth_attachment = ELEM(
          type, GPU_FB_DEPTH_ATTACHMENT, GPU_FB_DEPTH_STENCIL_ATTACHMENT);

      BLI_assert_msg(!is_depth_attachment || !has_depth_attachment,
                     "There can only be one depth/stencil attachment.");
      has_depth_attachment |= is_depth_attachment;
      VkAttachmentReference &attachment_reference = attachment_references[attachment_location];
      attachment_reference.attachment = attachment_location;
      attachment_reference.layout = is_depth_attachment ?
                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
  }

  /* Update the size, viewport & scissor based on the first attachment. */
  if (first_attachment != GPU_FB_MAX_ATTACHMENT) {
    GPUAttachment &attachment = attachments_[first_attachment];
    BLI_assert(attachment.tex);

    int size[3];
    GPU_texture_get_mipmap_size(attachment.tex, attachment.mip, size);
    size_set(size[0], size[1]);
  }
  else {
    /* A framebuffer should at least be 1 by 1.*/
    this->size_set(1, 1);
  }
  viewport_reset();
  scissor_reset();

  /* Create render pass. */

  const int attachment_len = has_depth_attachment ? depth_location + 1 : depth_location;
  const int color_attachment_len = depth_location;
  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = color_attachment_len;
  subpass.pColorAttachments = attachment_references.data();
  if (has_depth_attachment) {
    subpass.pDepthStencilAttachment = &attachment_references[depth_location];
  }

  // Use subpass dependencies for layout transitions
  std::array<VkSubpassDependency, 2> dependencies;
  int depCnt = 1;
  dependencies[0].srcSubpass = 0;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  if (has_depth_attachment) {

    depCnt++;
    dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].dstSubpass = 0;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                   VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                   VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies[1].dependencyFlags = 0;
  }

  VkRenderPassCreateInfo render_pass_info = {};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = attachment_len;
  render_pass_info.pAttachments = attachment_descriptions.data();
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = depCnt;
  render_pass_info.pDependencies = dependencies.data();

  VKContext &context = *VKContext::get();
  vkCreateRenderPass(
      context.device_get(), &render_pass_info, vk_allocation_callbacks, &vk_render_pass_);

  /* We might want to split framebuffer and render pass....*/
  VkFramebufferCreateInfo framebuffer_create_info = {};
  framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer_create_info.renderPass = vk_render_pass_;
  framebuffer_create_info.attachmentCount = attachment_len;
  framebuffer_create_info.pAttachments = image_views.data();
  framebuffer_create_info.width = width_;
  framebuffer_create_info.height = height_;
  framebuffer_create_info.layers = 1;

  vkCreateFramebuffer(
      context.device_get(), &framebuffer_create_info, vk_allocation_callbacks, &vk_framebuffer_);
  debug::object_label(&context, vk_framebuffer_, "OffscreenFB");
  debug::object_label(&context, image_views[0], "OffscreenIV");
}

void VKFrameBuffer::render_pass_free()
{
  BLI_assert(!immutable_);
  if (vk_render_pass_ == VK_NULL_HANDLE) {
    return;
  }
  VK_ALLOCATION_CALLBACKS

  VKContext &context = *VKContext::get();
  vkDestroyRenderPass(context.device_get(), vk_render_pass_, vk_allocation_callbacks);
  vkDestroyFramebuffer(context.device_get(), vk_framebuffer_, vk_allocation_callbacks);
  vk_render_pass_ = VK_NULL_HANDLE;
  vk_framebuffer_ = VK_NULL_HANDLE;
}

/** \} */

}  // namespace blender::gpu
