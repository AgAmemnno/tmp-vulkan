/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_common.hh"
#include "vk_debug.hh"
#include "vk_framebuffer.hh"
#include "vk_resource_tracker.hh"

#include "BLI_utility_mixins.hh"

namespace blender::gpu {

enum class VkTransitionState {
  VK_BEFORE_RENDER_PASS,
  VK_BEFORE_PRESENT,
  VK_ENSURE_TEXTURE,
  VK_TRANSITION_STATE_ALL
};

enum class VkTransitionStateRaw {
  VK_UNDEF2PRESE,
  VK_UNDEF2COLOR,
  VK_PRESE2COLOR,
  VK_PRESE2GENER,
  VK_COLOR2PRESE,
  VK_UNDEF2GENER,
  VK_GENER2COLOR,
  VK_TRANSITION_STATE_RAW_ALL
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
  bool init_image_layout(VKCommandBuffer &cmd, T *safeim, VkImageLayout dst_layout)
  {

    VkImageLayout src_layout = safeim->current_layout_get();

    if (src_layout == dst_layout) {
      return false;
    }

    BLI_assert(ELEM(dst_layout,
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
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
        safeim->vk_image_handle(), src_acs, dst_acs, src_layout, dst_layout, aspect, 0, 1);

    cmd.pipeline_barrier(srcPipe, dstPipe, {barrier});

    safeim->current_layout_set(dst_layout);

    return true;
  };

  template<typename T>
  bool pre_reder_image_layout(VKCommandBuffer &cmd, T *safeim, VkImageLayout dst_layout)
  {

    VkImageLayout src_layout = safeim->current_layout_get();

    if (src_layout == dst_layout) {
      return false;
    }

    BLI_assert(ELEM(src_layout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_GENERAL));

    VkAccessFlagBits src_acs = (src_layout == VK_IMAGE_LAYOUT_GENERAL) ?
                                   (VkAccessFlagBits)(VK_ACCESS_MEMORY_READ_BIT |
                                                      VK_ACCESS_MEMORY_WRITE_BIT) :
                                   VK_ACCESS_MEMORY_READ_BIT;
    VkAccessFlagBits dst_acs = (VkAccessFlagBits)(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    VkPipelineStageFlags srcPipe = makeAccessMaskPipelineStageFlags(src_acs);
    VkPipelineStageFlags dstPipe = makeAccessMaskPipelineStageFlags(dst_acs);

    VkImageAspectFlags aspect = get_aspect_flag(src_layout, dst_layout);

    VkImageMemoryBarrier barrier = makeImageMemoryBarrier(
        safeim->vk_image_handle(), src_acs, dst_acs, src_layout, dst_layout, aspect, 0, 1);

    cmd.pipeline_barrier(srcPipe, dstPipe, {barrier});

    safeim->current_layout_set(dst_layout);

    return true;
  };

  template<typename T> bool pre_sent_image_layout(VKCommandBuffer &cmd, T *safeim)
  {

    VkImageLayout src_layout = safeim->current_layout_get();
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
        safeim->vk_image_handle(), src_acs, dst_acs, src_layout, dst_layout, aspect, 0, 1);

    cmd.pipeline_barrier(srcPipe, dstPipe, {barrier});

    safeim->current_layout_set(dst_layout);

    return true;
  };

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
  bool in_rp_submit_ = false;

  VkSemaphore sema_fin_;
  VkSemaphore sema_toggle_[2];
  const int sema_own_ = 1;
  int        sema_signal_ = -1;
  uint8_t sema_frame_ = 0;

 public:


  void set_remote_semaphore(VkSemaphore &se)
  {
    sema_toggle_[(sema_own_ + 1) % 2] = se;
    sema_signal_ = sema_own_;
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
    begin_cmd_ = begin_rp_ = false;
  };
  virtual ~VKCommandBuffer();
  bool init(const VkDevice vk_device, const VkQueue vk_queue, VkCommandBuffer vk_command_buffer);
  bool begin_recording();
  void end_recording(bool imm_submit = false);
  void bind(const VKPipeline &vk_pipeline, VkPipelineBindPoint bind_point);
  void bind(const VKDescriptorSet &descriptor_set,
            const VkPipelineLayout vk_pipeline_layout,
            VkPipelineBindPoint bind_point);

  bool ensure_render_pass(){
    if (begin_rp_) {
      return true;
    }
    if(in_submit_){
      submit(true,false);
    }
    if (!in_toggle_) {
      if (!begin_recording()) {
        return false;
      };
    }
    return false;
  }
  template<typename T> bool begin_render_pass(const VKFrameBuffer &framebuffer, T &sfim)
  {
    #if 0
    if (begin_rp_) {
      return true;
    }
    if(in_submit_){
      submit(true,false);
    }
    if (!in_toggle_) {
      if (!begin_recording()) {
        return false;
      };
    }
    #endif
    image_transition(&sfim,
                     VkTransitionState::VK_BEFORE_RENDER_PASS,
                     true,
                     framebuffer.vk_render_pass_init_layout_get());

    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = framebuffer.vk_render_pass_get();
    render_pass_begin_info.framebuffer = framebuffer.vk_framebuffer_get();
    render_pass_begin_info.renderArea = framebuffer.vk_render_area_get();
    vkCmdBeginRenderPass(vk_command_buffer_, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    begin_rp_ = true;

    return true;
  }
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
  void copy(VKBuffer &dst_buffer, VKBuffer &src_buffer, Span<VkBufferCopy> regions);

  void pipeline_barrier(VkPipelineStageFlags source_stages,
                        VkPipelineStageFlags destination_stages);
  void pipeline_barrier(Span<VkImageMemoryBarrier> image_memory_barriers);
  void pipeline_barrier(VkPipelineStageFlags source_stages,
                        VkPipelineStageFlags destination_stages,
                        Span<VkImageMemoryBarrier> image_memory_barriers);

  void in_flight_receive();

  template<typename T>
  bool image_transition(T *sfim,
                        VkTransitionState eTrans,
                        bool recorded = false,
                        VkImageLayout dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

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

  void bind_vertex_buffers(uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *pBuffers);

  void viewport(VkViewport &viewport);
  void scissor(VkRect2D &scissor);
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
