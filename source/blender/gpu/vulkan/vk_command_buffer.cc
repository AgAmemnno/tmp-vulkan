/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_debug.hh"

#include "vk_buffer.hh"
#include "vk_command_buffer.hh"
#include "vk_context.hh"
#include "vk_framebuffer.hh"
#include "vk_memory.hh"
#include "vk_pipeline.hh"
#include "vk_texture.hh"

#include "BLI_assert.h"

#include "intern/GHOST_ContextVk.h"

namespace blender::gpu {

VKCommandBuffer::~VKCommandBuffer()
{
  if (vk_device_ != VK_NULL_HANDLE) {
    VK_ALLOCATION_CALLBACKS;
    vkDestroyFence(vk_device_, vk_fence_, vk_allocation_callbacks);
    vk_fence_ = VK_NULL_HANDLE;

    if (sema_toggle_[sema_own_] != VK_NULL_HANDLE) {
      vkDestroySemaphore(vk_device_, sema_toggle_[sema_own_], VK_NULL_HANDLE);
      sema_toggle_[0] = sema_toggle_[1] = VK_NULL_HANDLE;
    }
  }
}

bool VKCommandBuffer::init(const VkDevice vk_device,
                           const VkQueue vk_queue,
                           VkCommandBuffer vk_command_buffer)
{

  vk_device_ = vk_device;
  vk_queue_ = vk_queue;

  if (vk_fence_ == VK_NULL_HANDLE) {

    VK_ALLOCATION_CALLBACKS;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(vk_device, &fenceInfo, vk_allocation_callbacks, &vk_fence_);

    sema_own_ = 1;
    BLI_assert(sema_toggle_[sema_own_] == VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(vk_device, &semaphore_info, NULL, &sema_toggle_[sema_own_]);

    submission_id_.reset();
  }
  else {

    submit(false, false);
  }

  begin_cmd_ = false;
  in_toggle_ = false;
  vk_command_buffer_ = vk_command_buffer;
  begin_rp_ = false;

  return begin_recording();
}

void VKCommandBuffer::in_flight_receive()
{

  if (in_flight_) {

    VkResult result = VK_NOT_READY;
    int Try = 10;
    do {
      result = vkWaitForFences(vk_device_, 1, &vk_fence_, VK_TRUE, 100);
    } while (result == VK_TIMEOUT && Try-- > 0);

    BLI_assert(Try > 0);
    in_flight_ = false;
  }
}

bool VKCommandBuffer::begin_recording()
{
  if (in_toggle_) {
    return true;
  }

  BLI_assert(VKContext::get()->validate_frame());
  in_flight_receive();

  vkResetFences(vk_device_, 1, &vk_fence_);
  end_recording();
  vkResetCommandBuffer(vk_command_buffer_, 0);

  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(vk_command_buffer_, &begin_info);
  in_toggle_ = true;

  return true;
}

void VKCommandBuffer::end_recording()
{
  if (in_toggle_) {
    end_render_pass(nullptr);
    vkEndCommandBuffer(vk_command_buffer_);
    in_submit_ = true;
  }
  in_toggle_ = false;
}

void VKCommandBuffer::bind(const VKPipeline &pipeline, VkPipelineBindPoint bind_point)
{
  vkCmdBindPipeline(vk_command_buffer_, bind_point, pipeline.vk_handle());
}

void VKCommandBuffer::bind(const VKDescriptorSet &descriptor_set,
                           const VkPipelineLayout vk_pipeline_layout,
                           VkPipelineBindPoint bind_point)
{
  VkDescriptorSet vk_descriptor_set = descriptor_set.vk_handle();
  vkCmdBindDescriptorSets(
      vk_command_buffer_, bind_point, vk_pipeline_layout, 0, 1, &vk_descriptor_set, 0, 0);
}

bool VKCommandBuffer::begin_render_pass(const VKFrameBuffer &framebuffer, SafeImage &sfim)
{

  if (!in_toggle_) {
    if (!begin_recording()) {
      return false;
    };
  }

  end_render_pass(nullptr);

  image_transition(sfim, VkTransitionState::VK_PRESE2COLOR, true);

  VkRenderPassBeginInfo render_pass_begin_info = {};
  render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_begin_info.renderPass = framebuffer.vk_render_pass_get();
  render_pass_begin_info.framebuffer = framebuffer.vk_framebuffer_get();
  render_pass_begin_info.renderArea = framebuffer.vk_render_area_get();
  vkCmdBeginRenderPass(vk_command_buffer_, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
  begin_rp_ = true;

  return true;
}

void VKCommandBuffer::end_render_pass(const VKFrameBuffer & /*framebuffer*/)
{

  if (begin_rp_) {
    vkCmdEndRenderPass(vk_command_buffer_);
    auto &sfim = VKContext::get()->sc_image_get();
    auto src_layout = sfim.current_layout_get();
    sfim.current_layout_set(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    debug::raise_vk_info(
        "VKContext::ImageTracker::transition   IMAGE[%llx] RenderPassTransition \n         %s ==> "
        "%s\n",
        (uint64_t)sfim.vk_image_handle(),
        to_string(src_layout),
        to_string(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));
    in_submit_ = true;
  }
  begin_rp_ = false;
}

void VKCommandBuffer::push_constants(const VKPushConstants &push_constants,
                                     const VkPipelineLayout vk_pipeline_layout,
                                     const VkShaderStageFlags vk_shader_stages)
{
  BLI_assert(push_constants.layout_get().storage_type_get() ==
             VKPushConstants::StorageType::PUSH_CONSTANTS);
  vkCmdPushConstants(vk_command_buffer_,
                     vk_pipeline_layout,
                     vk_shader_stages,
                     push_constants.offset(),
                     push_constants.layout_get().size_in_bytes(),
                     push_constants.data());
}

void VKCommandBuffer::fill(VKBuffer &buffer, uint32_t clear_data)
{
  vkCmdFillBuffer(vk_command_buffer_, buffer.vk_handle(), 0, buffer.size_in_bytes(), clear_data);
}

void VKCommandBuffer::copy(VKBuffer &dst_buffer,
                           VKTexture &src_texture,
                           Span<VkBufferImageCopy> regions)
{
  vkCmdCopyImageToBuffer(vk_command_buffer_,
                         src_texture.vk_image_handle(),
                         src_texture.current_layout_get(),
                         dst_buffer.vk_handle(),
                         regions.size(),
                         regions.data());
}
void VKCommandBuffer::copy(VKTexture &dst_texture,
                           VKBuffer &src_buffer,
                           Span<VkBufferImageCopy> regions)
{
  vkCmdCopyBufferToImage(vk_command_buffer_,
                         src_buffer.vk_handle(),
                         dst_texture.vk_image_handle(),
                         dst_texture.current_layout_get(),
                         regions.size(),
                         regions.data());
}

void VKCommandBuffer::clear(VkImage vk_image,
                            VkImageLayout vk_image_layout,
                            const VkClearColorValue &vk_clear_color,
                            Span<VkImageSubresourceRange> ranges)
{
  vkCmdClearColorImage(vk_command_buffer_,
                       vk_image,
                       vk_image_layout,
                       &vk_clear_color,
                       ranges.size(),
                       ranges.data());
}

void VKCommandBuffer::clear(Span<VkClearAttachment> attachments, Span<VkClearRect> areas)
{
  vkCmdClearAttachments(
      vk_command_buffer_, attachments.size(), attachments.data(), areas.size(), areas.data());
}

void VKCommandBuffer::draw(int v_first, int v_count, int i_first, int i_count)
{
  vkCmdDraw(vk_command_buffer_, v_count, i_count, v_first, i_first);
}

void VKCommandBuffer::pipeline_barrier(VkPipelineStageFlags source_stages,
                                       VkPipelineStageFlags destination_stages)
{
  vkCmdPipelineBarrier(vk_command_buffer_,
                       source_stages,
                       destination_stages,
                       0,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       0,
                       nullptr);
}

void VKCommandBuffer::pipeline_barrier(Span<VkImageMemoryBarrier> image_memory_barriers)
{
  vkCmdPipelineBarrier(vk_command_buffer_,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_DEPENDENCY_BY_REGION_BIT,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       image_memory_barriers.size(),
                       image_memory_barriers.data());
}

void VKCommandBuffer::pipeline_barrier(VkPipelineStageFlags source_stages,
                                       VkPipelineStageFlags destination_stages,
                                       Span<VkImageMemoryBarrier> image_memory_barriers)
{
  vkCmdPipelineBarrier(vk_command_buffer_,
                       source_stages,
                       destination_stages,
                       VK_DEPENDENCY_BY_REGION_BIT,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       image_memory_barriers.size(),
                       image_memory_barriers.data());
}

void VKCommandBuffer::dispatch(int groups_x_len, int groups_y_len, int groups_z_len)
{
  vkCmdDispatch(vk_command_buffer_, groups_x_len, groups_y_len, groups_z_len);
}

bool VKCommandBuffer::submit(bool toggle, bool fin)
{
  end_recording();

  if (in_submit_) {
    encode_recorded_commands();
    submit_encoded_commands(fin);
    in_submit_ = false;
  }
  else {
    if (fin) {
      submit_encoded_commands(fin);
    }
  }

  if (toggle) {
    return begin_recording();
  }
  return true;
}

void VKCommandBuffer::encode_recorded_commands()
{
  /* Intentionally not implemented. For the graphics pipeline we want to extract the
   * resources and its usages so we can encode multiple commands in the same command buffer with
   * the correct synchronizations. */
}

void VKCommandBuffer::submit_encoded_commands(bool fin)
{

  VkSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &vk_command_buffer_;

  if (!in_submit_) {
    if (!fin) {
      return;
    }
    submit_info.commandBufferCount = 0;
    submit_info.pCommandBuffers = nullptr;
    ;
  }

  in_flight_receive();
  vkResetFences(vk_device_, 1, &vk_fence_);

  submit_info.signalSemaphoreCount = 0;
  submit_info.waitSemaphoreCount = 0;

  VkSemaphore wait = sema_toggle_[(sema_own_ + 1) % 2];
  VkSemaphore finish = (fin) ? sema_fin_ : sema_toggle_[sema_own_];

  VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};

  submit_info.waitSemaphoreCount = (wait == VK_NULL_HANDLE) ? 0 : 1;
  submit_info.pWaitSemaphores = &wait;
  submit_info.pWaitDstStageMask = wait_stages;

  submit_info.signalSemaphoreCount = (finish == VK_NULL_HANDLE) ? 0 : 1;
  submit_info.pSignalSemaphores = &finish;

  debug::raise_vk_info(
      "\nSubmit Information Continue %d \n     wait semaphore [%llx]\n      signal semaphore "
      "[%llx]\n",
      (int)!fin,
      (uint64_t)wait,
      (uint64_t)finish);

  vkQueueSubmit(vk_queue_, 1, &submit_info, vk_fence_);

  sema_own_ = (sema_own_ + 1) % 2;

  vkQueueWaitIdle(vk_queue_);

  BLI_assert(!in_flight_);

  in_flight_ = true;

  submission_id_.next();

  if (fin) {
    sema_fin_ = VK_NULL_HANDLE;
  }
}

}  // namespace blender::gpu

namespace blender::gpu {

SafeImage::SafeImage()
{
  image_ = VK_NULL_HANDLE;
  format_ = VK_FORMAT_UNDEFINED;
  layout_ = VK_IMAGE_LAYOUT_MAX_ENUM;
  keep_ = false;
}

void SafeImage::validate(bool val)
{
  keep_ = val;
};

bool SafeImage::is_valid(const VkImage &image)
{
  if (image_ != image) {
    return false;
  }
  return keep_;
}

void SafeImage::init(VkImage &image, VkFormat &format)
{
  image_ = image;
  format_ = format;
  keep_ = true;
}

void SafeImage::current_layout_set(VkImageLayout new_layout)
{
  layout_ = new_layout;
  printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> image %llx   layout  %s   \n",
         (uint64_t)image_,
         to_string(new_layout));
};

VkImageLayout SafeImage::current_layout_get()
{
  return layout_;
};

VkImage SafeImage::vk_image_handle()
{
  return image_;
}

uint32_t ImageLayoutState::makeAccessMaskPipelineStageFlags(
    uint32_t accessMask, VkPipelineStageFlags supportedShaderBits)
{
  static const uint32_t accessPipes[] = {
    VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
    VK_ACCESS_INDEX_READ_BIT,
    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
    VK_ACCESS_UNIFORM_READ_BIT,
    supportedShaderBits,
    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    VK_ACCESS_SHADER_READ_BIT,
    supportedShaderBits,
    VK_ACCESS_SHADER_WRITE_BIT,
    supportedShaderBits,
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
    VK_ACCESS_TRANSFER_READ_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_ACCESS_TRANSFER_WRITE_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_ACCESS_HOST_READ_BIT,
    VK_PIPELINE_STAGE_HOST_BIT,
    VK_ACCESS_HOST_WRITE_BIT,
    VK_PIPELINE_STAGE_HOST_BIT,
    VK_ACCESS_MEMORY_READ_BIT,
    0,
    VK_ACCESS_MEMORY_WRITE_BIT,
    0,
#if VK_NV_device_generated_commands
    VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV,
    VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
    VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV,
    VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
#endif
#if VK_NV_ray_tracing
    VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,
    VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV | supportedShaderBits |
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
    VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV,
    VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
#endif
  };
  if (!accessMask) {
    return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  }

  uint32_t pipes = 0;
#define NV_ARRAY_SIZE(X) (sizeof((X)) / sizeof((X)[0]))

  for (uint32_t i = 0; i < NV_ARRAY_SIZE(accessPipes); i += 2) {
    if (accessPipes[i] & accessMask) {
      pipes |= accessPipes[i + 1];
    }
  }
#undef NV_ARRAY_SIZE
  if (!pipes) {
    return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  }
  return pipes;
};

VkImageMemoryBarrier ImageLayoutState::makeImageMemoryBarrier(VkImage img,
                                                              VkAccessFlags srcAccess,
                                                              VkAccessFlags dstAccess,
                                                              VkImageLayout oldLayout,
                                                              VkImageLayout newLayout,
                                                              VkImageAspectFlags aspectMask,
                                                              int basemip,
                                                              int miplevel)
{

  VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.srcAccessMask = srcAccess;
  barrier.dstAccessMask = dstAccess;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = img;
  barrier.subresourceRange = {0};
  barrier.subresourceRange.baseMipLevel = basemip;
  barrier.subresourceRange.aspectMask = aspectMask;
  barrier.subresourceRange.levelCount = (miplevel == -1) ? VK_REMAINING_MIP_LEVELS : miplevel;
  barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

  return barrier;
};

VkImageAspectFlags ImageLayoutState::get_aspect_flag(VkImageLayout srcLayout,
                                                     VkImageLayout dstLayout)
{
  /* Note that in larger applications, we could batch together pipeline
   *  barriers for better performance!
   */

  VkImageAspectFlags aspectMask = 0;

  if (srcLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
      dstLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    /*
       if (format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
         format == VK_FORMAT_D24_UNORM_S8_UINT) {
       aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
     }
     */
  }
  else {
    aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  return aspectMask;
}

#if 0
  GHOST_TSuccess GHOST_ContextVK::fail_image_layout() {

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (m_current_layouts[m_currentImage] != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
      BLI_assert(m_current_layouts[m_currentImage] ==
                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      return GHOST_kSuccess;
    }

    begin_submit_simple(cmd);

    blender::vulkan::GHOST_ImageTransition(
        cmd, m_swapchain_images[m_currentImage], getImageFormat(),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    end_submit_simple();
    m_current_layouts[m_currentImage] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    return GHOST_kSuccess;
  };
  GHOST_TSuccess GHOST_ContextVK::finalize_image_layout() {

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (m_current_layouts[m_currentImage] == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
      return GHOST_kSuccess;
    }
    if (m_current_layouts[m_currentImage] !=
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
      BLI_assert(false);
      return GHOST_kFailure;
    }

    begin_submit_simple(cmd);

    blender::vulkan::GHOST_ImageTransition(
        cmd, m_swapchain_images[m_currentImage], getImageFormat(),
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    end_submit_simple();

    m_current_layouts[m_currentImage] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    return GHOST_kSuccess;
  };
  void insert_image_memory_barrier(VkCommandBuffer command_buffer, VkImage image,
                                 VkAccessFlags src_access_mask,
                                 VkAccessFlags dst_access_mask,
                                 VkImageLayout old_layout,
                                 VkImageLayout new_layout,
                                 VkPipelineStageFlags src_stage_mask,
                                 VkPipelineStageFlags dst_stage_mask,
                                 VkImageSubresourceRange subresource_range) {

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.srcAccessMask = src_access_mask;
  barrier.dstAccessMask = dst_access_mask;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.image = image;
  barrier.subresourceRange = subresource_range;

  vkCmdPipelineBarrier(command_buffer, src_stage_mask, dst_stage_mask, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);
  };
#endif

};  // namespace blender::gpu
