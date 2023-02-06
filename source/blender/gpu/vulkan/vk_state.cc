/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. */

/** \file
 * \ingroup gpu
 */
#if 1

#include "BKE_global.h"

#include "BLI_math_base.h"
#include "BLI_math_bits.h"

#include "GPU_capabilities.h"

#include "vk_context.hh"

#include "vk_backend.hh"

#include "vk_framebuffer.hh"
#include "vk_texture.hh"

#include "vk_state.hh"
#include "vk_layout.hh"

namespace blender::gpu {

static  PipelineStateCreateInfoVk  current_pipeline_ = PipelineStateCreateInfoVk();
static VKGraphicsPipelineStateDescriptor current_pipeline_desc_;
static std::vector<VkDynamicState> dynamicStateEnables;

VKGraphicsPipelineStateDescriptor &VKStateManager::getPipelineStateDesc()
{
  return current_pipeline_desc_;
};
PipelineStateCreateInfoVk &VKStateManager::getPipelineStateCI()
{
  return current_pipeline_;
};



  void VKStateManager::cmd_dynamic_state(VkCommandBuffer& cmd)
{
  auto &state = current_pipeline_;
  vkCmdSetViewport(cmd, 0, 1, &state.viewport_cache);
  vkCmdSetScissor(cmd, 0, 1, &state.scissor_cache);

  }

/* -------------------------------------------------------------------- */
/** \name VKStateManager
 * \{ */

VKStateManager::VKStateManager(VKContext *_ctx) : ctx_(_ctx)
{
  
  texture_unbind_all();


  dynamicStateEnables.clear();
  dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);

  /* Set other states that never change. */
  /* https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_non_seamless_cube_map.html */
  /* glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS); --> device extension(VK_EXT_NON_SEAMLESS_CUBE_MAP_EXTENSION_NAME) */

  /* PipelineState&& RenderPass Resolve SubPass */
  /* #glEnable(GL_MULTISAMPLE); */
  auto &ms = current_pipeline_.multisample;
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
  ///  glDisable(GL_DITHER); --> device extension(VK_EXT_LEGACY_DITHERING_EXTENSION_NAME)
  /// Issue 1) In OpenGL, the dither state can change dynamically. Should this extension add a pipeline state for dither? 
  ///glDisable(GL_DITHER);

  ///This should either be the image view info or the copy or
  /// blit command info.Functions incompatible with vulkan.
  /// glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  /// glPixelStorei(GL_PACK_ALIGNMENT, 1);
  /// glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  ///
  /// The Vulkan spec states: If the pipeline is being created with pre-rasterization shader state, and the wideLines feature is not enabled, and no element of the pDynamicStates member of pDynamicState is VK_DYNAMIC_STATE_LINE_WIDTH,
  /// the lineWidth member of pRasterizationState must be 1.0 (https://vulkan.lunarg.com/doc/view/1.3.231.1/windows/1.3-extensions/vkspec.html#VUID-VkGraphicsPipelineCreateInfo-pDynamicStates-00749)
  auto &rast = current_pipeline_.rasterization;
  rast.lineWidth = 1.;
    /// <summary>
  /// This preference may be enabled by default or this extension is sufficient
  /// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_primitive_topology_list_restart.html
  ///  glEnable(GL_PRIMITIVE_RESTART); --> device
  ///  extension(VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME)
  ///  VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE_EXT  Promotion to Vulkan 1.3
  /// </summary>
  auto &ia = current_pipeline_.inputassembly;
  ia.primitiveRestartEnable = VK_TRUE;   /// static state
  /// <summary>
  /// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPipelineInputAssemblyStateCreateInfo.html
  /// </summary>
  ///assigned by default.  glPrimitiveRestartIndex((GLuint)0xFFFFFFFF);
  /* TODO: Should become default. But needs at least GL 4.3
  if (VKContext::fixed_restart_index_support) {
    // Takes precedence over #GL_PRIMITIVE_RESTART. 
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
  }
* /

  /* Limits. */
  ///glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, line_width_range_);
  memcpy(line_width_range_, vulkan::getProperties().limits.lineWidthRange, sizeof(float) * 2);
 
  /* Force update using default state. */
  current_ = ~state;
  current_mutable_ = ~mutable_state;
  set_state(state);

