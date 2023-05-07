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
  VK_BEFORE_RENDER_PASS_DEPTH,
  VK_BEFORE_PRESENT,
  VK_ENSURE_TEXTURE,
  VK_ENSURE_COPY_DST,
  VK_ENSURE_COPY_SRC,
  VK_TRANSITION_STATE_ALL
};

enum class VkTransitionStateRaw {
  VK_UNDEF2PRESE,
  VK_UNDEF2COLOR,
  VK_UNDEF2DEPTH,
  VK_PRESE2COLOR,
  VK_PRESE2GENER,
  VK_COLOR2PRESE,
  VK_UNDEF2GENER,
  VK_GENER2COLOR,
  VK_SHADER2COLOR,
  VK_SHADER2DEPTH,
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
  int current_mip_ = 0;
  SafeImage();

  bool is_valid(const VkImage &image);

  void validate(bool val);

  void init(VkImage &image, VkFormat &format);

  void current_layout_set(VkImageLayout new_layout);

  VkImageLayout current_layout_get();

  VkImage vk_image_handle();

  int current_mip_get()
  {
    return current_mip_;
  };
};

class ImageLayoutState {
 private:
  const VkAccessFlagBits acs_general =
      (VkAccessFlagBits)(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT |
                         VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
  const VkAccessFlagBits acs_attach = (VkAccessFlagBits)(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  const VkAccessFlagBits acs_shader = (VkAccessFlagBits)(VK_ACCESS_SHADER_READ_BIT);
  const VkAccessFlagBits acs_undef = (VkAccessFlagBits)(VK_ACCESS_NONE_KHR);
  const VkAccessFlagBits acs_transfer_dst = (VkAccessFlagBits)(VK_ACCESS_TRANSFER_WRITE_BIT);
  const VkAccessFlagBits acs_transfer_src = (VkAccessFlagBits)(VK_ACCESS_TRANSFER_READ_BIT);
  const VkAccessFlagBits acs_depth = (VkAccessFlagBits)(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

  VkAccessFlagBits to_vk_access_flag(VkImageLayout vk_image_layout)
  {
    switch(vk_image_layout)
    {
      case VK_IMAGE_LAYOUT_UNDEFINED:
        return acs_undef;
      case VK_IMAGE_LAYOUT_GENERAL:
        return acs_general;
      case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return acs_attach;
      case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
      case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
      case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
      case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
        return acs_depth;
      case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
      case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
      case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return acs_shader;
      case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return acs_transfer_src;
      case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return acs_transfer_dst;
      case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return VK_ACCESS_MEMORY_READ_BIT;
      case VK_IMAGE_LAYOUT_PREINITIALIZED:
#ifdef VK_ENABLE_BETA_EXTENSIONS
      case VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR:
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
      case VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR:
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
      case VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR:
#endif
      case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
      case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
      case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
#ifdef VK_ENABLE_BETA_EXTENSIONS
      case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR:
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
      case VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR:
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
      case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR:
#endif
      case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR :
      case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR:
      case VK_IMAGE_LAYOUT_MAX_ENUM:
      default:
        BLI_assert_unreachable();
    }
    return acs_undef;
  }

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
  void barrier(VkCommandBuffer &cmd,
               T *safeim,
               VkImageLayout dst_layout,
               VkImageLayout src_layout,
               VkAccessFlagBits src_acs,
               VkAccessFlagBits dst_acs)
  {
    VkPipelineStageFlags srcPipe = makeAccessMaskPipelineStageFlags(src_acs);
    VkPipelineStageFlags dstPipe = makeAccessMaskPipelineStageFlags(dst_acs);

    VkImageAspectFlags aspect = get_aspect_flag(src_layout, dst_layout);
    static int  stats = 0;
    if(stats==117){
      printf("");
    }
    stats++;
    VkImageMemoryBarrier barrier = makeImageMemoryBarrier(safeim->vk_image_handle(),
                                                          src_acs,
                                                          dst_acs,
                                                          src_layout,
                                                          dst_layout,
                                                          aspect,
                                                          safeim->current_mip_get(),
                                                          mip_level);


    vkCmdPipelineBarrier(cmd,
                       srcPipe,
                       dstPipe,
                       VK_DEPENDENCY_BY_REGION_BIT,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       1,
                       &barrier);
    safeim->current_layout_set(dst_layout);
  }
  int mip_level = 0;

 public:
  template<typename T>
  void transition(VkCommandBuffer &cmd,
                         T *safeim,
                         VkImageLayout dst_layout,
                         VkImageLayout src_layout)
  {
    BLI_assert(dst_layout != src_layout);
    VkAccessFlagBits src_acs = to_vk_access_flag(src_layout);
    VkAccessFlagBits dst_acs = to_vk_access_flag(dst_layout);

    barrier(cmd, safeim, dst_layout, src_layout, src_acs, dst_acs);
  };

  void mip_level_set(int level)
  {
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
  VKContext* context_  = VK_NULL_HANDLE;
  VkDevice vk_device_ = VK_NULL_HANDLE;
  VkCommandBuffer vk_command_buffer_ = VK_NULL_HANDLE;
  VkQueue vk_queue_ = VK_NULL_HANDLE;
  VkCommandBuffer vk_transfer_command_ = VK_NULL_HANDLE;
  VkCommandPool vk_command_pool_ = VK_NULL_HANDLE;

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
  int sema_signal_ = -1;
  uint8_t sema_frame_ = 0;
  VkFence vk_fence_trans_ = VK_NULL_HANDLE;
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

  VKCommandBuffer();
  virtual ~VKCommandBuffer();
  bool init(const VkDevice vk_device, const VkQueue vk_queue, VkCommandBuffer vk_command_buffer);
  bool begin_recording();
  void end_recording(bool imm_submit = false);
  void create_resource(VKContext* context);
  bool ensure_render_pass()
  {
    if (begin_rp_) {
      return true;
    }
    if (in_submit_) {
      submit(true, false);
    }
    if (!in_toggle_) {
      if (!begin_recording()) {
        return false;
      };
    }
    return false;
  }
  template<typename T> bool begin_render_pass(const VKFrameBuffer &framebuffer, T *sfim);
  void end_render_pass(const VKFrameBuffer &framebuffer);
  void ensure_no_render_pass();
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
  void copy_imm(VKTexture &dst_texture,
                           VKBuffer &src_buffer,
                           Span<VkBufferImageCopy> regions);
  void blit(VKTexture &dst_texture, VKTexture &src_texture, Span<VkImageBlit> regions);
  void pipeline_barrier(VkPipelineStageFlags source_stages,
                        VkPipelineStageFlags destination_stages);
  void pipeline_barrier(Span<VkImageMemoryBarrier> image_memory_barriers);
  void pipeline_barrier(VkPipelineStageFlags source_stages,
                        VkPipelineStageFlags destination_stages,
                        Span<VkImageMemoryBarrier> image_memory_barriers);
  void in_flight_receive(VkFence& vk_fence,bool& in_flight);
  template<typename T>
  bool image_transition(T *sfim,
                        VkTransitionState eTrans,
                        bool recorded = false,
                        VkImageLayout dst_layout = VK_IMAGE_LAYOUT_MAX_ENUM,
                        int mip_level = 1);
  bool image_transition_from_framebuffer(const VKFrameBuffer& framebuffer);
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
  void draw_indexed(int idx_count, int i_count, int idx_first,int v_first, int i_first);

  void bind(const VKPipeline &vk_pipeline, VkPipelineBindPoint bind_point);
  void bind(const VKDescriptorSet &descriptor_set,
            const VkPipelineLayout vk_pipeline_layout,
            VkPipelineBindPoint bind_point);
  void bind_vertex_buffers(uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *pBuffers);
  void bind(const uint32_t binding, const VkBuffer &vk_buffer, const VkDeviceSize offset);
  void bind(const uint32_t binding,
            const VKVertexBuffer &vertex_buffer,
            const VkDeviceSize offset);
  void bind(const VKIndexBuffer &index_buffer, VkIndexType index_type);
  void bind(const VkDescriptorSet vk_descriptor_set,
                           int set_location,
                           const VkPipelineLayout vk_pipeline_layout,
                           VkPipelineBindPoint bind_point);
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
  VkRenderPassBeginInfo render_pass_begin_info = {};
  void imm_begin();
  void imm_end(bool fail = false);
 private:
  void encode_recorded_commands();
  void submit_encoded_commands(bool fin = true);
};

}  // namespace blender::gpu
