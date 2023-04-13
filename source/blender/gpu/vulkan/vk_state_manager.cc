/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.h"

#include "BLI_math_base.h"
#include "BLI_math_bits.h"

#include "GPU_capabilities.h"

#include "vk_backend.hh"
#include "vk_framebuffer.hh"
#include "vk_texture.hh"
#include "vk_context.hh"
#include "vk_pipeline.hh"
#include "vk_shader.hh"
#include "vk_state_manager.hh"

namespace blender::gpu {

static std::vector<VkDynamicState> dynamicStateEnables;

void VKStateManager::set_vertex_input(VKVertArray::VKVertexInput &input)
{
  vk_state_.pipelineCI_.pVertexInputState = &input.get();
};

PipelineStateCreateInfoVk &VKStateManager::get_pipeline_state()
{
  return vk_state_;
};

/* -------------------------------------------------------------------- */
/** \name VKStateManager
 * \{ */

VKStateManager::VKStateManager(VKContext *_ctx) : ctx_(_ctx)
{

  texture_unbind_all();
  unpack_row_length = 0;

  vk_state_.pipelineCI_.flags = 0;
  vk_state_.pipelineCI_.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  vk_state_.pipelineCI_.pNext = VK_NULL_HANDLE;

  dynamicStateEnables.clear();
  dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);

  /* Set other states that never change. */
  /* https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_non_seamless_cube_map.html
   */
  /* glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS); --> device
   * extension(VK_EXT_NON_SEAMLESS_CUBE_MAP_EXTENSION_NAME) */

  /* PipelineState&& RenderPass Resolve SubPass */
  /* #glEnable(GL_MULTISAMPLE); */
  auto &ms = vk_state_.multisample;
  {
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.pNext = NULL;
    ms.flags = 0;
    ms.pSampleMask = NULL;
    ms.rasterizationSamples = (VkSampleCountFlagBits)1;
    ms.sampleShadingEnable = VK_FALSE;
    ms.alphaToCoverageEnable = VK_FALSE;
    ms.alphaToOneEnable = VK_FALSE;
    ms.minSampleShading = 0.0;
  }

  ///
  ///  https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_legacy_dithering.html
  ///  glDisable(GL_DITHER); --> device
  ///  extension(VK_EXT_LEGACY_DITHERING_EXTENSION_NAME)
  /// Issue 1) In OpenGL, the dither state can change dynamically. Should this
  /// extension add a pipeline state for dither?
  /// glDisable(GL_DITHER);

  /// This should either be the image view info or the copy or
  /// blit command info.Functions incompatible with vulkan.
  /// glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  /// glPixelStorei(GL_PACK_ALIGNMENT, 1);
  /// glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  ///
  /// The Vulkan spec states: If the pipeline is being created with
  /// pre-rasterization shader state, and the wideLines feature is not enabled,
  /// and no element of the pDynamicStates member of pDynamicState is
  /// VK_DYNAMIC_STATE_LINE_WIDTH, the lineWidth member of pRasterizationState
  /// must be 1.0
  /// (https://vulkan.lunarg.com/doc/view/1.3.231.1/windows/1.3-extensions/vkspec.html#VUID-VkGraphicsPipelineCreateInfo-pDynamicStates-00749)
  auto &rast = vk_state_.rasterization;
  rast.lineWidth = 1.f;
  /// <summary>
  /// This preference may be enabled by default or this extension is sufficient
  /// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_primitive_topology_list_restart.html
  ///  glEnable(GL_PRIMITIVE_RESTART); --> device
  ///  extension(VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME)
  ///  VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE_EXT  Promotion to Vulkan 1.3
  /// </summary>
  auto &ia = vk_state_.inputassembly;
  ia.primitiveRestartEnable = VK_TRUE;  /// static state
  /// <summary>
  /// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPipelineInputAssemblyStateCreateInfo.html
  /// </summary>
  /// assigned by default.  glPrimitiveRestartIndex((GLuint)0xFFFFFFFF);
  /* TODO: Should become default. But needs at least GL 4.3
  if (VKContext::fixed_restart_index_support) {
    // Takes precedence over #GL_PRIMITIVE_RESTART.
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
  }
* /

  /* Limits. */
  /// glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, line_width_range_);

