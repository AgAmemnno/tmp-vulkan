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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * GPU shader interface (C --> GLSL)
 *
 * Structure detailing needed vertex inputs and resources for a specific shader.
 * A shader interface can be shared between two similar shaders.
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "BLI_vector.hh"
#include "gpu_shader_interface.hh"
#include "vk_context.hh"

#include "gpu_shader_create_info.hh"
#include "vk_shader_interface_type.hh"

/// https://github.com/KhronosGroup/SPIRV-Cross
/// Dependent packages for reverse engineering and code modifications
#include "spirv_cross.hpp"
#include "spirv_glsl.hpp"

namespace spirv_cross {
class CompilerBlender;
};
namespace blender::gpu {

class VKVaoCache;
class VKVertBuf;

struct ShaderModule {
  ShaderModule() : module(0)
  {
  }

  VkShaderModule module;
  VkShaderModuleCreateInfo shaderModuleInfo;

  // Definition definition;
};
struct VKDescriptorInputs {

  bool is_block = false;

  VkPipelineVertexInputStateCreateInfo vkPVISci = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL};
  blender::Vector<VkVertexInputBindingDescription> bindings;
  blender::Vector<VkVertexInputAttributeDescription> attributes;
  blender::Vector<std::string> attr_names;

  void append(uint32_t stride, uint32_t binding, bool vert);
  void initialise(uint32_t attr_vertex_nums, uint32_t attr_instance_nums, bool block);
  void finalise();
};
struct _max_name_len {
  uint32_t attr, ubo, ssbo, image, push;
};

struct _len {
  uint32_t attr, ubo, ssbo, image, push;
};
struct PushRanges {
  Vector<VkPushConstantRange> push_ranges_;

  void append(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size)
  {

    Vector<VkPushConstantRange> new_;
    bool fnd = false;
    for (auto &range : push_ranges_) {
      if (range.size == size) {
        if (range.offset == offset) {
          VkPushConstantRange new_push = {range.stageFlags | stageFlags, range.offset, range.size};
          new_.append(new_push);
          fnd = true;
        }
      }
      else
        new_.append(range);
    }
    if (!fnd) {
      VkPushConstantRange new_push = {
          stageFlags,
          offset,
          size,
      };
      push_ranges_.append(new_push);
    }
    else
      push_ranges_ = new_;
  }
};
class VKShaderInterface : public ShaderInterface {
#define VK_LAYOUT_BINDING_LIMIT 32
#define VK_LAYOUT_IMAGE_TYPE_NUNMS 3
#define VK_LAYOUT_SET_MAX 2
  const uint32_t swapchain_image_nums = 2;

 public:
  VKShaderInterface();
  ~VKShaderInterface();
  bool valid = false;
  blender::Vector<VkDescriptorPoolSize> poolsize_;
  int max_inline_ubo_;
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout setlayouts_[VK_LAYOUT_SET_MAX];

  blender::Vector<VkDescriptorSetLayoutBinding> setlayoutbindings_[VK_LAYOUT_SET_MAX];
  blender::Vector<VkDescriptorSet> sets_vec_[VK_LAYOUT_SET_MAX];

  VkPipelineLayout pipelinelayout_ = VK_NULL_HANDLE;
  blender::Vector<VKDescriptorInputs> desc_inputs_;
  VkPushConstantRange push_range_;
  Vector<VKVaoCache *> refs_;
  shader::ShaderCreateInfo *sc_info_ = nullptr;
  VKShader *active_shader = nullptr;

  int push_loc_[2];
  bool is_valid_push_location(int i)
  {
    /// <summary>
    /// Uniform is pushConst only, others are described in descriptorsets.
    /// </summary>
    return (push_loc_[0] <= i && push_loc_[1] >= i);
  };
  char *push_cache_ = nullptr;
  _max_name_len max_name_len_ = {0, 0, 0, 0, 0};
  _len len_ = {0, 0, 0, 0, 0};
  uint32_t name_buffer_len_ = 0, name_buffer_offset_ = 0;
  void destroy();
  uint32_t getInputIndexPush()
  {
    return len_.attr + len_.ubo + +len_.image;
  };

  uint32_t Set_input_name(ShaderInput *input, char *name, uint32_t name_len) const;

