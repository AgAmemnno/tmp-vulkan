/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "gpu_state_private.hh"

#include "vk_vertex_array.hh"

namespace blender {
namespace gpu {

class VKTexture;
class VKShader;

struct PipelineStateCreateInfoVk {
  bool dirty;
  bool initialised;

  /* Shader resources. */
  VKShader *null_shader;
  /* Active Shader State. */
  VKShader *active_shader;
  VkGraphicsPipelineCreateInfo pipelineCI_;

  VkPipelineDynamicStateCreateInfo dynamic = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                              NULL};
  VkPipelineViewportStateCreateInfo viewport = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL};
  VkPipelineMultisampleStateCreateInfo multisample = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, NULL};

  VkPipelineInputAssemblyStateCreateInfo inputassembly = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL};
  VkPipelineRasterizationStateCreateInfo rasterization = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, NULL};
  VkPipelineDepthStencilStateCreateInfo depthstencil = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, NULL};
  VkPipelineColorBlendStateCreateInfo colorblend = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL};

  Vector<VkPipelineColorBlendAttachmentState> colorblend_attachment = {};
  VkPipelineRasterizationProvokingVertexStateCreateInfoEXT provokingvertex = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT, NULL};
  VkPipelineRasterizationLineStateCreateInfoEXT rasterline = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT, NULL};

  eGPUStencilTest _stenciltest;
  bool scissor_enabled;
  VkViewport viewport_cache;
  VkRect2D scissor_cache;

  float point_size = 1.0f;

  PipelineStateCreateInfoVk()
  {
    colorblend_attachment.resize(1);
    colorblend.pAttachments = colorblend_attachment.data();
    colorblend.attachmentCount = 1;

    provokingvertex.pNext = &rasterline;
    rasterization.pNext = &provokingvertex;
    initialised = true;
    null_shader = nullptr;
    /* Active Shader State. */
    active_shader = nullptr;
    viewport_cache = {0, 0, 0, 0};
    scissor_cache = {{0, 0}, {0, 0}};
  };
};

class VKStateManager : public StateManager {
 public:
  /** Another reference to the active frame-buffer. */
  VKFrameBuffer *active_fb = nullptr;

 private:
  PipelineStateCreateInfoVk vk_state_;
  /** Current state of the GL implementation. Avoids resetting the whole state
   * for every change. */
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

  void texture_bind_temp(VKTexture *tex);

  void set_color_blend_from_fb(VKFrameBuffer *fb);

  PipelineStateCreateInfoVk &get_pipeline_state();

  VkGraphicsPipelineCreateInfo &get_pipeline_create_info(VkRenderPass vkRP,
                                                         VkPipelineLayout &layout);

  void set_prim_type(const GPUPrimType prim);

  void set_raster_discard();
  uint32_t unpack_row_length = 0;

  void set_vertex_input(VKVertArray::VKVertexInput &input);

 private:
  void set_write_mask(eGPUWriteMask value);
  void set_depth_test(eGPUDepthTest value);
  void set_stencil_test(eGPUStencilTest test, eGPUStencilOp operation);
  void set_stencil_mask(eGPUStencilTest test, const GPUStateMutable state);
  void set_clip_distances(int new_dist_len, int old_dist_len);
  void set_logic_op(bool enable);
  void set_facing(bool invert);
  void set_backface_culling(eGPUFaceCullTest test);
  void set_provoking_vert(eGPUProvokingVertex vert);
  void set_shadow_bias(bool enable);
  void set_blend(eGPUBlend value);

  void set_state(const GPUState &state);
  void set_mutable_state(const GPUStateMutable & /*state*/){};
  void set_mutable_state(VkCommandBuffer commandBuffer, const GPUStateMutable &state);

  void texture_bind_apply();
  uint64_t bound_texture_slots();
  void image_bind_apply();
  uint8_t bound_image_slots();

  MEM_CXX_CLASS_ALLOC_FUNCS("VKStateManager")
};

}  // namespace gpu
}  // namespace blender