  memcpy(line_width_range_, _ctx->physical_device_limits_get().lineWidthRange, sizeof(float) * 2);

  /* Force update using default state. */
  current_ = ~state;
  current_mutable_ = ~mutable_state;
  set_state(state);

  /// <summary>
  /// viewport scissor dynamic default.
  /// </summary>

  auto &vp = vk_state_.viewport;
  vp.pNext = NULL;
  vp.flags = 0;
  vp.pScissors = NULL;
  vp.pViewports = NULL;
  vp.viewportCount = 1;
  vp.scissorCount = 1;

  auto &dynamic = vk_state_.dynamic;
  dynamic.flags = 0;

  dynamic.pDynamicStates = dynamicStateEnables.data();
  dynamic.dynamicStateCount = (uint32_t)dynamicStateEnables.size();

  vk_state_.depthstencil.minDepthBounds = 0.f;
  vk_state_.depthstencil.maxDepthBounds = 1.f;

  /// set_mutable_state(mutable_state);
}

VKStateManager::~VKStateManager(){

};

VkGraphicsPipelineCreateInfo &VKStateManager::get_pipeline_create_info(VkRenderPass vkRP,
                                                                       VkPipelineLayout &layout)
{
  apply_state();

  /// If topology is VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
  /// VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
  /// VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
  /// VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY or
  /// VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, primitiveRestartEnable must be VK_FALSE
  /// (https://vulkan.lunarg.com/doc/view/1.3.231.1/windows/1.3-extensions/vkspec.html#VUID-VkPipelineInputAssemblyStateCreateInfo-topology-00428)

  if (vk_state_.inputassembly.primitiveRestartEnable == VK_TRUE) {
    bool conflict = false;
#define EQ_TPL(name) \
  if (name == vk_state_.inputassembly.topology) \
    conflict = true;
    EQ_TPL(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
    EQ_TPL(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
    EQ_TPL(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
    EQ_TPL(VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY)
    EQ_TPL(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY)
    EQ_TPL(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
#undef EQ_TPL
    if (conflict)
      vk_state_.inputassembly.primitiveRestartEnable = VK_FALSE;
  }

  auto &pipelineCreateInfo = vk_state_.pipelineCI_;
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
  pipelineCreateInfo.renderPass = vkRP;
  pipelineCreateInfo.layout = layout;
  pipelineCreateInfo.pViewportState = &vk_state_.viewport;
  pipelineCreateInfo.pDynamicState = &vk_state_.dynamic;
  pipelineCreateInfo.pColorBlendState = &vk_state_.colorblend;
  pipelineCreateInfo.pDepthStencilState = &vk_state_.depthstencil;
  pipelineCreateInfo.pInputAssemblyState = &vk_state_.inputassembly;
  pipelineCreateInfo.pRasterizationState = &vk_state_.rasterization;
  pipelineCreateInfo.pMultisampleState = &vk_state_.multisample;
  return pipelineCreateInfo;
}

void VKStateManager::apply_state()
{
  VKContext &context = *VKContext::get();
  VKShader &shader = unwrap(*context.shader);
  VKPipeline &pipeline = shader.pipeline_get();
  pipeline.state_manager_get().set_state(state, mutable_state);
}

void VKStateManager::force_state()
{
  VKContext &context = *VKContext::get();
  VKShader &shader = unwrap(*context.shader);
  VKPipeline &pipeline = shader.pipeline_get();
  pipeline.state_manager_get().force_state(state, mutable_state);
}

void VKStateManager::set_prim_type(const GPUPrimType prim)
{
  auto &ia = vk_state_.inputassembly;
  ia.topology = to_vk(prim);
}

void VKStateManager::set_raster_discard()
{
  auto &raster = vk_state_.rasterization;
  raster.rasterizerDiscardEnable = VK_TRUE;
};

void VKStateManager::set_state(const GPUState &state)
{
  GPUState changed = state ^ current_;

  if (changed.blend != 0) {
    set_blend((eGPUBlend)state.blend);
  }
  if (changed.write_mask != 0) {
    set_write_mask((eGPUWriteMask)state.write_mask);
  }
  if (changed.depth_test != 0) {
    set_depth_test((eGPUDepthTest)state.depth_test);
  }
  if (changed.stencil_test != 0 || changed.stencil_op != 0) {
    set_stencil_test((eGPUStencilTest)state.stencil_test, (eGPUStencilOp)state.stencil_op);
    set_stencil_mask((eGPUStencilTest)state.stencil_test, mutable_state);
  }
  if (changed.clip_distances != 0) {
    set_clip_distances(state.clip_distances, current_.clip_distances);
  }
  if (changed.culling_test != 0) {
    set_backface_culling((eGPUFaceCullTest)state.culling_test);
  }
  if (changed.logic_op_xor != 0) {
    set_logic_op(state.logic_op_xor);
  }
  if (changed.invert_facing != 0) {
    set_facing(state.invert_facing);
  }
  if (changed.provoking_vert != 0) {
    set_provoking_vert((eGPUProvokingVertex)state.provoking_vert);
  }
  if (changed.shadow_bias != 0) {
    set_shadow_bias(state.shadow_bias);
  }
  /* TODO: remove. */
  /*
   * no found compatible logic. An alternative control method is multisampling.
   */
  if (changed.polygon_smooth) {
    if (state.polygon_smooth) {
      /// glEnable(GL_POLYGON_SMOOTH);
    }
    else {
      /// glDisable(GL_POLYGON_SMOOTH);
    }
  }

  if (changed.line_smooth) {
    if (state.line_smooth) {
      vk_state_.rasterline.lineRasterizationMode =
          VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT;
      /// glEnable(GL_LINE_SMOOTH);
    }
    else {
      vk_state_.rasterline.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT;
      /// glDisable(GL_LINE_SMOOTH);
    }
  }

  current_ = state;
}

void VKStateManager::set_mutable_state(VkCommandBuffer commandBuffer, const GPUStateMutable &state)
{
  dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT,
                         VK_DYNAMIC_STATE_SCISSOR,
                         VK_DYNAMIC_STATE_LINE_WIDTH,
                         VK_DYNAMIC_STATE_DEPTH_BOUNDS,
                         VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
                         VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
                         VK_DYNAMIC_STATE_STENCIL_REFERENCE,
                         VK_DYNAMIC_STATE_STENCIL_OP_EXT};

  auto &dynamic = vk_state_.dynamic;
  dynamic.flags = 0;

  dynamic.pDynamicStates = dynamicStateEnables.data();
  dynamic.dynamicStateCount = (uint32_t)dynamicStateEnables.size();

  GPUStateMutable changed = state ^ current_mutable_;

  if (float_as_uint(changed.line_width) != 0) {
    /* TODO: remove, should use wide line shader. */
    vkCmdSetLineWidth(commandBuffer,
                      clamp_f(state.line_width, line_width_range_[0], line_width_range_[1]));
    /// glLineWidth(clamp_f(state.line_width, line_width_range_[0],
    /// line_width_range_[1]));
  }

  if (float_as_uint(changed.depth_range[0]) != 0 || float_as_uint(changed.depth_range[1]) != 0) {
    /* TODO: remove, should modify the projection matrix instead. */
    vkCmdSetDepthBounds(commandBuffer, UNPACK2(state.depth_range));
    /// glDepthRange(UNPACK2(state.depth_range));
  }

  if (changed.stencil_compare_mask != 0 || changed.stencil_reference != 0 ||
      changed.stencil_write_mask != 0) {
    /// <summary>
    /// dynamic operation enabled  >= vulkan version1.3
    /// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdSetStencilOp.html
    /// </summary>
    BLI_assert(vk_state_._stenciltest == (eGPUStencilTest)current_.stencil_test);
    /// set_stencil_mask((eGPUStencilTest)current_.stencil_test, state);
    vkCmdSetStencilCompareMask(commandBuffer,
                               VK_STENCIL_FRONT_AND_BACK,
                               static_cast<uint32_t>(state.stencil_compare_mask));
    vkCmdSetStencilReference(
        commandBuffer, VK_STENCIL_FRONT_AND_BACK, static_cast<uint32_t>(state.stencil_reference));
    vkCmdSetStencilWriteMask(
        commandBuffer, VK_STENCIL_FRONT_AND_BACK, static_cast<uint32_t>(state.stencil_write_mask));
  }

  current_mutable_ = state;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name State set functions
 * \{ */

void VKStateManager::set_write_mask(const eGPUWriteMask value)
{

  VkPipelineDepthStencilStateCreateInfo &ds = vk_state_.depthstencil;
  ds.depthWriteEnable = ((value & GPU_WRITE_DEPTH) != 0) ? VK_TRUE : VK_FALSE;
  /// glDepthMask((value & GPU_WRITE_DEPTH) != 0);

  VkPipelineColorBlendAttachmentState &att_state = vk_state_.colorblend_attachment.last();
  att_state.colorWriteMask = 0x0;

  if ((value & GPU_WRITE_RED) != 0)
    att_state.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
  if ((value & GPU_WRITE_GREEN) != 0)
    att_state.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
  if ((value & GPU_WRITE_BLUE) != 0)
    att_state.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
  if ((value & GPU_WRITE_ALPHA) != 0)
    att_state.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;

  /*
  glColorMask((value & GPU_WRITE_RED) != 0,
              (value & GPU_WRITE_GREEN) != 0,
              (value & GPU_WRITE_BLUE) != 0,
              (value & GPU_WRITE_ALPHA) != 0);
*/

  VkPipelineRasterizationStateCreateInfo &rast = vk_state_.rasterization;

  if (value == GPU_WRITE_NONE) {
    rast.rasterizerDiscardEnable = VK_TRUE;
    // glEnable(GL_RASTERIZER_DISCARD);
  }
  else {
    rast.rasterizerDiscardEnable = VK_FALSE;
    // glDisable(GL_RASTERIZER_DISCARD);
  }
}

void VKStateManager::set_depth_test(const eGPUDepthTest value)
{
  VkPipelineDepthStencilStateCreateInfo &ds = vk_state_.depthstencil;

  switch (value) {
    case GPU_DEPTH_LESS:
      ds.depthCompareOp = VK_COMPARE_OP_LESS;
      break;
    case GPU_DEPTH_LESS_EQUAL:
      ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
      break;
    case GPU_DEPTH_EQUAL:
      ds.depthCompareOp = VK_COMPARE_OP_EQUAL;
      break;
    case GPU_DEPTH_GREATER:
      ds.depthCompareOp = VK_COMPARE_OP_GREATER;
      break;
    case GPU_DEPTH_GREATER_EQUAL:
      ds.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
      break;
    case GPU_DEPTH_ALWAYS:
    default:
      ds.depthCompareOp = VK_COMPARE_OP_ALWAYS;
      break;
  }

  if (value != GPU_DEPTH_NONE) {
    ds.depthTestEnable = VK_TRUE;
    /// glEnable(GL_DEPTH_TEST);
    /// glDepthFunc(func);
  }
  else {
    ds.depthTestEnable = VK_FALSE;
    /// glDisable(GL_DEPTH_TEST);
    ds.depthCompareOp = VK_COMPARE_OP_NEVER;
  }

  ds.depthBoundsTestEnable = VK_TRUE;
}

void VKStateManager::set_stencil_test(const eGPUStencilTest test, const eGPUStencilOp operation)
{
  VkPipelineDepthStencilStateCreateInfo &ds = vk_state_.depthstencil;

  ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
  ds.front.compareMask = 0;
  ds.front.reference = 0;

  switch (operation) {
    case GPU_STENCIL_OP_REPLACE:
      ds.front.failOp = VK_STENCIL_OP_KEEP;
      ds.front.passOp = VK_STENCIL_OP_KEEP;
      ds.front.depthFailOp = VK_STENCIL_OP_REPLACE;
      ds.back = ds.front;
      /// glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
      break;
    case GPU_STENCIL_OP_COUNT_DEPTH_PASS:
      ds.front.failOp = VK_STENCIL_OP_KEEP;
      ds.front.passOp = VK_STENCIL_OP_KEEP;
      ds.front.depthFailOp = VK_STENCIL_OP_DECREMENT_AND_WRAP;
      ds.back = ds.front;
      ds.back.depthFailOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
      /// glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
      /// glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
      break;
    case GPU_STENCIL_OP_COUNT_DEPTH_FAIL:
      ds.front.failOp = VK_STENCIL_OP_KEEP;
      ds.front.passOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
      ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
      ds.back = ds.front;
      ds.back.depthFailOp = VK_STENCIL_OP_DECREMENT_AND_WRAP;
      /// glStencilOpSeparate(GL_BACK, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
      /// glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
      break;
    case GPU_STENCIL_OP_NONE:
    default:
      ds.front.failOp = VK_STENCIL_OP_KEEP;
      ds.front.passOp = VK_STENCIL_OP_KEEP;
      ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
      ds.back = ds.front;
     
  }

  if (test != GPU_STENCIL_NONE) {
    ds.stencilTestEnable = VK_TRUE;
    /// glEnable(GL_STENCIL_TEST);
  }
  else {
    ds.stencilTestEnable = VK_FALSE;
    /// glDisable(GL_STENCIL_TEST);
  }
}

void VKStateManager::set_stencil_mask(const eGPUStencilTest test, const GPUStateMutable state)
{
  /// dynamically set as necessary.
  vk_state_._stenciltest = test;
  VkPipelineDepthStencilStateCreateInfo &ds = vk_state_.depthstencil;
  ds.front.writeMask = static_cast<uint32_t>(state.stencil_write_mask);
  ds.front.reference = static_cast<uint32_t>(state.stencil_reference);

  ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
  ds.front.compareMask = static_cast<uint32_t>(state.stencil_compare_mask);

  switch (test) {
    case GPU_STENCIL_NEQUAL:
      ds.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;
      break;
    case GPU_STENCIL_EQUAL:
      ds.front.compareOp = VK_COMPARE_OP_EQUAL;
      break;
    case GPU_STENCIL_ALWAYS:
      ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
      break;
    case GPU_STENCIL_NONE:
    default:
      ds.front.compareMask = 0x00;
      /// glStencilMask(0x00);
      ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
      /// glStencilFunc(GL_ALWAYS, 0x00, 0x00);
      return;
  }

  /// separate operation as necessary.
  ds.back = ds.front;

  /// glStencilMask(state.stencil_write_mask);
  /// glStencilFunc(func, state.stencil_reference, state.stencil_compare_mask);
  ///
}

void VKStateManager::set_clip_distances(const int /*new_dist_len*/, const int /*old_dist_len*/)
{

  // BLI_assert(vulkan::getProperties().limits.maxClipDistances > 0);
  /*
  * TODO compability.
  for (int i = 0; i < new_dist_len; i++) {
    glEnable(GL_CLIP_DISTANCE0 + i);
  }
  for (int i = new_dist_len; i < old_dist_len; i++) {
    glDisable(GL_CLIP_DISTANCE0 + i);
  }
  */
}

void VKStateManager::set_logic_op(const bool enable)
{
  VkPipelineColorBlendStateCreateInfo &cb = vk_state_.colorblend;

  if (enable) {
    cb.logicOpEnable = VK_TRUE;
    /// glEnable(GL_COLOR_LOGIC_OP);
    cb.logicOp = VK_LOGIC_OP_XOR;
    /// glLogicOp(GL_XOR);
  }
  else {
    cb.logicOpEnable = VK_FALSE;
    /// glDisable(GL_COLOR_LOGIC_OP);
  }
}

void VKStateManager::set_facing(const bool invert)
{
  auto &rast = vk_state_.rasterization;
  // rast.frontFace = (invert) ? VK_FRONT_FACE_CLOCKWISE :
  // VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rast.frontFace = (invert) ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
  /// glFrontFace((invert) ? GL_CW : GL_CCW);
}

void VKStateManager::set_backface_culling(const eGPUFaceCullTest test)
{
  auto &rast = vk_state_.rasterization;
  if (test != GPU_CULL_NONE) {
    rast.cullMode = (test == GPU_CULL_FRONT) ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
    /// glEnable(GL_CULL_FACE);
    /// glCullFace((test == GPU_CULL_FRONT) ? GL_FRONT : GL_BACK);
  }
  else {
    /// glDisable(GL_CULL_FACE);
    rast.cullMode = VK_CULL_MODE_NONE;
  }
}

void VKStateManager::set_provoking_vert(const eGPUProvokingVertex vert)
{
  /// need:: extension VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME
  /// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPipelineRasterizationProvokingVertexStateCreateInfoEXT.html
  auto &rs = vk_state_.rasterization;
  rs.pNext = VK_NULL_HANDLE;  //&vk_state_.provokingvertex;

  vk_state_.provokingvertex.provokingVertexMode = (vert == GPU_VERTEX_FIRST) ?
                                                      VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT :
                                                      VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT;

  /// glProvokingVertex(value);
}

void VKStateManager::set_shadow_bias(const bool enable)
{
  auto &rs = vk_state_.rasterization;
  /// <summary>
  /// TODO GL_POLYGON_OFFSET_FILL probably not needed.Or need alternative logic.
  /// </summary>
  if (enable) {
    rs.depthBiasEnable = VK_TRUE;

    /// glEnable(GL_POLYGON_OFFSET_FILL);
    /// glEnable(GL_POLYGON_OFFSET_LINE);
    /* 2.0 Seems to be the lowest possible slope bias that works in every case.
     */
    /// glPolygonOffset(2.0f, 1.0f);
    rs.depthBiasSlopeFactor = 2.f;
    rs.depthBiasConstantFactor = 1.f;
    rs.depthBiasClamp = 0.f;
  }
  else {
    rs.depthBiasEnable = VK_FALSE;
    /// glDisable(GL_POLYGON_OFFSET_FILL);
    /// glDisable(GL_POLYGON_OFFSET_LINE);
  }
}

void VKStateManager::set_blend(const eGPUBlend value)
{
  /**
   * Factors to the equation.
   * SRC is fragment shader output.
   * DST is frame-buffer color.
   * final.rgb = SRC.rgb * src_rgb + DST.rgb * dst_rgb;
   * final.a = SRC.a * src_alpha + DST.a * dst_alpha;
   */
  // GLenum src_rgb, src_alpha, dst_rgb, dst_alpha;

  VkPipelineColorBlendStateCreateInfo &cb = vk_state_.colorblend;
  cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  cb.pNext = NULL;
  cb.flags = 0;

  VkPipelineColorBlendAttachmentState &att_state = vk_state_.colorblend_attachment.last();

  att_state.blendEnable = VK_TRUE;
  att_state.alphaBlendOp = VK_BLEND_OP_ADD;
  att_state.colorBlendOp = VK_BLEND_OP_ADD;
  att_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
  att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  cb.attachmentCount = 1;
  cb.pAttachments = &att_state;
  cb.logicOpEnable = VK_FALSE;
  cb.logicOp = VK_LOGIC_OP_NO_OP;
  cb.blendConstants[0] = 1.0f;
  cb.blendConstants[1] = 1.0f;
  cb.blendConstants[2] = 1.0f;
  cb.blendConstants[3] = 1.0f;

  switch (value) {
    default:
    case GPU_BLEND_ALPHA: {

      att_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;  /// src_rgb = GL_SRC_ALPHA;
      att_state.dstColorBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;              // dst_rgb =
                                                            // GL_ONE_MINUS_SRC_ALPHA;
      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  /// src_alpha = GL_ONE;
      att_state.dstAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  /// dst_alpha =
                                                /// GL_ONE_MINUS_SRC_ALPHA;
      break;
    }
    case GPU_BLEND_ALPHA_PREMULT: {
      att_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;  ///    src_rgb = GL_ONE;
      att_state.dstColorBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  ///   dst_rgb =
                                                ///   GL_ONE_MINUS_SRC_ALPHA;

      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  ///  src_alpha = GL_ONE;
      att_state.dstAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  /// dst_alpha =
                                                /// GL_ONE_MINUS_SRC_ALPHA;

      break;
    }
    case GPU_BLEND_ADDITIVE: {
      /* Do not let alpha accumulate but pre-multiply the source RGB by it. */
      att_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;  ///    src_rgb =GL_SRC_ALPHA;
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;        ///   dst_rgb =GL_ONE;

      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  ///  src_alpha =  GL_ZERO;
      att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;   /// dst_alpha = GL_ONE;

      break;
    }
    case GPU_BLEND_SUBTRACT:
    case GPU_BLEND_ADDITIVE_PREMULT: {
      /* Let alpha accumulate. */
      att_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;  ///     src_rgb = GL_ONE;
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;  ///   dst_rgb =GL_ONE;

      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  ///  src_alpha = GL_ONE;
      att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  /// dst_alpha = GL_ONE;

      break;
    }
    case GPU_BLEND_MULTIPLY: {
      att_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;  ///     src_rgb =GL_DST_COLOR;
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;       ///   dst_rgb =GL_ZERO;

      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;  ///  src_alpha =GL_DST_ALPHA;
      att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;       /// dst_alpha = GL_ZERO;

      break;
    }
    case GPU_BLEND_INVERT: {
      att_state.srcColorBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;               ///    src_rgb =
                                                             ///    GL_ONE_MINUS_DST_COLOR;
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;  /// dst_rgb = GL_ZERO;

      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  ///   src_alpha = GL_ZERO;
      att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;   /// dst_alpha = = GL_ONE;

      break;
    }
    case GPU_BLEND_OIT: {
      att_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;  ///    src_rgb = GL_ONE;
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;  /// dst_rgb = GL_ONE;

      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  ///   src_alpha =  GL_ZERO;
      att_state.dstAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  /// dst_alpha =
                                                /// =GL_ONE_MINUS_SRC_ALPHA;

      break;
    }
    case GPU_BLEND_BACKGROUND: {
      att_state.srcColorBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;                    ///    src_rgb =
                                                                  ///    GL_ONE_MINUS_DST_ALPHA
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;  /// dst_rgb = GL_SRC_ALPHA;

      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  ///   src_alpha =  GL_ZERO;
      att_state.dstAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  /// dst_alpha =
                                                /// =GL_ONE_MINUS_SRC_ALPHA;

      break;
    }
    case GPU_BLEND_ALPHA_UNDER_PREMUL: {
      att_state.srcColorBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;              ///    src_rgb =
                                                            ///    GL_ONE_MINUS_DST_ALPHA
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;  /// dst_rgb = GL_ONE

      att_state.srcAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;              ///   src_alpha =
                                                            ///   GL_ONE_MINUS_DST_ALPHA
      att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  /// dst_alpha = =GL_ONE;

      break;
    }
    case GPU_BLEND_CUSTOM: {
      att_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;         ///    src_rgb = GL_ONE
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC1_COLOR;  /// dst_rgb = GL_SRC1_COLOR

      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;         ///   src_alpha =  GL_ONE
      att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA;  /// dst_alpha = =GL_SRC1_ALPHA;

      break;
    }
  }

  if (value == GPU_BLEND_SUBTRACT) {
    att_state.alphaBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
    att_state.colorBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;

    /// glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
  }
  else {
    att_state.alphaBlendOp = VK_BLEND_OP_ADD;
    att_state.colorBlendOp = VK_BLEND_OP_ADD;
    /// glBlendEquation(GL_FUNC_ADD);
  }

  /* Always set the blend function. This avoid a rendering error when blending
   * is disabled but GPU_BLEND_CUSTOM was used just before and the frame-buffer
   * is using more than 1 color target.
   */
  /// glBlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
  if (value != GPU_BLEND_NONE) {
    /// glEnable(GL_BLEND);
    att_state.blendEnable = VK_TRUE;
  }
  else {
    /// glDisable(GL_BLEND);
    att_state.blendEnable = VK_FALSE;
  }
}

void VKStateManager::set_color_blend_from_fb(VKFrameBuffer *fb)
{

  VkPipelineColorBlendStateCreateInfo &cb = vk_state_.colorblend;
  cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  cb.pNext = NULL;
  cb.flags = 0;

  VkPipelineColorBlendAttachmentState att_state_last = vk_state_.colorblend_attachment.last();

  auto size_atta = vk_state_.colorblend_attachment.size();
  cb.attachmentCount = 0;

  for (int i = 0; i < 8; i++) {

    VkFormat tex_format  = fb->is_color(i);
    if (tex_format == VK_FORMAT_UNDEFINED) {
      continue;
    }

    cb.attachmentCount++;
    VkPipelineColorBlendAttachmentState att_state = att_state_last;
    if (cb.attachmentCount > (size_atta)) {
      vk_state_.colorblend_attachment.append(att_state_last);
      size_atta++;
    }
    else {
      vk_state_.colorblend_attachment[cb.attachmentCount - 1] = att_state_last;
    }
    /*
     * vkCreateGraphicsPipelines():
     * pipeline.pColorBlendState.pAttachments[2].blendEnable is VK_TRUE but
     * format VK_FORMAT_R16_UINT of the corresponding attachment description
     * (subpass 0, attachment 2) does not support
     * VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT. The Vulkan spec states: If
     * renderPass is not VK_NULL_HANDLE, and the pipeline is being created with
     * fragment output interface state, then for each color attachment in the
     * subpass, if the potential format features of the format of the
     * corresponding attachment description do not contain
     * VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT, then the blendEnable member
     * of the corresponding element of the pAttachments member of
     * pColorBlendState must be VK_FALSE
     * (https://vulkan.lunarg.com/doc/view/1.3.231.1/windows/1.3-extensions/vkspec.html#VUID-VkGraphicsPipelineCreateInfo-renderPass-06041)
     */
    switch (tex_format) {
      case VK_FORMAT_R16_UINT:
        vk_state_.colorblend_attachment.last().blendEnable = VK_FALSE;
        break;
      default:
        break;
    }
  }

  cb.pAttachments = vk_state_.colorblend_attachment.data();
  cb.logicOpEnable = VK_FALSE;
  cb.logicOp = VK_LOGIC_OP_NO_OP;
};
/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture State Management
 * \{ */


void VKStateManager::texture_bind(Texture *tex_, eGPUSamplerState sampler_type,
                                  int binding) {
  BLI_assert(binding < GPU_max_textures());
  VKTexture *tex = static_cast<VKTexture *>(tex_);
  if (G.debug & G_DEBUG_GPU) {
    //tex->check_feedback_loop();
  }
  //attachment2sampler(tex);

  tex->texture_bind(binding,sampler_type);


  dirty_texture_binds_ |= 1ULL << binding;
}


void VKStateManager::texture_bind_temp(VKTexture * /*tex*/)
{
  /*TODO :: Set in descriptorset. */

  /*TODO :: Will update the descriptorset.*/
  dirty_texture_binds_ |= 1ULL;
}

void VKStateManager::texture_unbind(Texture * tex_)
{

}

void VKStateManager::texture_unbind_all()
{

  this->texture_bind_apply();
}

void VKStateManager::texture_bind_apply()
{
  if (dirty_texture_binds_ == 0) {
    return;
  }

  // uint64_t dirty_bind = dirty_texture_binds_;

  dirty_texture_binds_ = 0;
  /*
  int first = bitscan_forward_uint64(dirty_bind);
  int last = 64 - bitscan_reverse_uint64(dirty_bind);
  int count = last - first;
  */

}

void VKStateManager::texture_unpack_row_length_set(uint len)
{
  /// glPixelStorei(GL_UNPACK_ROW_LENGTH, len);
  /* https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkBufferImageCopy.html
   */
  unpack_row_length = len;
}

uint64_t VKStateManager::bound_texture_slots()
{
  uint64_t bound_slots = 0;

  return bound_slots;
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Binding (from image load store)
 * \{ */

uint8_t VKStateManager::bound_image_slots()
{
  uint8_t bound_slots = 0;

  return bound_slots;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Memory barrier
 * \{ */

void VKStateManager::issue_barrier(eGPUBarrier /*barrier_bits*/)
{
  VKContext &context = *VKContext::get();
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  /* TODO: Pipeline barriers should be added. We might be able to extract it from
   * the actual pipeline, later on, but for now we submit the work as barrier. */
  command_buffer.submit();
}

/** \} */

}  // namespace blender::gpu

namespace blender::gpu {

void VKStateManager::image_bind(Texture *tex, int binding)
{
  VKTexture *texture = unwrap(tex);
  texture->image_bind(binding);
}

void VKStateManager::image_unbind(Texture * /*tex*/)
{
}

void VKStateManager::image_unbind_all()
{
}

void VKStateManager::image_bind_apply()
{
  if (dirty_image_binds_ == 0) {
    return;
  }
  // uint32_t dirty_bind = dirty_image_binds_;
  dirty_image_binds_ = 0;

  /*
  int first = bitscan_forward_uint(dirty_bind);
  int last = 32 - bitscan_reverse_uint(dirty_bind);
  int count = last - first;

  if (GLContext::multi_bind_support) {
    glBindImageTextures(first, count, images_ + first);
  }
  else {
    for (int unit = first; unit < last; unit++) {
      if ((dirty_bind >> unit) & 1UL) {
        glBindImageTexture(unit, images_[unit], 0, GL_TRUE, 0, GL_READ_WRITE,
  formats_[unit]);
      }
    }
  }
  */
}

}  // namespace blender::gpu