  /// <summary>
  /// viewport scissor dynamic default.
  /// </summary>

  auto& vp = current_pipeline_.viewport;
  vp.pNext = NULL;
  vp.flags = 0;
  vp.pScissors = NULL;
  vp.pViewports = NULL;
  vp.viewportCount  = 1;
  vp.scissorCount    = 1;

  auto &dynamic = current_pipeline_.dynamic;
  dynamic.flags = 0;

  dynamic.pDynamicStates = dynamicStateEnables.data();
  dynamic.dynamicStateCount = (uint32_t)dynamicStateEnables.size();

  
  ///set_mutable_state(mutable_state);
}

VKStateManager::~VKStateManager()
{

 };

VkGraphicsPipelineCreateInfo VKStateManager::get_pipelinecreateinfo(VkRenderPass vkRP,
                                      VkPipelineLayout &layout
                                     )
{
  apply_state();

  ///If topology is VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY or VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, primitiveRestartEnable must be VK_FALSE (https://vulkan.lunarg.com/doc/view/1.3.231.1/windows/1.3-extensions/vkspec.html#VUID-VkPipelineInputAssemblyStateCreateInfo-topology-00428)

  if (current_pipeline_.inputassembly.primitiveRestartEnable == VK_TRUE) {
    bool conflict = false;
#  define EQ_TPL(name) if (name == current_pipeline_.inputassembly.topology) conflict = true;
    EQ_TPL(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
    EQ_TPL(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
    EQ_TPL(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
    EQ_TPL(VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY)
    EQ_TPL(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY)
    EQ_TPL(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
#  undef EQ_TPL
    if (conflict)
      current_pipeline_.inputassembly.primitiveRestartEnable = VK_FALSE;

  } 

  auto &pipelineCreateInfo = current_pipeline_desc_.pipelineCI;
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
  pipelineCreateInfo.renderPass = vkRP;
  pipelineCreateInfo.layout = layout;
  pipelineCreateInfo.pViewportState = &current_pipeline_.viewport;
  pipelineCreateInfo.pDynamicState = &current_pipeline_.dynamic;
  pipelineCreateInfo.pColorBlendState = &current_pipeline_.colorblend;
  pipelineCreateInfo.pDepthStencilState = &current_pipeline_.depthstencil;
  pipelineCreateInfo.pInputAssemblyState = &current_pipeline_.inputassembly;
  pipelineCreateInfo.pRasterizationState = &current_pipeline_.rasterization;
  pipelineCreateInfo.pMultisampleState = &current_pipeline_.multisample;
  return pipelineCreateInfo;
 }

void VKStateManager::apply_state()
{
  if (!this->use_bgl) {
    this->set_state(this->state);
   /// this->set_mutable_state(this->mutable_state);
    ///this->texture_bind_apply();
   // this->image_bind_apply();
  }
  /* This is needed by gpu_py_offscreen. */
  active_fb->apply_state();

  
};

void VKStateManager::force_state()
{
  /* Little exception for clip distances since they need to keep the old count correct. */
  uint32_t clip_distances = current_.clip_distances;
  current_ = ~this->state;
  current_.clip_distances = clip_distances;
  current_mutable_ = ~this->mutable_state;
  this->set_state(this->state);
  this->set_mutable_state(this->mutable_state);
};

void VKStateManager::set_prim_type(const GPUPrimType prim)
{
  auto&  ia = current_pipeline_.inputassembly;
  ia.topology = to_vk(prim);
 
}

void VKStateManager::set_raster_discard(){
  auto &raster = current_pipeline_.rasterization;
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
      ///glEnable(GL_POLYGON_SMOOTH);
    }
    else {
      ///glDisable(GL_POLYGON_SMOOTH);
    }
  }


  if (changed.line_smooth) {
    if (state.line_smooth) {
      current_pipeline_.rasterline.lineRasterizationMode =
          VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT; 
      ///glEnable(GL_LINE_SMOOTH);
    }
    else {
      current_pipeline_.rasterline.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT;
      ///glDisable(GL_LINE_SMOOTH);
    }
  }

  current_ = state;
}

void VKStateManager::set_mutable_state(VkCommandBuffer commandBuffer,const GPUStateMutable &state)
{
  dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT,
                                        VK_DYNAMIC_STATE_SCISSOR, 
                                        VK_DYNAMIC_STATE_LINE_WIDTH,
                                        VK_DYNAMIC_STATE_DEPTH_BOUNDS,
                                        VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
                                        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
                                        VK_DYNAMIC_STATE_STENCIL_REFERENCE,
                                        VK_DYNAMIC_STATE_STENCIL_OP_EXT};

  auto &dynamic = current_pipeline_.dynamic;
  dynamic.flags = 0;



  dynamic.pDynamicStates = dynamicStateEnables.data();
  dynamic.dynamicStateCount = (uint32_t)dynamicStateEnables.size();

  
  GPUStateMutable changed = state ^ current_mutable_;



  if (float_as_uint(changed.line_width) != 0) {
    /* TODO: remove, should use wide line shader. */
    vkCmdSetLineWidth(commandBuffer, clamp_f(state.line_width, line_width_range_[0], line_width_range_[1]));
    ///glLineWidth(clamp_f(state.line_width, line_width_range_[0], line_width_range_[1]));
  }

  if (float_as_uint(changed.depth_range[0]) != 0 || float_as_uint(changed.depth_range[1]) != 0) {
    /* TODO: remove, should modify the projection matrix instead. */
    vkCmdSetDepthBounds(commandBuffer, UNPACK2(state.depth_range));
    ///glDepthRange(UNPACK2(state.depth_range));
  }

  if (changed.stencil_compare_mask != 0 || changed.stencil_reference != 0 ||
      changed.stencil_write_mask != 0) {
    /// <summary>
    /// dynamic operation enabled  >= vulkan version1.3  https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdSetStencilOp.html
    /// </summary>
    BLI_assert(current_pipeline_._stenciltest == (eGPUStencilTest)current_.stencil_test);
    ///set_stencil_mask((eGPUStencilTest)current_.stencil_test, state);
    vkCmdSetStencilCompareMask(commandBuffer,VK_STENCIL_FRONT_AND_BACK, static_cast<uint32_t>(state.stencil_compare_mask));
     vkCmdSetStencilReference(commandBuffer,VK_STENCIL_FRONT_AND_BACK,static_cast<uint32_t>(state.stencil_reference));
       vkCmdSetStencilWriteMask(commandBuffer,VK_STENCIL_FRONT_AND_BACK,static_cast<uint32_t>(state.stencil_write_mask));

  }

  current_mutable_ = state;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name State set functions
 * \{ */

void VKStateManager::set_write_mask(const eGPUWriteMask value)
{


  VkPipelineDepthStencilStateCreateInfo &ds = current_pipeline_.depthstencil;
  ds.depthWriteEnable = ((value & GPU_WRITE_DEPTH) != 0) ? VK_TRUE : VK_FALSE;
  ///glDepthMask((value & GPU_WRITE_DEPTH) != 0);


  VkPipelineColorBlendAttachmentState &att_state = current_pipeline_.colorblend_attachment.last();
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

   VkPipelineRasterizationStateCreateInfo &rast = current_pipeline_.rasterization;

  if (value == GPU_WRITE_NONE) {
     rast.rasterizerDiscardEnable = VK_TRUE;
    //glEnable(GL_RASTERIZER_DISCARD);
  }
  else {
    rast.rasterizerDiscardEnable = VK_FALSE;
    //glDisable(GL_RASTERIZER_DISCARD);
  }
}

void VKStateManager::set_depth_test(const eGPUDepthTest value)
{
  VkPipelineDepthStencilStateCreateInfo &ds = current_pipeline_.depthstencil;

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
    ///glEnable(GL_DEPTH_TEST);
   ///glDepthFunc(func);
  }
  else {
    ds.depthTestEnable = VK_FALSE;
    ///glDisable(GL_DEPTH_TEST);
    ds.depthCompareOp = VK_COMPARE_OP_NEVER;
  }

  ds.depthBoundsTestEnable = VK_TRUE;
}

void VKStateManager::set_stencil_test(const eGPUStencilTest test, const eGPUStencilOp operation)
{
  VkPipelineDepthStencilStateCreateInfo &ds = current_pipeline_.depthstencil;


  ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
  ds.front.compareMask = 0;
  ds.front.reference = 0;

  switch (operation) {
    case GPU_STENCIL_OP_REPLACE:
      ds.front.failOp = VK_STENCIL_OP_KEEP;
      ds.front.passOp = VK_STENCIL_OP_KEEP;
      ds.front.depthFailOp = VK_STENCIL_OP_REPLACE;
      ds.back = ds.front;
      ///glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
      break;
    case GPU_STENCIL_OP_COUNT_DEPTH_PASS:
      ds.front.failOp = VK_STENCIL_OP_KEEP;
      ds.front.passOp = VK_STENCIL_OP_KEEP;
      ds.front.depthFailOp = VK_STENCIL_OP_DECREMENT_AND_WRAP;
      ds.back = ds.front;
      ds.back.depthFailOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
      ///glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
      ///glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
      break;
    case GPU_STENCIL_OP_COUNT_DEPTH_FAIL:
      ds.front.failOp = VK_STENCIL_OP_KEEP;
      ds.front.passOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
      ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
      ds.back = ds.front;
      ds.back.depthFailOp = VK_STENCIL_OP_DECREMENT_AND_WRAP;
       /// glStencilOpSeparate(GL_BACK, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
      ///glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
      break;
    case GPU_STENCIL_OP_NONE:
    default:
      ds.front.failOp = VK_STENCIL_OP_KEEP;
      ds.front.passOp = VK_STENCIL_OP_KEEP;
      ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
      ds.back = ds.front;
      glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  }

  if (test != GPU_STENCIL_NONE) {
    ds.stencilTestEnable = VK_TRUE;
    ///glEnable(GL_STENCIL_TEST);
    
  }
  else {
    ds.stencilTestEnable = VK_FALSE;
    ///glDisable(GL_STENCIL_TEST);
  }
}

void VKStateManager::set_stencil_mask(const eGPUStencilTest test, const GPUStateMutable state)
{
  ///dynamically set as necessary.
  current_pipeline_._stenciltest = test;
  VkPipelineDepthStencilStateCreateInfo &ds = current_pipeline_.depthstencil;
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
      ///glStencilMask(0x00);
      ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
      ///glStencilFunc(GL_ALWAYS, 0x00, 0x00);
      return;
  }

  /// separate operation as necessary.
  ds.back = ds.front;

  ///glStencilMask(state.stencil_write_mask);
  ///glStencilFunc(func, state.stencil_reference, state.stencil_compare_mask);
  /// 
}

void VKStateManager::set_clip_distances(const int new_dist_len, const int old_dist_len)
{

  BLI_assert(vulkan::getProperties().limits.maxClipDistances > 0);
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
  VkPipelineColorBlendStateCreateInfo &cb = current_pipeline_.colorblend;

  if (enable) {
    cb.logicOpEnable = VK_TRUE;
    ///glEnable(GL_COLOR_LOGIC_OP);
    cb.logicOp = VK_LOGIC_OP_XOR;
    ///glLogicOp(GL_XOR);
  }
  else {
    cb.logicOpEnable = VK_FALSE;
    ///glDisable(GL_COLOR_LOGIC_OP);
  }
}

void VKStateManager::set_facing(const bool invert)
{
  auto &rast = current_pipeline_.rasterization;
  rast.frontFace = (invert) ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
  ///glFrontFace((invert) ? GL_CW : GL_CCW);
  
}

void VKStateManager::set_backface_culling(const eGPUFaceCullTest test)
{
  auto &rast = current_pipeline_.rasterization;
  if (test != GPU_CULL_NONE) {
    rast.cullMode = (test == GPU_CULL_FRONT) ? VK_CULL_MODE_FRONT_BIT: VK_CULL_MODE_BACK_BIT;
    ///glEnable(GL_CULL_FACE);
    ///glCullFace((test == GPU_CULL_FRONT) ? GL_FRONT : GL_BACK);
  }
  else {
    ///glDisable(GL_CULL_FACE);
    rast.cullMode = VK_CULL_MODE_NONE;
  }
}


void VKStateManager::set_provoking_vert(const eGPUProvokingVertex vert)
{
  ///need:: extension VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME
  /// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPipelineRasterizationProvokingVertexStateCreateInfoEXT.html
  auto &rs = current_pipeline_.rasterization;
  rs.pNext = &current_pipeline_.provokingvertex;

  current_pipeline_.provokingvertex.provokingVertexMode = (vert == GPU_VERTEX_FIRST) ? VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT :
                                   VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT; 
    
  ///glProvokingVertex(value);
}

void VKStateManager::set_shadow_bias(const bool enable)
{
  auto &rs = current_pipeline_.rasterization;
  /// <summary>
  /// TODO GL_POLYGON_OFFSET_FILL probably not needed.Or need alternative logic.
  /// </summary>
  if (enable) {
    rs.depthBiasEnable = VK_TRUE;

    ///glEnable(GL_POLYGON_OFFSET_FILL);
    ///glEnable(GL_POLYGON_OFFSET_LINE);
    /* 2.0 Seems to be the lowest possible slope bias that works in every case. */
    ///glPolygonOffset(2.0f, 1.0f);
    rs.depthBiasSlopeFactor  = 2.f;
    rs.depthBiasConstantFactor =1.f;
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
 
  VkPipelineColorBlendStateCreateInfo& cb = current_pipeline_.colorblend;
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.pNext = NULL;
    cb.flags = 0;

    VkPipelineColorBlendAttachmentState &att_state = current_pipeline_.colorblend_attachment.last();
    
    att_state.blendEnable = VK_TRUE;
    att_state.alphaBlendOp = VK_BLEND_OP_ADD;
    att_state.colorBlendOp = VK_BLEND_OP_ADD;
    att_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    att_state.dstColorBlendFactor =VK_BLEND_FACTOR_ONE; 
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
      
      att_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; ///src_rgb = GL_SRC_ALPHA;
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  // dst_rgb = GL_ONE_MINUS_SRC_ALPHA;
      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  /// src_alpha = GL_ONE;
      att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  /// dst_alpha = GL_ONE_MINUS_SRC_ALPHA;
      break;
    }
    case GPU_BLEND_ALPHA_PREMULT: {
      att_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;  ///    src_rgb = GL_ONE;
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; ///   dst_rgb = GL_ONE_MINUS_SRC_ALPHA;
   
      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  ///  src_alpha = GL_ONE;
      att_state.dstAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  /// dst_alpha = GL_ONE_MINUS_SRC_ALPHA;
 
      break;
    }
    case GPU_BLEND_ADDITIVE: {
      /* Do not let alpha accumulate but pre-multiply the source RGB by it. */
      att_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;  ///    src_rgb =GL_SRC_ALPHA;
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;  ///   dst_rgb =GL_ONE;

      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  ///  src_alpha =  GL_ZERO;
      att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  /// dst_alpha = GL_ONE;
     
      break;
    }
    case GPU_BLEND_SUBTRACT:
    case GPU_BLEND_ADDITIVE_PREMULT: {
      /* Let alpha accumulate. */
      att_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;  ///     src_rgb = GL_ONE;
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;        ///   dst_rgb =GL_ONE;

      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  ///  src_alpha = GL_ONE;
      att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;   /// dst_alpha = GL_ONE;
     

      break;
    }
    case GPU_BLEND_MULTIPLY: {
      att_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;  ///     src_rgb =GL_DST_COLOR;
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;  ///   dst_rgb =GL_ZERO;

      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;  ///  src_alpha =GL_DST_ALPHA;
      att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  /// dst_alpha = GL_ZERO;
     

      break;
    }
    case GPU_BLEND_INVERT: {
      att_state.srcColorBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;               ///    src_rgb = GL_ONE_MINUS_DST_COLOR;
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;       /// dst_rgb = GL_ZERO;

      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  ///   src_alpha = GL_ZERO;
      att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;        /// dst_alpha = = GL_ONE;

      break;
    }
    case GPU_BLEND_OIT: {
      att_state.srcColorBlendFactor =
          VK_BLEND_FACTOR_ONE;  ///    src_rgb = GL_ONE;
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;  /// dst_rgb = GL_ONE;

      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  ///   src_alpha =  GL_ZERO;
      att_state.dstAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  /// dst_alpha = =GL_ONE_MINUS_SRC_ALPHA;


      break;
    }
    case GPU_BLEND_BACKGROUND: {
      att_state.srcColorBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;              ///    src_rgb = GL_ONE_MINUS_DST_ALPHA
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;  /// dst_rgb = GL_SRC_ALPHA;

      att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  ///   src_alpha =  GL_ZERO;
      att_state.dstAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  /// dst_alpha = =GL_ONE_MINUS_SRC_ALPHA;


      break;
    }
    case GPU_BLEND_ALPHA_UNDER_PREMUL: {
      att_state.srcColorBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;  ///    src_rgb = GL_ONE_MINUS_DST_ALPHA
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;  /// dst_rgb = GL_ONE

      att_state.srcAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;  ///   src_alpha =  GL_ONE_MINUS_DST_ALPHA
      att_state.dstAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE;  /// dst_alpha = =GL_ONE;


      break;
    }
    case GPU_BLEND_CUSTOM: {
      att_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;  ///    src_rgb = GL_ONE
      att_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC1_COLOR;  /// dst_rgb = GL_SRC1_COLOR

      att_state.srcAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE;  ///   src_alpha =  GL_ONE
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
    ///glBlendEquation(GL_FUNC_ADD);
  }

  /* Always set the blend function. This avoid a rendering error when blending is disabled but
   * GPU_BLEND_CUSTOM was used just before and the frame-buffer is using more than 1 color target.
   */
  ///glBlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
  if (value != GPU_BLEND_NONE) {
    ///glEnable(GL_BLEND);
    att_state.blendEnable = VK_TRUE;
  }
  else {
    ///glDisable(GL_BLEND);
    att_state.blendEnable = VK_FALSE;
  }
}

void VKStateManager::set_color_blend_from_fb(VKFrameBuffer* fb) {

  VkPipelineColorBlendStateCreateInfo &cb = current_pipeline_.colorblend;
  cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  cb.pNext = NULL;
  cb.flags = 0;

  VkPipelineColorBlendAttachmentState att_state_last = current_pipeline_.colorblend_attachment.last();

  auto size_atta = current_pipeline_.colorblend_attachment.size();
  cb.attachmentCount = 0;

  for (auto desc : fb->get_attach_desc()) {
    if (desc.finalLayout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
      continue;
    };
    cb.attachmentCount++;
    VkPipelineColorBlendAttachmentState att_state = att_state_last;
    if (cb.attachmentCount  > (size_atta)) {
      current_pipeline_.colorblend_attachment.append(att_state_last);
      size_atta++;
    }
    else {
      current_pipeline_.colorblend_attachment[cb.attachmentCount-1] = att_state_last;
    }

    }

  cb.pAttachments = current_pipeline_.colorblend_attachment.data();
    cb.logicOpEnable = VK_FALSE;
    cb.logicOp = VK_LOGIC_OP_NO_OP;

  


};
/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture State Management
 * \{ */

void VKStateManager::texture_bind(Texture *tex_, eGPUSamplerState sampler_type, int binding)
{
  BLI_assert(binding < GPU_max_textures());
  VKTexture *tex = static_cast<VKTexture *>(tex_);
  if (G.debug & G_DEBUG_GPU) {
    tex->check_feedback_loop();
  }
  

  auto shader = VKContext::get()->pipeline_state.active_shader;
  shader->append_write_descriptor(tex, sampler_type,binding);
  /* Eliminate redundant binds. */
  if ((textures_[binding].imageView == tex->desc_info_.imageView) && (this->samplers_[binding] == ((uint32_t) sampler_type))) {
    return;
  }
  targets_[binding] = tex->target_type_;
  textures_[binding] = tex->desc_info_;
  this->samplers_[binding] = (uint)sampler_type;
  tex->is_bound_ = true;
  dirty_texture_binds_ |= 1ULL << binding;
}

void VKStateManager::texture_bind_temp(VKTexture *tex)
{
  /*TODO :: Set in descriptorset. */

  /*TODO :: Will update the descriptorset.*/
  dirty_texture_binds_ |= 1ULL;
}

void VKStateManager::texture_unbind(Texture *tex_)
{
  VKTexture *tex = static_cast<VKTexture *>(tex_);
  if ( (!tex->is_bound_) || (tex->desc_info_.imageView == VK_NULL_HANDLE) ){
    return;
  }


  for (int i = 0; i < ARRAY_SIZE(textures_); i++) {
    if (textures_[i].imageView == tex->desc_info_.imageView) {
      textures_[i] = {};
      this->samplers_[i] = 0;
      dirty_texture_binds_ |= 1ULL << i;
    }
  }
  tex->is_bound_ = false;
}

void VKStateManager::texture_unbind_all()
{
  for (int i = 0; i < ARRAY_SIZE(textures_); i++) {
    if (textures_[i].imageView != VK_NULL_HANDLE) {
      textures_[i] = {VK_NULL_HANDLE,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_MAX_ENUM};
      this->samplers_[i] = 0;
      dirty_texture_binds_ |= 1ULL << i;
    }
  }
  this->texture_bind_apply();
}


void VKStateManager::texture_bind_apply()
{
  if (dirty_texture_binds_ == 0) {
    return;
  }
  
  
  //uint64_t dirty_bind = dirty_texture_binds_;
  
  dirty_texture_binds_ = 0;
  /*
  int first = bitscan_forward_uint64(dirty_bind);
  int last = 64 - bitscan_reverse_uint64(dirty_bind);
  int count = last - first;
  */
  ///
  ///TODO write out Or rebuild layout set
  /*

  if (GLContext::multi_bind_support) {
    glBindTextures(first, count, textures_ + first);
    glBindSamplers(first, count, samplers_ + first);
  }
  else {
    for (int unit = first; unit < last; unit++) {
      if ((dirty_bind >> unit) & 1UL) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(targets_[unit], textures_[unit]);
        glBindSampler(unit, samplers_[unit]);
      }
    }
  }
  */
}

void VKStateManager::texture_unpack_row_length_set(uint len)
{
  ///glPixelStorei(GL_UNPACK_ROW_LENGTH, len);
}

uint64_t VKStateManager::bound_texture_slots()
{
  uint64_t bound_slots = 0;
  for (int i = 0; i < ARRAY_SIZE(textures_); i++) {
    if (textures_[i].imageView != VK_NULL_HANDLE) {
      bound_slots |= 1ULL << i;
    }
  }
  return bound_slots;
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Binding (from image load store)
 * \{ */

void VKStateManager::image_bind(Texture *tex_, int unit)
{
  /* Minimum support is 8 image in the fragment shader. No image for other stages. */
  BLI_assert(GPU_shader_image_load_store_support() && unit < 8);
  VKTexture *tex = static_cast<VKTexture *>(tex_);
  if (G.debug & G_DEBUG_GPU) {
    tex->check_feedback_loop();
  }
  images_[unit] = tex->tex_id_;
  formats_[unit] = to_vk(tex->format_);
  tex->is_bound_image_ = true;
  dirty_image_binds_ |= 1ULL << unit;
}

void VKStateManager::image_unbind(Texture *tex_)
{
  VKTexture *tex = static_cast<VKTexture *>(tex_);
  if (!tex->is_bound_image_) {
    return;
  }

  GLuint tex_id = tex->tex_id_;
  for (int i = 0; i < ARRAY_SIZE(images_); i++) {
    if (images_[i] == tex_id) {
      images_[i] = 0;
      dirty_image_binds_ |= 1ULL << i;
    }
  }
  tex->is_bound_image_ = false;
}

void VKStateManager::image_unbind_all()
{
  for (int i = 0; i < ARRAY_SIZE(images_); i++) {
    if (images_[i] != 0) {
      images_[i] = 0;
      dirty_image_binds_ |= 1ULL << i;
    }
  }
  this->image_bind_apply();
}

void VKStateManager::image_bind_apply()
{
  if (dirty_image_binds_ == 0) {
    return;
  }
  //uint32_t dirty_bind = dirty_image_binds_;
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
        glBindImageTexture(unit, images_[unit], 0, GL_TRUE, 0, GL_READ_WRITE, formats_[unit]);
      }
    }
  }
  */


}

uint8_t VKStateManager::bound_image_slots()
{
  uint8_t bound_slots = 0;
  for (int i = 0; i < ARRAY_SIZE(images_); i++) {
    if (images_[i] != 0) {
      bound_slots |= 1ULL << i;
    }
  }
  return bound_slots;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Memory barrier
 * \{ */

void VKStateManager::issue_barrier(eGPUBarrier barrier_bits)
{
 /// TODO PipelineBarrier
  ///glMemoryBarrier(to_gl(barrier_bits));
}

/** \} */

}  // namespace blender::gpu///

#endif
