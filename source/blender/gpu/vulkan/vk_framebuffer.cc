/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_framebuffer.hh"
#include "vk_state.hh"
#include "vk_texture.hh"

#include "intern/GHOST_ContextVK.h"
#include <fstream>

namespace blender::gpu {
#define VK_PNEXT_INIT_VAL 0x7777777
#define SET_VK_PNEXT_INIT(str) str.pNext = (const void *)VK_PNEXT_INIT_VAL;
#define IS_VK_PNEXT_INIT(str) (VK_PNEXT_INIT_VAL == (uint64_t)str.pNext)
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

VKFrameBuffer::VKFrameBuffer(const char *name, VKContext *ctx)
    : FrameBuffer(name), vk_attachments_(this)
{
  /* Just-In-Time init. See #VKFrameBuffer::init(). */
  init(ctx);
  context_ = ctx;
  viewport_[0] = scissor_[0] = 0;
  viewport_[1] = scissor_[1] = 0;
  viewport_[2] = scissor_[2] = 0;
  viewport_[3] = scissor_[3] = 0;

  printf("Frmae buffer Init  Mutable ============================================%llu   %s     \n",
         (uint64_t)this,
         name_);
}

VKFrameBuffer::VKFrameBuffer(const char *name, VKContext *ctx, int w, int h)
    : FrameBuffer(name), vk_attachments_(this)
{

  init(ctx);
  context_ = ctx;
  immutable_ = true;
  /* Never update an internal frame-buffer. */
  dirty_attachments_ = false;
  width_ = w;
  height_ = h;
  srgb_ = false;

  viewport_[0] = scissor_[0] = 0;
  viewport_[1] = scissor_[1] = 0;
  viewport_[2] = scissor_[2] = w;
  viewport_[3] = scissor_[3] = h;

  printf(
      "Frmae buffer Init  Immutable ============================================%llu   %s     \n",
      (uint64_t)this,
      name_);
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

  this->vk_attachments_.clear();

  if (context_ == nullptr) {
    return;
  }
  VkDevice device = context_->device_get();

  for (auto &sema : submit_signal_) {
    vkDestroySemaphore(device, sema, nullptr);
  };
}

void VKFrameBuffer::init(VKContext *ctx)
{
  immutable_ = false;
  fbo_id_ = 0;
  is_init_ = true;
  context_ = ctx;
  state_manager_ = (VKStateManager *)(context_->state_manager);
  vk_attachments_.set_ctx(ctx);
  vk_attachments_.clear();
  is_command_begin_ = false;
  is_render_begin_ = false;
  is_swapchain_ = false;
  is_blit_begin_ = false;
  offscreen_render_times_ = 0;
  printf("Frmae buffer Init  Mutable ============================================   %s     \n",
         name_);
}

/** \} */
void VKFrameBuffer::update_attachments()
{
  /* Default frame-buffers cannot have attachments. */
  BLI_assert(immutable_ == false);
  BLI_assert(vk_attachments_.get_nums() == 0);

  /* First color texture OR the depth texture if no color is attached.
   * Used to determine frame-buffer color-space and dimensions. */
  is_nocolor_ = true;

  /*Color*/
  for (int type = (int)GPU_FB_COLOR_ATTACHMENT0; type <= (int)GPU_FB_COLOR_ATTACHMENT7; type++) {
    GPUAttachment &attach = attachments_[type];
    if (attach.tex) {
      vk_attachments_.append(attach, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      is_nocolor_ = false;
    };
  };

  /*Depth*/

  {
    GPUAttachment &attach = attachments_[GPU_FB_DEPTH_ATTACHMENT];
    if (attach.tex) {
      vk_attachments_.append(attach, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    }
  };

  /*Stencil*/

  {
    GPUAttachment &attach = attachments_[GPU_FB_DEPTH_STENCIL_ATTACHMENT];
    if (attach.tex) {
      vk_attachments_.append(attach, VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL);
    }
  };

  /*Skip unused_fb_slot_workaround*/

  dirty_attachments_ = false;
  vk_attachments_.create_framebuffer();

  VkSemaphoreCreateInfo semaphore_info = {};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_info.flags = 0;
  VKContext *context = VKContext::get();
  VkDevice device = context->device_get();
  for (int i = 0; i < 2; i++) {
    VkSemaphore sema = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSemaphore(device, &semaphore_info, NULL, &sema));
    submit_signal_.append(sema);
  }
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  ;

  BLI_assert(vk_attachments_.vtex_.size() == 1);

  VKTexture *tex = vk_attachments_.vtex_[0];

  context->begin_submit_simple(cmd);
  blender::vulkan::GHOST_ImageTransition(
      cmd,
      tex->get_image(),
      tex->info.format,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM,
      VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED);
  context->end_submit_simple();
  tex->set_image_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0);
};

void VKFrameBuffer::bind(bool enabled_srgb)
{
  BLI_assert_msg(context_, "Trying to use the same frame-buffer in multiple context");
  if (!immutable_ && !is_init_) {
    this->init(context_);
  }
  printf("Frmae buffer Bind ============================================%llu   %s     \n",
         (uint64_t)this,
         name_);

  if (context_->active_fb != this) {
    /*Do we need to regenerate the pipeline?*/
  }

  if (dirty_attachments_) {
    this->update_attachments();
    this->viewport_reset();
    this->scissor_reset();
  }

  if (context_->active_fb != this || enabled_srgb_ != enabled_srgb) {
    enabled_srgb_ = enabled_srgb;
    GPU_shader_set_framebuffer_srgb_target(enabled_srgb && srgb_);
  }

  if (context_->active_fb != this) {
    context_->active_fb = this;
    state_manager_->active_fb = this;
    dirty_state_ = true;
  }
}

bool VKFrameBuffer::check(char err_out[256])
{
  this->bind(true);

  return true;
}

void VKFrameBuffer::force_clear()
{
}

void VKFrameBuffer::clear(eGPUFrameBufferBits buffers,
                          const float clear_col[4],
                          float clear_depth,
                          uint clear_stencil)
{

  VkClearValue clearValues[2];
  for (int i = 0; i < 4; i++)
    clearValues[0].color.float32[i] = clear_col[i];
  clearValues[1].depthStencil.depth = clear_depth;
  clearValues[1].depthStencil.stencil = clear_stencil;
  auto loadOp = vk_attachments_.get_LoadOp();
  if (loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {  // VK_ATTACHMENT_LOAD_OP_LOAD
    if (is_render_begin_) {
      render_end();
      context_->end_frame();
      context_->begin_frame();
    }
    render_begin(VK_NULL_HANDLE, VK_COMMAND_BUFFER_LEVEL_PRIMARY, (VkClearValue *)clearValues);
  }
  else if (loadOp == VK_ATTACHMENT_LOAD_OP_LOAD) {

    BLI_assert(!is_render_begin_);

    VKContext *context = VKContext::get();
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VKTexture *tex = vk_attachments_.vtex_[0];
    VkImage srcImage = tex->get_image();
    VkImageLayout src_layout = tex->get_image_layout(0);
    // VkFormat src_format = tex->info.format;

    context->begin_submit_simple(cmd);

    if (src_layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
      VkImageMemoryBarrier imageMemoryBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
      imageMemoryBarrier.pNext = NULL;
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      imageMemoryBarrier.oldLayout = src_layout;
      imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      imageMemoryBarrier.image = srcImage;
      imageMemoryBarrier.subresourceRange = VkImageSubresourceRange{
          VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

      vkCmdPipelineBarrier(cmd,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0,
                           0,
                           nullptr,
                           0,
                           nullptr,
                           1,
                           &imageMemoryBarrier);
      tex->set_image_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0);
    }

    VkImageSubresourceRange ImageSubresourceRange;
    ImageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ImageSubresourceRange.baseMipLevel = 0;
    ImageSubresourceRange.levelCount = 1;
    ImageSubresourceRange.baseArrayLayer = 0;
    ImageSubresourceRange.layerCount = 1;
    vkCmdClearColorImage(cmd,
                         srcImage,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         (const VkClearColorValue *)&clearValues[0].color,
                         1,
                         &ImageSubresourceRange);
    {
      VkImageMemoryBarrier imageMemoryBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
      imageMemoryBarrier.pNext = NULL;
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      imageMemoryBarrier.image = srcImage;
      imageMemoryBarrier.subresourceRange = VkImageSubresourceRange{
          VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

      vkCmdPipelineBarrier(cmd,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           0,
                           0,
                           nullptr,
                           0,
                           nullptr,
                           1,
                           &imageMemoryBarrier);
      tex->set_image_layout(imageMemoryBarrier.newLayout, 0);
    }

    context->end_submit_simple();
  }

  // context_->clear_color(vk_cmd, (VkClearColorValue*)&clearValues[0].color.float32[0]);
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

/*I will use it almost as is.
 * https://github.com/SaschaWillems/Vulkan/blob/master/examples/screenshot/screenshot.cpp */
void saveScreenshot(const char *filename, VKTexture *tex)
{
  int mip = 0;

  VKContext *context = VKContext::get();
  BLI_assert(context);
  auto pdevice = context->get_physical_device();
  auto device = context->device_get();
  auto colorFormat = context->getImageFormat();

  bool supportsBlit = true;

  // Check blit support for source and destination
  VkFormatProperties formatProps;

  // Check if the device supports blitting from optimal images (the swapchain images are in optimal
  // format)
  vkGetPhysicalDeviceFormatProperties(pdevice, colorFormat, &formatProps);
  if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
    std::cerr << "Device does not support blitting from optimal tiled images, using copy instead "
                 "of blit!"
              << std::endl;
    supportsBlit = false;
  }

  // Check if the device supports blitting to linear images
  vkGetPhysicalDeviceFormatProperties(pdevice, VK_FORMAT_R8G8B8A8_UNORM, &formatProps);
  if (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
    std::cerr
        << "Device does not support blitting to linear tiled images, using copy instead of blit!"
        << std::endl;
    supportsBlit = false;
  }

  // Source for the copy is the last rendered swapchain image
  VkImage srcImage = tex->get_image();

  // Create the linear tiled destination image to copy to and to read the memory from
  VkImageCreateInfo imageCreateCI = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
  // Note that vkCmdBlitImage (if supported) will also do format conversions if the swapchain color
  // format would differ
  imageCreateCI.format = VK_FORMAT_R8G8B8A8_UNORM;
  imageCreateCI.extent.width = tex->info.extent.width;
  imageCreateCI.extent.height = tex->info.extent.height;
  imageCreateCI.extent.depth = 1;
  imageCreateCI.arrayLayers = 1;
  imageCreateCI.mipLevels = 1;
  imageCreateCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageCreateCI.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateCI.tiling = VK_IMAGE_TILING_LINEAR;
  imageCreateCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  // Create the image
  VkImage dstImage;
  VK_CHECK(vkCreateImage(device, &imageCreateCI, nullptr, &dstImage));
  // Create memory to back up the image
  VkMemoryRequirements memRequirements;
  VkMemoryAllocateInfo memAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  VkDeviceMemory dstImageMemory;
  vkGetImageMemoryRequirements(device, dstImage, &memRequirements);
  memAllocInfo.allocationSize = memRequirements.size;
  // Memory must be host visible to copy from
  memAllocInfo.memoryTypeIndex = getMemoryType(memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  VK_CHECK(vkAllocateMemory(device, &memAllocInfo, nullptr, &dstImageMemory));
  VK_CHECK(vkBindImageMemory(device, dstImage, dstImageMemory, 0));

  VkCommandBuffer copyCmd = VK_NULL_HANDLE;
  context->begin_submit_simple(copyCmd);

  // Transition destination image to transfer destination layout
  insert_image_memory_barrier(copyCmd,
                              dstImage,
                              0,
                              VK_ACCESS_TRANSFER_WRITE_BIT,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  auto src_layout = tex->get_image_layout(mip);
  // Transition swapchain image from present to transfer source layout
  insert_image_memory_barrier(
      copyCmd,
      srcImage,
      (src_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) ?
          (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) :
          VK_ACCESS_MEMORY_READ_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      src_layout,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      (src_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) ?
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT :
          VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  // If source and destination support blit we'll blit as this also does automatic format
  // conversion (e.g. from BGR to RGB)
  if (supportsBlit) {
    // Define the region to blit (we will blit the whole swapchain image)
    VkOffset3D blitSize;
    blitSize.x = tex->info.extent.width;
    blitSize.y = tex->info.extent.height;
    blitSize.z = 1;
    VkImageBlit imageBlitRegion{};
    imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBlitRegion.srcSubresource.layerCount = 1;
    imageBlitRegion.srcOffsets[1] = blitSize;
    imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBlitRegion.dstSubresource.layerCount = 1;
    imageBlitRegion.dstOffsets[1] = blitSize;

    // Issue the blit command
    vkCmdBlitImage(copyCmd,
                   srcImage,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   dstImage,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &imageBlitRegion,
                   VK_FILTER_NEAREST);
  }
  else {
    // Otherwise use image copy (requires us to manually flip components)
    VkImageCopy imageCopyRegion{};
    imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.srcSubresource.layerCount = 1;
    imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.dstSubresource.layerCount = 1;
    imageCopyRegion.extent.width = tex->info.extent.width;
    imageCopyRegion.extent.height = tex->info.extent.height;
    imageCopyRegion.extent.depth = 1;

    // Issue the copy command
    vkCmdCopyImage(copyCmd,
                   srcImage,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   dstImage,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &imageCopyRegion);
  }

  // Transition destination image to general layout, which is the required layout for mapping the
  // image memory later on
  insert_image_memory_barrier(copyCmd,
                              dstImage,
                              VK_ACCESS_TRANSFER_WRITE_BIT,
                              VK_ACCESS_MEMORY_READ_BIT,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_GENERAL,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  // Transition back the swap chain image after the blit is done
  insert_image_memory_barrier(
      copyCmd,
      srcImage,
      VK_ACCESS_TRANSFER_READ_BIT,
      (src_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) ?
          (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) :
          VK_ACCESS_MEMORY_READ_BIT,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      src_layout,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      (src_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) ?
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT :
          VK_PIPELINE_STAGE_TRANSFER_BIT,
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  context->end_submit_simple();

  // Get layout of the image (including row pitch)
  VkImageSubresource subResource{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
  VkSubresourceLayout subResourceLayout;
  vkGetImageSubresourceLayout(device, dstImage, &subResource, &subResourceLayout);

  // Map image memory so we can start copying from it
  const char *data;
  vkMapMemory(device, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void **)&data);
  data += subResourceLayout.offset;

  std::ofstream file(filename, std::ios::out | std::ios::binary);

  // ppm header
  file << "P6\n"
       << tex->info.extent.width << "\n"
       << tex->info.extent.height << "\n"
       << 255 << "\n";

  // If source is BGR (destination is always RGB) and we can't use blit (which does automatic
  // conversion), we'll have to manually swizzle color components
  bool colorSwizzle = false;
  // Check if source is BGR
  // Note: Not complete, only contains most common and basic BGR surface formats for demonstration
  // purposes
  if (!supportsBlit) {
    std::vector<VkFormat> formatsBGR = {
        VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM};
    colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), colorFormat) !=
                    formatsBGR.end());
  }

  // ppm binary pixel data
  for (uint32_t y = 0; y < tex->info.extent.height; y++) {
    unsigned int *row = (unsigned int *)data;
    for (uint32_t x = 0; x < tex->info.extent.width; x++) {
      if (colorSwizzle) {
        file.write((char *)row + 2, 1);
        file.write((char *)row + 1, 1);
        file.write((char *)row, 1);
      }
      else {
        file.write((char *)row, 3);
      }
      row++;
    }
    data += subResourceLayout.rowPitch;
  }
  file.close();

  std::cout << "Screenshot saved to disk" << std::endl;

  // Clean up resources
  vkUnmapMemory(device, dstImageMemory);
  vkFreeMemory(device, dstImageMemory, nullptr);
  vkDestroyImage(device, dstImage, nullptr);
}

void VKFrameBuffer::save_current_frame(const char *filename)
{
  if (is_swapchain_) {
    BLI_assert(false);
  }
  else {

    BLI_assert(vk_attachments_.vtex_.size() == 1);
    saveScreenshot(filename, vk_attachments_.vtex_[0]);
  }
};
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

void VKFrameBuffer::blit_to(
    eGPUFrameBufferBits planes, int src_slot, FrameBuffer *dst_, int dst_slot, int x, int y)
{

  VKFrameBuffer *src = this;
  VKFrameBuffer *dst = static_cast<VKFrameBuffer *>(dst_);
  if (dst->wait_sema.size() != 1) {
    return;
  }

  BLI_assert(dst->wait_sema[0] == src->get_signal());

  /* Frame-buffers must be up to date. This simplify this function. */
  if (src->dirty_attachments_) {
    src->bind(true);
  }
  if (dst->dirty_attachments_) {
    dst->bind(true);
  }

  VkCommandBuffer cmd = dst->get_command();
  BLI_assert(cmd != VK_NULL_HANDLE);

  if (planes & GPU_COLOR_BIT) {
    BLI_assert(src->immutable_ == false || src_slot == 0);
    BLI_assert(dst->immutable_ == false || dst_slot == 0);
  }

  context_->state_manager->apply_state();

  VKContext *context_ = VKContext::get();

  // VkImageAspectFlags mask = to_vk(planes);
  VkImage dstImage = dst->get_swapchain_image();
  VkImageLayout dst_layout = dst->get_swapchain_image_layout();
  VkFormat dst_format = VKContext::get()->getImageFormat();

  BLI_assert(context_->is_support_format(dst_format, VK_FORMAT_FEATURE_BLIT_DST_BIT, false));

  if (dst_layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    VkImageMemoryBarrier imageMemoryBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    imageMemoryBarrier.pNext = NULL;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.oldLayout = dst_layout;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.image = dstImage;
    imageMemoryBarrier.subresourceRange = VkImageSubresourceRange{
        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &imageMemoryBarrier);
    dst->set_swapchain_image_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  }

  VKTexture *src_tex = src->vk_attachments_.vtex_[0];

  VkImage srcImage = src_tex->get_image();
  VkImageLayout src_layout = src_tex->get_image_layout(0);
  VkFormat src_format = src_tex->info.format;

  BLI_assert(context_->is_support_format(src_format, VK_FORMAT_FEATURE_BLIT_SRC_BIT, false));
  if (src_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
    VkImageMemoryBarrier imageMemoryBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    imageMemoryBarrier.pNext = NULL;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    imageMemoryBarrier.oldLayout = src_layout;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imageMemoryBarrier.image = srcImage;
    imageMemoryBarrier.subresourceRange = VkImageSubresourceRange{
        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &imageMemoryBarrier);
    src_tex->set_image_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0);
  }

  {
    VkOffset3D blitSize;

    VkImageBlit imageBlitRegion{};
    imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBlitRegion.srcSubresource.layerCount = 1;
    blitSize.x = 0;
    blitSize.y = 0;
    blitSize.z = 0;
    imageBlitRegion.srcOffsets[0] = blitSize;
    blitSize.x = src->width_;
    blitSize.y = src->height_;
    blitSize.z = 1;
    imageBlitRegion.srcOffsets[1] = blitSize;

    imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBlitRegion.dstSubresource.layerCount = 1;
    blitSize.x = x;
    blitSize.y = y;
    blitSize.z = 0;
    imageBlitRegion.dstOffsets[0] = blitSize;
    blitSize.x = x + src->width_;
    blitSize.y = y + src->height_;
    blitSize.z = 1;
    imageBlitRegion.dstOffsets[1] = blitSize;

    vkCmdBlitImage(cmd,
                   srcImage,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   dstImage,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &imageBlitRegion,
                   VK_FILTER_NEAREST);
  }

  {
    VkImageMemoryBarrier imageMemoryBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    imageMemoryBarrier.pNext = NULL;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    imageMemoryBarrier.image = dstImage;
    imageMemoryBarrier.subresourceRange = VkImageSubresourceRange{
        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &imageMemoryBarrier);
    dst->set_swapchain_image_layout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  }

  {
    VkImageMemoryBarrier imageMemoryBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    imageMemoryBarrier.pNext = NULL;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageMemoryBarrier.image = srcImage;
    imageMemoryBarrier.subresourceRange = VkImageSubresourceRange{
        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &imageMemoryBarrier);
    src_tex->set_image_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0);
  }

#if 0
  if (!dst->immutable_) {
    /* Restore the draw buffers. */
    glDrawBuffers(ARRAY_SIZE(dst->gl_attachments_), dst->gl_attachments_);
  }
#endif
  /* Ensure previous buffer is restored. */
  context_->active_fb = dst;
  offscreen_render_times_ = 0;
};

VkCommandBuffer VKFrameBuffer::render_begin(VkCommandBuffer cmd,
                                            VkCommandBufferLevel level,
                                            VkClearValue *clearValues,
                                            bool blit)
{

  BLI_assert(vk_attachments_.renderpass_ != VK_NULL_HANDLE);
  if (is_command_begin_) {
    BLI_assert(vk_cmd != VK_NULL_HANDLE);
    if (is_blit_begin_) {
      return vk_cmd;
    }
    if (blit) {
      if (is_render_begin_) {
        render_end();
        context_->end_frame();
      };
    }
    else {
      bool prim = bool(level == VK_COMMAND_BUFFER_LEVEL_PRIMARY);
      if (prim) {
        BLI_assert(is_render_begin_ == true);
      }
      return vk_cmd;
    }
  };
  BLI_assert(flight_ticket_ < 0);

  printf("Begin  Renderpass =================================================-- %llu  \n",
         (uint64_t)vk_attachments_.renderpass_);

  bool prim = false;
  if (cmd == VK_NULL_HANDLE) {
    if (level == VK_COMMAND_BUFFER_LEVEL_SECONDARY) {
      vk_cmd = context_->request_command_buffer(true);
    }
    else {
      vk_cmd = context_->request_command_buffer();
      prim = true;
    };
  }
  else {
    // vk_cmd = cmd;
    vk_cmd = context_->request_command_buffer();
    prim = bool(level == VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  };

  cmd_refs = 0;
  if (blit) {
    flight_ticket_ = context_->begin_blit_submit(vk_cmd);
    is_blit_begin_ = true;
  }
  else if (is_swapchain_) {
    flight_ticket_ = context_->begin_onetime_submit(vk_cmd);
  }
  else {
    flight_ticket_ = context_->begin_offscreen_submit(vk_cmd);
  }

  VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  /*
  VkCommandBufferInheritanceInfo inheritance = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO
  }; inheritance.renderPass = vk_attachments_.renderpass_; inheritance.framebuffer =
  vk_attachments_.framebuffer_; inheritance.pNext = NULL; inheritance.subpass = 0;
  */
  // begin_info.pInheritanceInfo = &inheritance;
  begin_info.pNext = NULL;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |
                     VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

  /*#vkResetCommandBuffer(vk_cmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);*/
  VK_CHECK2(vkBeginCommandBuffer(vk_cmd, &begin_info));

  is_command_begin_ = true;
  if (prim && !blit) {
    BLI_assert(is_render_begin_ == false);

    static VkRenderPassBeginInfo renderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};

    renderPassBeginInfo.renderPass = vk_attachments_.renderpass_;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = vk_attachments_.extent_.width;
    renderPassBeginInfo.renderArea.extent.height = vk_attachments_.extent_.height;
    renderPassBeginInfo.clearValueCount = 1;
    if (clearValues == nullptr) {
      static VkClearValue clearValues_[2];
      clearValues_[0].color = {{0.25f, 0.25f, 0.25f, 1.0f}};
      clearValues_[1].depthStencil = {1.0f, 0};
      renderPassBeginInfo.pClearValues = clearValues_;
    }
    else {
      renderPassBeginInfo.pClearValues = clearValues;
    }

    auto fid = 0;
    if (is_swapchain_) {
      fid = context_->get_current_image_index();
    }
    renderPassBeginInfo.framebuffer = vk_attachments_.framebuffer_[fid];
    vkCmdBeginRenderPass(vk_cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    is_render_begin_ = true;
  }
  if (blit) {
    /*blit does not use renderpass.*/
    is_dirty_render_ = true;
  }
  else {
    is_dirty_render_ = false;
  }

  return vk_cmd;
};
void VKFrameBuffer::render_end()
{

  bool submit = false;

  if (is_render_begin_) {
    vkCmdEndRenderPass(vk_cmd);
    is_render_begin_ = false;
    submit = true;
  }

  if (is_command_begin_) {
    VK_CHECK2(vkEndCommandBuffer(vk_cmd));
    is_command_begin_ = false;
    submit = true;
  }

  if (!is_dirty_render_) {
    context_->fail_transition();
  }

  if (submit) {

    if (is_blit_begin_) {
      context_->end_blit_submit(vk_cmd, wait_sema);
      is_blit_begin_ = false;
    }
    else if (is_swapchain_) {
      BLI_assert(flight_ticket_ >= 0);
      context_->end_onetime_submit(flight_ticket_);
    }
    else {

      signal_index_ = (signal_index_ + 1) % 2;
      if (offscreen_render_times_ > 0) {
        int wait_index_ = (signal_index_ + 1) % 2;
        context_->end_offscreen_submit(
            vk_cmd, submit_signal_[wait_index_], submit_signal_[signal_index_]);
      }
      else {
        context_->end_offscreen_submit(vk_cmd, VK_NULL_HANDLE, submit_signal_[signal_index_]);
      }
      offscreen_render_times_++;
    }
  }

  if (cmd_refs == 0) {
    /*Different attachment frameworks have different ways of transitioning. For now, for a simple
     * frame of a swap chain.*/
    // context_->bottom_transition(vk_cmd);
  }
  if (offscreen_render_times_ == 0) {
    vk_cmd = VK_NULL_HANDLE;
  };

  flight_ticket_ = -1;
  cmd_refs = 0;
  is_dirty_render_ = true;
  wait_sema.clear();
};

void VKFrameBuffer::create_swapchain_frame_buffer(int i)
{

  srgb_ = false;

  vk_attachments_.append_from_swapchain(i);
  vk_attachments_.create_framebuffer();
  width_ = vk_attachments_.extent_.width;
  height_ = vk_attachments_.extent_.height;

  immutable_ = true;
  /* Never update an internal frame-buffer. */
  dirty_attachments_ = false;
  is_swapchain_ = true;
};

/*Couldn't find it used.Will leave the attachments to a naive implementation and do it later.*/
void VKFrameBuffer::attachment_set_loadstore_op(GPUAttachmentType type,
                                                eGPULoadOp load_action,
                                                eGPUStoreOp store_action)
{
  if (type >= GPU_FB_COLOR_ATTACHMENT0) {
  }
  else if (type == GPU_FB_DEPTH_STENCIL_ATTACHMENT) {
  }
  else if (type == GPU_FB_DEPTH_ATTACHMENT) {
  }
}
VkAttachmentLoadOp vk_load_action_from_gpu(eGPULoadOp action)
{
  return (action == GPU_LOADACTION_LOAD) ?
             VK_ATTACHMENT_LOAD_OP_LOAD :
             ((action == GPU_LOADACTION_CLEAR) ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                                                 VK_ATTACHMENT_LOAD_OP_DONT_CARE);
}

VkAttachmentStoreOp vk_store_action_from_gpu(eGPUStoreOp action)
{
  return (action == GPU_STOREACTION_STORE) ? VK_ATTACHMENT_STORE_OP_STORE :
                                             VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

}  // namespace blender::gpu
