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

#include "gpu_shader_interface.hh"
#include "BLI_vector.hh"
#include "vk_context.hh"


#include "gpu_shader_create_info.hh"
#include "vk_shader_interface_type.hh"

///https://github.com/KhronosGroup/SPIRV-Cross
///Dependent packages for reverse engineering and code modifications
#include "spirv_cross.hpp"
#include "spirv_glsl.hpp"



namespace spirv_cross {
class CompilerBlender;
};
namespace blender::gpu {

class VKVaoCache;
struct ShaderModule {
  ShaderModule() : module(0)
  {
  }

  VkShaderModule module;
  VkShaderModuleCreateInfo shaderModuleInfo;

  // Definition definition;
};
struct VKDescriptorInputs {

  VkPipelineVertexInputStateCreateInfo vkPVISci = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL};
  blender::Vector<VkVertexInputBindingDescription> bindings;
  blender::Vector<VkVertexInputAttributeDescription> attributes;
  blender::Vector<std::string> attr_names;

  void initialise(uint32_t stride, uint32_t attr_nums);
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
      }else
          new_.append(range);
    }
    if (!fnd) {
      VkPushConstantRange new_push = {
          stageFlags,offset,
          size,
      };
      push_ranges_.append(new_push);
    }else
    push_ranges_ = new_;





  }
};
class VKShaderInterface : public ShaderInterface {
#define VK_LAYOUT_BINDING_LIMIT 128
#define VK_LAYOUT_IMAGE_TYPE_NUNMS 3
  const uint32_t swapchain_image_nums = 2;

 public:
  VKShaderInterface();
  ~VKShaderInterface();
  bool valid = false;
  blender::Vector<VkDescriptorPoolSize> poolsize_;
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  blender::Vector<VkDescriptorSetLayout> setlayouts_;
  blender::Vector<VkDescriptorSetLayoutBinding> setlayoutbindings_;
  blender::Vector<blender::Vector<VkDescriptorSet>> sets_vec_;
  VkPipelineLayout pipelinelayout_ = VK_NULL_HANDLE;
  blender::Vector<VKDescriptorInputs> desc_inputs_;
  VkPushConstantRange                         push_range_;
  int push_loc_[2];
  bool is_valid_push_location(int i)
  {
    /// <summary>
    /// Uniform is pushConst only, others are described in descriptorsets.
    /// </summary>
    return (push_loc_[0] <= i && push_loc_[1] >= i);
  };
  char*                                         push_cache_ =nullptr;
  _max_name_len  max_name_len_ = {0, 0, 0, 0, 0};
  _len  len_ = {0, 0, 0, 0, 0};
  uint32_t name_buffer_len_ = 0, name_buffer_offset_= 0;
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
  GHOST_TSuccess createSetLayout(VkDescriptorSetLayout &setlayout);

  /// <summary>
  /// Classifying the pool size by VkDescriptorType results in a simple sense, Are there other good classifications?
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
  void append_binding(uint32_t binding,
                      const char *name,
                      VkDescriptorType dtype,
                      VkShaderStageFlagBits stage);
  bool apply(spirv_cross::CompilerBlender *vert,
             spirv_cross::CompilerBlender *frag,
             spirv_cross::CompilerBlender *geom =nullptr);




  bool parse(ShaderModule &vcode, ShaderModule &fcode);
  bool finalize(VkPipelineLayout* playout = nullptr);

  GHOST_TSuccess createPipelineLayout(VkPipelineLayout &layout,
                                      blender::Vector<VkDescriptorSetLayout> &pSetLayouts,
                                      blender::Vector<VkPushConstantRange> pushConstantRange);
  bool alloc_inputs(Vector<_max_name_len> max_names, Vector<_len> lens);
  const ShaderInput *append_input(int index,
                                  const char *name,
                                  int name_len,
                                  int location,
                                  int binding,
                                  bool attr = false,
                                  bool tex = false,
                                  bool ima = false);
  int sortUniformLocation();


 private:
  VkShaderStageFlagBits current_stage_;
  int pool_image_index_[3];
  spirv_cross::SPIRType::BaseType
      cache_set[VK_LAYOUT_BINDING_LIMIT];  /// Assume that the set number is only 0.
  
};
}

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

  int parse_inputs(blender::gpu::ShaderInput *inputs,int ofs = 0);
  int parse_images(blender::gpu::ShaderInput *inputs,int ofs = 0);
  /// <summary>
  /// TODO:: SSBO UBO
  /// </summary>
  int parse_pushconst(blender::gpu::ShaderInput *inputs,int ofs= 0);
  blender::gpu::_max_name_len max_name_len_ = {0, 0, 0, 0, 0};
  blender::gpu::_len len_ = {0, 0, 0, 0, 0};
  /// <summary>
  /// TODO:: ssbo , inputattachment,ubo
  /// </summary>
  ///
 private:
  blender::gpu::VKShaderInterface &iface_;
  ShaderResources resources_;
  VkShaderStageFlagBits stage_;
};
}  // namespace spirv_cross

