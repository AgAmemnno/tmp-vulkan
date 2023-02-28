/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#pragma once

#ifdef __APPLE__
#  include <MoltenVK/vk_mvk_moltenvk.h>
#else
#  include <vulkan/vulkan.h>
#endif

#include "BLI_vector.hh"
#include "GPU_framebuffer_private.hh"
#include "GPU_vertex_format.h"

#define GPU_VK_DEBUG

#ifdef GPU_VK_DEBUG
#  define GPU_VK_DEBUG_PRINTF(...) printf(__VA_ARGS__);
#else
#  define GPU_VK_DEBUG_PRINTF(...)
#endif
#define VK_DEVICE blender::gpu::VKContext::get()->device_get()

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)
#define VK_CHECK2(expr) \
  do { \
    if ((expr) < 0) { \
      assert(0 && #expr); \
      throw std::runtime_error(__FILE__ "(" LINE_STRING "): VkResult( " #expr " ) < 0"); \
    } \
  } while (false)

/* SPDX-License-Identifier: GPL-2.0-or-later */

/* Global parameters. */
#define VK_SSBO_VERTEX_FETCH_MAX_VBOS 6 /* buffer bind 0..5 */
#define VK_SSBO_VERTEX_FETCH_IBO_INDEX VK_SSBO_VERTEX_FETCH_MAX_VBOS

/* Add Types as needed (Also need to be added to mtl_shader.h). */
#define GPU_SHADER_ATTR_TYPE_FLOAT 0
#define GPU_SHADER_ATTR_TYPE_INT 1
#define GPU_SHADER_ATTR_TYPE_SHORT 2
#define GPU_SHADER_ATTR_TYPE_CHAR 3
#define GPU_SHADER_ATTR_TYPE_VEC2 4
#define GPU_SHADER_ATTR_TYPE_VEC3 5
#define GPU_SHADER_ATTR_TYPE_VEC4 6
#define GPU_SHADER_ATTR_TYPE_UVEC2 7
#define GPU_SHADER_ATTR_TYPE_UVEC3 8
#define GPU_SHADER_ATTR_TYPE_UVEC4 9
#define GPU_SHADER_ATTR_TYPE_IVEC2 10
#define GPU_SHADER_ATTR_TYPE_IVEC3 11
#define GPU_SHADER_ATTR_TYPE_IVEC4 12
#define GPU_SHADER_ATTR_TYPE_MAT3 13
#define GPU_SHADER_ATTR_TYPE_MAT4 14
#define GPU_SHADER_ATTR_TYPE_UCHAR_NORM 15
#define GPU_SHADER_ATTR_TYPE_UCHAR2_NORM 16
#define GPU_SHADER_ATTR_TYPE_UCHAR3_NORM 17
#define GPU_SHADER_ATTR_TYPE_UCHAR4_NORM 18
#define GPU_SHADER_ATTR_TYPE_INT1010102_NORM 19
#define GPU_SHADER_ATTR_TYPE_SHORT3_NORM 20
#define GPU_SHADER_ATTR_TYPE_CHAR2 21
#define GPU_SHADER_ATTR_TYPE_CHAR3 22
#define GPU_SHADER_ATTR_TYPE_CHAR4 23
#define GPU_SHADER_ATTR_TYPE_UINT 24
using namespace blender;
namespace blender::gpu {
class VKShader;
class VKTexture;

typedef enum VKPipelineStateDirtyFlag {
  VK_PIPELINE_STATE_NULL_FLAG = 0,
  /* Whether we need to call setViewport. */
  VK_PIPELINE_STATE_VIEWPORT_FLAG = (1 << 0),
  /* Whether we need to call setScissor. */
  VK_PIPELINE_STATE_SCISSOR_FLAG = (1 << 1),
  /* Whether we need to update/rebind active depth stencil state. */
  VK_PIPELINE_STATE_DEPTHSTENCIL_FLAG = (1 << 2),
  /* Whether we need to update/rebind active PSO. */
  VK_PIPELINE_STATE_PSO_FLAG = (1 << 3),
  /* Whether we need to update the frontFacingWinding state. */
  VK_PIPELINE_STATE_FRONT_FACING_FLAG = (1 << 4),
  /* Whether we need to update the culling state. */
  VK_PIPELINE_STATE_CULLMODE_FLAG = (1 << 5),
  /* Full pipeline state needs applying. Occurs when beginning a new render pass. */
  VK_PIPELINE_STATE_ALL_FLAG = (VK_PIPELINE_STATE_VIEWPORT_FLAG | VK_PIPELINE_STATE_SCISSOR_FLAG |
                                VK_PIPELINE_STATE_DEPTHSTENCIL_FLAG | VK_PIPELINE_STATE_PSO_FLAG |
                                VK_PIPELINE_STATE_FRONT_FACING_FLAG |
                                VK_PIPELINE_STATE_CULLMODE_FLAG)
} VKPipelineStateDirtyFlag;

struct VKUniformBufferBinding {
  /// <summary>
  /// TODO ::
  ///  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
  /// </summary>
  bool bound;

  VkDescriptorBufferInfo ubo;
  VkWriteDescriptorSet write;
  VKUniformBufferBinding()
  {
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = NULL;
    write.dstSet = 0;
    write.descriptorCount = 1;
    write.pBufferInfo = &ubo;
    write.pTexelBufferView = NULL;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.dstArrayElement = 0;
    write.dstBinding = 0;
    ubo.buffer = VK_NULL_HANDLE;
    ubo.offset = 0;
    ubo.range = VK_WHOLE_SIZE;
  }
};
struct VKSamplerState {
  bool initialized;
  eGPUSamplerState state;

  bool operator==(const VKSamplerState &other) const
  {
    /* Add other parameters as needed. */
    return (this->state == other.state);
  }

  operator uint() const
  {
    return uint(state);
  }

  operator uint64_t() const
  {
    return uint64_t(state);
  }
  VKSamplerState()
  {
    initialized = false;
  };
  VKSamplerState(eGPUSamplerState state_) : state(state_)
  {
    initialized = false;
  }
};

const VKSamplerState DEFAULT_SAMPLER_STATE = VKSamplerState(GPU_SAMPLER_DEFAULT /*, 0, 9999*/);

struct VKTextureBinding {

  bool used;
  uint slot_index;
  VkDescriptorImageInfo info;
  VkWriteDescriptorSet write;
  VKTexture *texture_resource;
  /* Structs containing information on current binding state for textures and samplers. */

  VKTextureBinding()
  {
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = NULL;
    write.dstSet = 0;
    write.descriptorCount = 1;
    write.pImageInfo = &info;
    write.pBufferInfo = NULL;
    write.pTexelBufferView = NULL;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.dstArrayElement = 0;
    write.dstBinding = 0;
    info.imageView = VK_NULL_HANDLE;
    info.sampler = VK_NULL_HANDLE;
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  };
  VkWriteDescriptorSet &getWriteDesc(VkImageView view)
  {
    info.imageView = view;
    return write;
  }
};
struct VKSamplerBinding {
  bool used;
  VKSamplerState state;
  VkDescriptorImageInfo info;
  VkWriteDescriptorSet write;
  VKSamplerBinding()
  {
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = NULL;
    write.dstSet = 0;
    write.descriptorCount = 1;
    write.pImageInfo = &info;
    write.pBufferInfo = NULL;
    write.pTexelBufferView = NULL;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    write.dstArrayElement = 0;
    write.dstBinding = 0;
    info.imageView = VK_NULL_HANDLE;
    info.sampler = VK_NULL_HANDLE;
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }
  bool operator==(VKSamplerBinding const &other) const
  {
    return (used == other.used && state == other.state);
  }
  VkWriteDescriptorSet &getWriteDesc(VkSampler sampler)
  {
    info.sampler = sampler;
    return write;
  }
};

struct PipelineStateCreateInfoVk {
  bool dirty;
  bool initialised;

  /* Shader resources. */
  VKShader *null_shader;
  /* Active Shader State. */
  VKShader *active_shader;

  VKPipelineStateDirtyFlag dirty_flags;

  std::vector<VkPipelineShaderStageCreateInfo> shaderstages;

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

  /// <summary>
  /// pNext Or Extentions Chain
  /// </summary>
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
  }
};

struct VKGraphicsPipelineStateDescriptor {

  VkGraphicsPipelineCreateInfo pipelineCI;
};

}  // namespace blender::gpu
