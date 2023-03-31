/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_common.hh"
#include "vk_debug.hh"
#include "vk_resource_tracker.hh"

#include "BLI_utility_mixins.hh"

namespace blender::gpu {

enum class VkTransitionState {
  VK_UNDEF2PRESE,
  VK_UNDEF2COLOR,
  VK_PRESE2COLOR,
  VK_COLOR2PRESE,
  VK_TRANSITION_STATE_ALL
};

typedef struct sc_im_prop {

  int nums;
  VkFormat format;
  VkImageLayout initial;

} sc_improp;

/*need some properties for swap chain. */
static sc_improp vk_im_prop = {2, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED};

class SafeImage {
 private:
  VkImage image_;
  VkFormat format_;
  VkImageLayout layout_;
  bool keep_;

 public:
  SafeImage();

  bool is_valid(const VkImage &image);

  void validate(bool val);

  void init(VkImage &image, VkFormat &format);

  void current_layout_set(VkImageLayout new_layout);

  VkImageLayout current_layout_get();

  VkImage vk_image_handle();
};

class ImageLayoutState {

 private:
  uint32_t makeAccessMaskPipelineStageFlags(
      uint32_t accessMask,
      VkPipelineStageFlags supportedShaderBits = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

  VkImageMemoryBarrier makeImageMemoryBarrier(VkImage img,
                                              VkAccessFlags srcAccess,
                                              VkAccessFlags dstAccess,
                                              VkImageLayout oldLayout,
                                              VkImageLayout newLayout,
                                              VkImageAspectFlags aspectMask,
                                              int basemip = 0,
                                              int miplevel = -1);

  VkImageAspectFlags get_aspect_flag(VkImageLayout srcLayout, VkImageLayout dstLayout);

 public:
  template<typename T>
  bool init_image_layout(VKCommandBuffer &cmd, T &safeim, VkImageLayout dst_layout)
  {

    VkImageLayout src_layout = safeim.current_layout_get();

    if (src_layout == dst_layout) {
      return false;
    }

    BLI_assert(ELEM(
        dst_layout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    BLI_assert(src_layout == VK_IMAGE_LAYOUT_UNDEFINED);

    const bool present = (dst_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VkAccessFlagBits src_acs = VK_ACCESS_NONE_KHR;
    VkAccessFlagBits dst_acs = (present) ?
                                   VK_ACCESS_MEMORY_READ_BIT :
                                   (VkAccessFlagBits)(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    VkPipelineStageFlags srcPipe = makeAccessMaskPipelineStageFlags(src_acs);
    VkPipelineStageFlags dstPipe = makeAccessMaskPipelineStageFlags(dst_acs);

    VkImageAspectFlags aspect = get_aspect_flag(src_layout, dst_layout);

    VkImageMemoryBarrier barrier = makeImageMemoryBarrier(
        safeim.vk_image_handle(), src_acs, dst_acs, src_layout, dst_layout, aspect, 0, 1);

    cmd.pipeline_barrier(srcPipe, dstPipe, {barrier});

    safeim.current_layout_set(dst_layout);

    return true;
  };

  template<typename T> bool pre_reder_image_layout(VKCommandBuffer &cmd, T &safeim)
  {

    VkImageLayout src_layout = safeim.current_layout_get();
    VkImageLayout dst_layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;  // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (src_layout == dst_layout) {
      return false;
    }
    BLI_assert(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR == src_layout);

    VkAccessFlagBits src_acs = VK_ACCESS_MEMORY_READ_BIT;
    VkAccessFlagBits dst_acs = (VkAccessFlagBits)(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    VkPipelineStageFlags srcPipe = makeAccessMaskPipelineStageFlags(src_acs);
    VkPipelineStageFlags dstPipe = makeAccessMaskPipelineStageFlags(dst_acs);

    VkImageAspectFlags aspect = get_aspect_flag(src_layout, dst_layout);

    VkImageMemoryBarrier barrier = makeImageMemoryBarrier(
        safeim.vk_image_handle(), src_acs, dst_acs, src_layout, dst_layout, aspect, 0, 1);

    cmd.pipeline_barrier(srcPipe, dstPipe, {barrier});

    safeim.current_layout_set(dst_layout);

    return true;
  };

  template<typename T> bool pre_sent_image_layout(VKCommandBuffer &cmd, T &safeim)
  {

    VkImageLayout src_layout = safeim.current_layout_get();
    VkImageLayout dst_layout =
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (src_layout == dst_layout) {
      return false;
    }
    BLI_assert(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL == src_layout);

    VkAccessFlagBits src_acs = (VkAccessFlagBits)(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    VkAccessFlagBits dst_acs = VK_ACCESS_MEMORY_READ_BIT;

    VkPipelineStageFlags srcPipe = makeAccessMaskPipelineStageFlags(src_acs);
    VkPipelineStageFlags dstPipe = makeAccessMaskPipelineStageFlags(dst_acs);

    VkImageAspectFlags aspect = get_aspect_flag(src_layout, dst_layout);

    VkImageMemoryBarrier barrier = makeImageMemoryBarrier(
        safeim.vk_image_handle(), src_acs, dst_acs, src_layout, dst_layout, aspect, 0, 1);

    cmd.pipeline_barrier(srcPipe, dstPipe, {barrier});

    safeim.current_layout_set(dst_layout);

    return true;
  };
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
};
#endif
};  // blender::gpu

}  //  namespace blender::gpu

namespace blender::gpu {
class VKBuffer;
class VKDescriptorSet;
class VKFrameBuffer;
class VKPipeline;
class VKPushConstants;
class VKTexture;

/** Command buffer to keep track of the life-time of a command buffer. */
class VKCommandBuffer : NonCopyable, NonMovable {
  /** None owning handle to the command buffer and device. Handle is owned by `GHOST_ContextVK`. */
  VkDevice vk_device_ = VK_NULL_HANDLE;
  VkCommandBuffer vk_command_buffer_ = VK_NULL_HANDLE;
  VkQueue vk_queue_ = VK_NULL_HANDLE;

  /** Owning handles */
  VkFence vk_fence_ = VK_NULL_HANDLE;
  bool in_flight_ = false;
  VKSubmissionID submission_id_;
  bool begin_cmd_;
  bool begin_rp_;
  ImageLayoutState layout_state_;
  uint32_t vk_fb_id_;
  bool in_toggle_ = false;
  bool in_submit_ = false;

  VkSemaphore sema_fin_;
  VkSemaphore sema_toggle_[2];
  int sema_own_ = -1;
  uint8_t sema_frame_ = 0;

 public:
  VkSemaphore &get_wait_semaphore()
  {
    return sema_toggle_[(sema_own_ + 1) % 2];
  }

  void set_wait_semaphore(VkSemaphore &se)
  {
    sema_toggle_[(sema_own_ + 1) % 2] = se;
  }

  VkSemaphore &get_fin_semaphore()
  {
    return sema_fin_;
  }

  void set_fin_semaphore(VkSemaphore &se)
  {
    sema_fin_ = se;
  }

  uint8_t get_sema_frame() const
  {
    return sema_frame_;
  }

  void set_sema_frame(uint8_t i)
  {
    sema_frame_ = i;
  }

  VKCommandBuffer()
  {
    sema_frame_ = 3;
    in_flight_ = false;
    sema_fin_ = sema_toggle_[0] = sema_toggle_[1] = VK_NULL_HANDLE;
  };
  virtual ~VKCommandBuffer();
  bool init(const VkDevice vk_device, const VkQueue vk_queue, VkCommandBuffer vk_command_buffer);
  bool begin_recording();
  void end_recording();
  void bind(const VKPipeline &vk_pipeline, VkPipelineBindPoint bind_point);
  void bind(const VKDescriptorSet &descriptor_set,
            const VkPipelineLayout vk_pipeline_layout,
            VkPipelineBindPoint bind_point);
  bool begin_render_pass(const VKFrameBuffer &framebuffer, SafeImage &sfim);
  void end_render_pass(const VKFrameBuffer &framebuffer);

  /**
   * Add a push constant command to the command buffer.
   *
   * Only valid when the storage type of push_constants is StorageType::PUSH_CONSTANTS.
   */
  void push_constants(const VKPushConstants &push_constants,
                      const VkPipelineLayout vk_pipeline_layout,
                      const VkShaderStageFlags vk_shader_stages);
  void dispatch(int groups_x_len, int groups_y_len, int groups_z_len);
  /** Copy the contents of a texture MIP level to the dst buffer. */
  void copy(VKBuffer &dst_buffer, VKTexture &src_texture, Span<VkBufferImageCopy> regions);
  void copy(VKTexture &dst_texture, VKBuffer &src_buffer, Span<VkBufferImageCopy> regions);
  void pipeline_barrier(VkPipelineStageFlags source_stages,
                        VkPipelineStageFlags destination_stages);
  void pipeline_barrier(Span<VkImageMemoryBarrier> image_memory_barriers);
  void pipeline_barrier(VkPipelineStageFlags source_stages,
                        VkPipelineStageFlags destination_stages,
                        Span<VkImageMemoryBarrier> image_memory_barriers);

  void in_flight_receive();

  template<typename T>
  bool image_transition(T &sfim, VkTransitionState eTrans, bool recorded = false)
  {

    bool trans = false;
    VkImageLayout src_layout = sfim.current_layout_get();
    switch (eTrans) {
      case VkTransitionState::VK_UNDEF2PRESE:
        trans = layout_state_.init_image_layout(*this, sfim, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        break;
      case VkTransitionState::VK_UNDEF2COLOR:
        trans = layout_state_.init_image_layout(
            *this, sfim, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        break;
      case VkTransitionState::VK_PRESE2COLOR:
        trans = layout_state_.pre_reder_image_layout(*this, sfim);
        break;
      case VkTransitionState::VK_COLOR2PRESE:
        trans = layout_state_.pre_sent_image_layout(*this, sfim);
        break;
      case VkTransitionState::VK_TRANSITION_STATE_ALL:
      default:
        BLI_assert_unreachable();
        break;
    }

    if (trans) {
      VkImageLayout dst_layout = sfim.current_layout_get();
      debug::raise_vk_info(
          "VKContext::ImageTracker::transition   IMAGE[%llx]  pipelineBarrier \n         %s ==> "
          "%s\n",
          (uint64_t)sfim.vk_image_handle(),
          to_string(src_layout),
          to_string(dst_layout));
    }

    if (trans && recorded) {

      submit();
    }

    return trans;
  };

  /**
   * Clear color image resource.
   */
  void clear(VkImage vk_image,
             VkImageLayout vk_image_layout,
             const VkClearColorValue &vk_clear_color,
             Span<VkImageSubresourceRange> ranges);

  /**
   * Clear attachments of the active framebuffer.
   */
  void clear(Span<VkClearAttachment> attachments, Span<VkClearRect> areas);
  void fill(VKBuffer &buffer, uint32_t data);

  void draw(int v_first, int v_count, int i_first, int i_count);

  /**
   * Stop recording commands, encode + send the recordings to Vulkan, wait for the until the
   * commands have been executed and start the command buffer to accept recordings again.
   */
  bool submit(bool toggle = true, bool fin = false);

  const VKSubmissionID &submission_id_get() const
  {
    return submission_id_;
  }

  const bool is_begin_rp()
  {
    return begin_rp_;
  }

 private:
  void encode_recorded_commands();
  void submit_encoded_commands(bool fin = true);
};

}  // namespace blender::gpu