  /// <summary>
  ///  Generate SetLayout from SPV OPCode.
  /// TODO ::: Specialize PoolSize.
  /// </summary>
  GHOST_TSuccess createSetLayout(uint i);

  /// <summary>
  /// Classifying the pool size by VkDescriptorType results in a simple sense, Are there other good
  /// classifications?
  /// </summary>
  GHOST_TSuccess createPool();

  GHOST_TSuccess allocateDescriptorSets(blender::Vector<VkDescriptorSet> &Sets,
                                        VkDeviceSize setlayoutID,
                                        uint32_t count = 1);

  /// <summary>
  /// update poolsize description
  /// </summary>
  /// <param name="binding"></param>
  /// <param name="btype"></param>
  /// <returns>
  /// new binding or not.
  /// </returns>
  bool append_image(uint32_t binding, spirv_cross::SPIRType::BaseType btype);
  bool append_ubo(uint32_t set, uint32_t binding, uint32_t block_size);
  void append_binding(uint32_t binding,
                      const char *name,
                      VkDescriptorType dtype,
                      VkShaderStageFlagBits stage,
                      uint32_t count = 1,
                      uint setNum = 0);
  bool apply(spirv_cross::CompilerBlender *vert,
             spirv_cross::CompilerBlender *frag,
             spirv_cross::CompilerBlender *geom = nullptr);

  bool parse(ShaderModule &vcode,
             ShaderModule &fcode,
             const shader::ShaderCreateInfo *scinfo,
             VKShader *shader);

  bool finalize(VkPipelineLayout *playout = nullptr);

  GHOST_TSuccess createPipelineLayout(VkPipelineLayout &layout,
                                      blender::Vector<VkPushConstantRange> pushConstantRange);
  bool alloc_inputs(Vector<_max_name_len> max_names, Vector<_len> lens);
  const ShaderInput *append_input(int index,
                                  const char *name,
                                  int name_len,
                                  int location,
                                  int binding,
                                  bool attr = false,
                                  bool tex = false,
                                  bool ima = false,
                                  bool ubo = false);
  int sortUniformLocation();
  void ref_remove(VKVaoCache *ref);
  void ref_add(VKVaoCache *ref);

  uint16_t vbo_bind(VKVertBuf *vbo,
                    VkCommandBuffer cmd,
                    const GPUVertFormat *format,
                    uint v_first,
                    uint v_len,
                    const bool use_instancing);

 private:
  VkShaderStageFlagBits current_stage_;
  int pool_image_index_[3];
  /* Assume that the set number is only 0. */
  spirv_cross::SPIRType::BaseType cache_set[VK_LAYOUT_BINDING_LIMIT];
  /* Set No.1 is an alternative to uniform*/
  spirv_cross::SPIRType::BaseType cache_set1[VK_LAYOUT_BINDING_LIMIT];
};
}  // namespace blender::gpu

namespace spirv_cross {
class CompilerBlender : public CompilerGLSL {

 public:
  CompilerBlender(std::vector<uint32_t> &ir,
                  blender::gpu::VKShaderInterface &iface,
                  VkShaderStageFlagBits stage);

  struct NameBuffer {
    char *name_buffer = nullptr;
    uint32_t name_buffer_len = 0;
    uint32_t name_buffer_offset = 0;
    uint16_t *enabled_attr_mask_;
    uint64_t *enabled_tex_mask_;
    uint8_t *enabled_ima_mask_;
  } NB;

  virtual ~CompilerBlender(){};

  int parse_inputs(blender::gpu::ShaderInput *inputs, int ofs = 0);
  int parse_images(blender::gpu::ShaderInput *inputs, int ofs = 0);
  int parse_ubo(blender::gpu::ShaderInput *inputs, int ofs);

  int parse_pushconst(blender::gpu::ShaderInput *inputs, int ofs = 0);
  blender::gpu::_max_name_len max_name_len_ = {0, 0, 0, 0, 0};
  blender::gpu::_len len_ = {0, 0, 0, 0, 0};

 private:
  blender::gpu::VKShaderInterface &iface_;
  ShaderResources resources_;
  VkShaderStageFlagBits stage_;
};
}  // namespace spirv_cross
