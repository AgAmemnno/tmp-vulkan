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
  VK_ENSURE_COPY_DST,
  VK_ENSURE_COPY_SRC,
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
  VK_ANY2SHADER_READ,
  VK_ANY2TRANS_DST,
  VK_ANY2TRANS_SRC,
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
  int   current_mip_ = 0;
  SafeImage();

  bool is_valid(const VkImage &image);

  void validate(bool val);

  void init(VkImage &image, VkFormat &format);

  void current_layout_set(VkImageLayout new_layout);

  VkImageLayout current_layout_get();

  VkImage vk_image_handle();

  int current_mip_get(){
    return current_mip_;
  };
};

class ImageLayoutState {
 private:

  const VkAccessFlagBits acs_general   =  (VkAccessFlagBits)(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT| VK_ACCESS_TRANSFER_READ_BIT| VK_ACCESS_TRANSFER_WRITE_BIT);
  const VkAccessFlagBits acs_attach    =   (VkAccessFlagBits)(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  const VkAccessFlagBits acs_shader    =  (VkAccessFlagBits)(VK_ACCESS_SHADER_READ_BIT);
  const VkAccessFlagBits acs_undef           =  (VkAccessFlagBits)(VK_ACCESS_NONE_KHR);
  const VkAccessFlagBits acs_transfer_dst  =  (VkAccessFlagBits)(VK_ACCESS_TRANSFER_WRITE_BIT);
  const VkAccessFlagBits acs_transfer_src  =  (VkAccessFlagBits)(VK_ACCESS_TRANSFER_READ_BIT);

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

  template<typename T>
  void barrier(VKCommandBuffer &cmd, T *safeim, VkImageLayout dst_layout,VkImageLayout src_layout,VkAccessFlagBits src_acs,VkAccessFlagBits dst_acs)
  {
    VkPipelineStageFlags srcPipe = makeAccessMaskPipelineStageFlags(src_acs);
    VkPipelineStageFlags dstPipe = makeAccessMaskPipelineStageFlags(dst_acs);

    VkImageAspectFlags aspect = get_aspect_flag(src_layout, dst_layout);

    VkImageMemoryBarrier barrier = makeImageMemoryBarrier(
        safeim->vk_image_handle(), src_acs, dst_acs, src_layout, dst_layout, aspect,safeim->current_mip_get() , mip_level);

    cmd.pipeline_barrier(srcPipe, dstPipe, {barrier});

    safeim->current_layout_set(dst_layout);
  }
  int mip_level = 0;
 public:
  template<typename T>
  bool init_image_layout(VKCommandBuffer &cmd, T *safeim, VkImageLayout dst_layout, VkImageLayout src_layout)
  {
    BLI_assert(ELEM(dst_layout,
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

    BLI_assert(src_layout == VK_IMAGE_LAYOUT_UNDEFINED);
    const bool present   = (dst_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    const bool color       = (dst_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const bool shader    = (dst_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkAccessFlagBits src_acs = VK_ACCESS_NONE_KHR;
    VkAccessFlagBits dst_acs = (present) ? VK_ACCESS_MEMORY_READ_BIT : (color)?acs_attach: (shader)?acs_shader:acs_general;

    barrier(cmd,safeim,dst_layout,src_layout,src_acs,dst_acs);

    return true;
  };

  template<typename T>
  bool transfer(VKCommandBuffer &cmd, T *safeim, VkImageLayout dst_layout, VkImageLayout src_layout)
  {

    const bool  dst         = dst_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    const bool present   = (src_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    const bool color       = (src_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const bool shader    = (src_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);


    VkAccessFlagBits src_acs = (present) ? VK_ACCESS_MEMORY_READ_BIT : (color)?acs_attach: (shader)?acs_shader:acs_general;
    VkAccessFlagBits dst_acs = (dst)?acs_transfer_dst:acs_transfer_src;

    barrier(cmd,safeim,dst_layout,src_layout,src_acs,dst_acs);

    return true;
  };

  template<typename T>
  bool shader(VKCommandBuffer &cmd, T *safeim, VkImageLayout dst_layout, VkImageLayout src_layout)
  {
    VkAccessFlagBits dst_acs = (dst_layout == VK_IMAGE_LAYOUT_GENERAL)?acs_general:acs_shader;
    VkAccessFlagBits src_acs;
    switch(src_layout){
      case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        src_acs = VK_ACCESS_MEMORY_READ_BIT;
        break;
      case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        src_acs = acs_attach;
        break;
      case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        src_acs = acs_shader;
        break;
      case VK_IMAGE_LAYOUT_GENERAL:
        src_acs = acs_general;
        break;
      case VK_IMAGE_LAYOUT_UNDEFINED:
        src_acs = acs_undef;
        break;
      case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL :
        src_acs = acs_transfer_src;
        break;
      case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL :
        src_acs = acs_transfer_dst;
        break;
      default:
        BLI_assert_unreachable();
        break;
    }

    barrier(cmd,safeim,dst_layout,src_layout,src_acs,dst_acs);
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
    VkImageLayout dst_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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
  void mip_level_set(int level){
    mip_level = level;
  }
};  // blender::gpu

}  //  namespace blender::gpu

namespace blender::gpu {
class VKBuffer;
class VKDescriptorSet;
class VKFrameBuffer;
class VKIndexBuffer;
class VKPipeline;
class VKPushConstants;
class VKTexture;
class VKVertexBuffer;

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
  template<typename T> bool begin_render_pass(const VKFrameBuffer &framebuffer, T &sfim);
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
                        VkImageLayout dst_layout = VK_IMAGE_LAYOUT_MAX_ENUM,int mip_level= 1);

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

  void bind(const VKPipeline &vk_pipeline, VkPipelineBindPoint bind_point);
  void bind(const VKDescriptorSet &descriptor_set,
            const VkPipelineLayout vk_pipeline_layout,
            VkPipelineBindPoint bind_point);
  void bind_vertex_buffers(uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *pBuffers);
  void bind(const uint32_t binding,
                           const VkBuffer &vk_buffer,
                           const VkDeviceSize offset);
  void bind(const uint32_t binding,
            const VKVertexBuffer &vertex_buffer,
            const VkDeviceSize offset);
  void bind(const VKIndexBuffer &index_buffer, VkIndexType index_type);

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
