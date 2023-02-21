/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2020, Blender Foundation.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "gpu_state_private.hh"
#include "vk_shader.hh"
#include "vk_layout.hh"
namespace blender {
namespace gpu {


  class VKTexture;



/**
 * State manager keeping track of the draw state and applying it before drawing.
 * Opengl Implementation.
 **/
class VKStateManager : public StateManager {
 public:
  /** Another reference to the active frame-buffer. */
  VKFrameBuffer *active_fb = nullptr;
  
 private:
 
  
  /** Current state of the GL implementation. Avoids resetting the whole state for every change. */
  GPUState current_;
  GPUStateMutable current_mutable_;
  /** Limits. */
  float line_width_range_[2];


  VkImageType targets_[64] = {};
  VkDescriptorImageInfo textures_[64] = {};
  GLuint samplers_[64] = {0};
  uint64_t dirty_texture_binds_ = 0;

  GLuint images_[8] = {0};
  GLenum formats_[8] = {0};
  uint8_t dirty_image_binds_ = 0;
  VKContext *ctx_;

  

 public:

  VKStateManager(VKContext *_ctx);
  ~VKStateManager();
  void apply_state(void) override;
  void force_state(void) override;

  void issue_barrier(eGPUBarrier barrier_bits) override;
 
  void texture_bind(Texture *tex, eGPUSamplerState sampler, int unit) override;
  void texture_unbind(Texture *tex) override;
  void texture_unbind_all(void) override;

  void image_bind(Texture *tex, int unit) override;
  void image_unbind(Texture *tex) override;
  void image_unbind_all(void) override;

  void texture_unpack_row_length_set(uint len) override;


  void texture_bind_temp(VKTexture* tex);

  void set_color_blend_from_fb(VKFrameBuffer *fb);

  VkGraphicsPipelineCreateInfo get_pipelinecreateinfo(VkRenderPass vkRP, VkPipelineLayout &layout);
  static VKGraphicsPipelineStateDescriptor &getPipelineStateDesc();
  static PipelineStateCreateInfoVk &getPipelineStateCI();
  static void set_prim_type(const GPUPrimType prim);

  static void cmd_dynamic_state(VkCommandBuffer &cmd);

  static void set_raster_discard();
  uint32_t unpack_row_length=0;

 private:


  static void set_write_mask(eGPUWriteMask value);
  static void set_depth_test(eGPUDepthTest value);
  static void set_stencil_test(eGPUStencilTest test, eGPUStencilOp operation);
  static void set_stencil_mask(eGPUStencilTest test, const GPUStateMutable state);
  static void set_clip_distances(int new_dist_len, int old_dist_len);
  static void set_logic_op(bool enable);
  static void set_facing(bool invert);
  static void set_backface_culling(eGPUFaceCullTest test);
  static void set_provoking_vert(eGPUProvokingVertex vert);
  static void set_shadow_bias(bool enable);
  static void set_blend(eGPUBlend value);


  void set_state(const GPUState &state);
  void set_mutable_state(const GPUStateMutable &state){};
  void set_mutable_state(VkCommandBuffer commandBuffer, const GPUStateMutable &state);

  
  void texture_bind_apply();
  uint64_t bound_texture_slots();
  void image_bind_apply();
  uint8_t bound_image_slots();


  MEM_CXX_CLASS_ALLOC_FUNCS("VKStateManager")
};

}  // namespace gpu
}  // namespace blender
