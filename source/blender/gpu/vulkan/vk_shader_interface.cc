/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU shader interface (C --> GLSL)
 */

#include "MEM_guardedalloc.h"
#include "vk_layout.hh"



#include "BLI_bitmap.h"
#include "GPU_capabilities.h"
#include "vk_debug.hh"
#include "vk_shader_interface.hh"
#include "vk_shader_interface_type.hh"
#include "vk_context.hh"
#include "vk_vertex_buffer.hh"
#include "vk_framebuffer.hh"
#include "vk_shader.hh"

#include "BLI_blenlib.h"
#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "BLI_vector.hh"

#include "gpu_shader_create_info.hh"
#include "gpu_shader_interface.hh"




#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>

namespace blender::gpu {
const int UNIFORM_SORT_OFS = 1000;

void VKDescriptorInputs::initialise(uint32_t attr_vertex_nums, uint32_t attr_instance_nums,bool block )
{
  /*TODO :: how to recognize an instance buffer?*/
  /* Naive implementation.*/
  /* Create a VkBuffer for each vbo and set location == binding.This way we can treat instances and vertices the same, but the number of uploads and binds will increase. */
  is_block = block;
  /// TODO  :: VK_VERTEX_INPUT_RATE_INSTANCE = 1;
  if (block) {
    if (attr_instance_nums > 0) {
      bindings.resize(2);
    }
    else {
      bindings.resize(1);
    }
  }
  else {
    bindings.resize(attr_vertex_nums + attr_instance_nums);
  }

  attributes.resize(attr_vertex_nums + attr_instance_nums);
};

void VKDescriptorInputs::append(uint32_t stride, uint32_t binding, bool vert) {
  
  VkVertexInputBindingDescription& I0 = bindings[binding];
  I0.binding       = binding;
  I0.stride        = stride;
  I0.inputRate  = (vert) ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;

};

void VKDescriptorInputs::finalise()
{
  vkPVISci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vkPVISci.pNext = NULL;  // TODO Divisor
  vkPVISci.flags = 0;     // VUID-VkPipelineVertexInputStateCreateInfo-flags-zerobitmask
  vkPVISci.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
  vkPVISci.pVertexBindingDescriptions = bindings.data();
  vkPVISci.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
  vkPVISci.pVertexAttributeDescriptions = attributes.data();
}



VKShaderInterface::VKShaderInterface()
{
  max_inline_ubo_ = 0;
  for (int i = 0; i < VK_LAYOUT_BINDING_LIMIT; i++) {
    cache_set[i] = spirv_cross::SPIRType::Unknown;
    cache_set1[i] = spirv_cross::SPIRType::Unknown;
  }
  for (int i = 0; i < VK_LAYOUT_IMAGE_TYPE_NUNMS; i++) {
    pool_image_index_[i] = -1;
  };
  push_range_.offset = 0;
  push_range_.size = 0;
  push_range_.stageFlags = 0;
  push_cache_ = nullptr;
  for (int i = 0; i < VK_LAYOUT_SET_MAX; i++) {
    sets_vec_[i].clear();
    setlayoutbindings_[i].clear();
    setlayouts_[i] = VK_NULL_HANDLE;
  }
  
  
  
  poolsize_.clear();
  desc_inputs_.clear();
};
VKShaderInterface ::~VKShaderInterface()
{
  MEM_SAFE_FREE(push_cache_);
  destroy();
};

void VKShaderInterface::destroy()
{
  auto device = blender::gpu::VKContext::get()->device_get();
#define DESTROYER(identifier, a) \
  { \
    if (a != VK_NULL_HANDLE) { \
      vkDestroy##identifier(device, a, nullptr); \
      a = VK_NULL_HANDLE; \
    } \
  }

  DESTROYER(PipelineLayout, pipelinelayout_)

  for (auto &v : sets_vec_)
    for (auto &set : v)
      vkFreeDescriptorSets(device, pool_, 1, &set);

  for (auto &a : setlayouts_)
    DESTROYER(DescriptorSetLayout, a)

  DESTROYER(DescriptorPool, pool_)

  for (int i = 0; i < VK_LAYOUT_SET_MAX; i++) {
      sets_vec_[i].clear();
      setlayoutbindings_[i].clear();
    }
  poolsize_.clear();
  desc_inputs_.clear();

#undef DESTROYER
}



uint16_t  VKShaderInterface::vbo_bind(VKVertBuf* vbo,VkCommandBuffer cmd, const GPUVertFormat* format,
  uint v_first,
  uint v_len,
  const bool use_instancing) {


  uint16_t enabled_attrib = 0;
  const uint attr_len = format->attr_len;
 

  if (attr_len_ <= 0) {
    return enabled_attrib;
  }


  blender::gpu::VKDescriptorInputs& dinputs = desc_inputs_.last();
  BLI_assert(dinputs.is_block);
  BLI_assert(dinputs.bindings[0].binding == 0);


  uint stride = format->stride;

  if (dinputs.bindings[0].stride != stride) {
    /* Need to generate the pipeline again.
    However, it can be avoided if more detailed layout information about input attribute can be described in shadercreateinfo. */
    VKContext* context = VKContext::get();
    auto fb        = static_cast<VKFrameBuffer*>(context->active_fb);
    auto shader =  context->pipeline_state.active_shader;
    shader->CreatePipeline(fb->get_render_pass());
  }


  
  //uint offset = 0;
  //GLuint divisor = (use_instancing) ? 1 : 0;

  for (uint a_idx = 0; a_idx < attr_len; a_idx++) {
    const GPUVertAttr* a = &format->attrs[a_idx];
    /*
    if (format->deinterleaved) {
      offset += ((a_idx == 0) ? 0 : format->attrs[a_idx - 1].size) * v_len;
      stride = a->size;
    }
    else {
      offset = a->offset;
    }
    */

    /* This is in fact an offset in memory. */
   // const GLvoid *pointer    = (const GLubyte *)intptr_t(offset + v_first * stride);
   // const   VkFormat  type = to_vk(static_cast<GPUVertCompType>(a->comp_type));

    for (uint n_idx = 0; n_idx < a->name_len; n_idx++) {
      const char* name = GPU_vertformat_attr_name_get(format, a, n_idx);
      const ShaderInput* input = attr_get(name);
      if (input == nullptr || input->location == -1) {
        continue;
      }
      if (use_instancing) {
        BLI_assert(input->binding == 1);
      }
      else {
        BLI_assert(input->binding == 0);
      }
      enabled_attrib |= (1 << input->location);
      /*TODO  attrib pointer*/
#if 0

      BLI_assert(false);
      if (ELEM(a->comp_len, 16, 12, 8)) {
        BLI_assert(a->fetch_mode == GPU_FETCH_FLOAT);
        BLI_assert(a->comp_type == GPU_COMP_F32);
        for (int i = 0; i < a->comp_len / 4; i++) {
          glEnableVertexAttribArray(input->location + i);
          glVertexAttribDivisor(input->location + i, divisor);
          glVertexAttribPointer(
            input->location + i, 4, type, GL_FALSE, stride, (const GLubyte*)pointer + i * 16);
        }
      }
      else {
        glEnableVertexAttribArray(input->location);
        glVertexAttribDivisor(input->location, divisor);

        switch (a->fetch_mode) {
        case GPU_FETCH_FLOAT:
        case GPU_FETCH_INT_TO_FLOAT:
          glVertexAttribPointer(input->location, a->comp_len, type, GL_FALSE, stride, pointer);
          break;
        case GPU_FETCH_INT_TO_FLOAT_UNIT:
          glVertexAttribPointer(input->location, a->comp_len, type, GL_TRUE, stride, pointer);
          break;
        case GPU_FETCH_INT:
          glVertexAttribIPointer(input->location, a->comp_len, type, stride, pointer);
          break;
        }
      }
#endif
    }
  }

  if (enabled_attrib) {
    VkDeviceSize offsets[1] = { 0 };
    VkBuffer vbo_ = vbo->get_vk_buffer();
    if (use_instancing) {
      vkCmdBindVertexBuffers(cmd, 1, 1, &vbo_, offsets);
    }
    else {
      vkCmdBindVertexBuffers(cmd, 0, 1, &vbo_, offsets);
    }
  }

  return enabled_attrib;


};


uint32_t VKShaderInterface::Set_input_name(ShaderInput *input, char *name, uint32_t name_len) const
{
  return set_input_name(input, name, name_len);
}

GHOST_TSuccess VKShaderInterface::createSetLayout(uint i)
{

  VkDescriptorSetLayout& setlayout = setlayouts_[i];
  size_t size = setlayoutbindings_[i].size();
  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, (uint32_t)size };
  if (size <= 0) {
    descriptorSetLayoutCreateInfo.pBindings = nullptr;
  }
  else {
    descriptorSetLayoutCreateInfo.pBindings = setlayoutbindings_[i].data();
  }
  for (auto& setlayout : setlayoutbindings_) {
    for (auto& set : setlayout) {
      printf(" setlayout  count %d  type %d binding %d ",set.descriptorCount ,set.descriptorType ,set.binding);
    }
  }
  VK_CHECK(vkCreateDescriptorSetLayout(blender::gpu::VKContext::get()->device_get(),
                                       &descriptorSetLayoutCreateInfo,
                                       NULL,
                                       &setlayout));
  return GHOST_kSuccess;
};

bool VKShaderInterface::finalize(VkPipelineLayout *playout)
{
  if (push_range_.size > 0) {
    createPipelineLayout(pipelinelayout_, { push_range_ });
  }
  else {
    createPipelineLayout(pipelinelayout_, {  });
  }
  if (playout) {
    *playout = pipelinelayout_;
  }
  return true;
 };
/// <summary>
/// Classifying the pool size by VkDescriptorType results in a simple sense, but what should
/// we do?
/// </summary>
GHOST_TSuccess VKShaderInterface::createPool()
{

  VkDescriptorPoolCreateInfo descriptorPoolInfo{};
  descriptorPoolInfo.maxSets = 0;

  for (auto psize : poolsize_) {
    psize.descriptorCount *= swapchain_image_nums;
    descriptorPoolInfo.maxSets += psize.descriptorCount;
  }
  max_inline_ubo_ *= swapchain_image_nums;
  if (descriptorPoolInfo.maxSets == 0)
    return GHOST_kFailure;

  descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolsize_.size());
  descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  descriptorPoolInfo.pPoolSizes = poolsize_.data();

  VkDescriptorPoolInlineUniformBlockCreateInfoEXT descriptorPoolInlineUniformBlockCreateInfo{};
  
  if (max_inline_ubo_ > 0) {
    descriptorPoolInlineUniformBlockCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT;
    descriptorPoolInlineUniformBlockCreateInfo.maxInlineUniformBlockBindings = max_inline_ubo_;
    descriptorPoolInfo.pNext = &descriptorPoolInlineUniformBlockCreateInfo;
  }


  VK_CHECK(vkCreateDescriptorPool(
      blender::gpu::VKContext::get()->device_get(), &descriptorPoolInfo, nullptr, &pool_));

  return GHOST_kSuccess;
};

GHOST_TSuccess VKShaderInterface::allocateDescriptorSets(blender::Vector<VkDescriptorSet>& Sets,
  VkDeviceSize setlayoutID,
  uint32_t count)
{

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

  VkDescriptorSetLayout& layout = setlayouts_[setlayoutID];

  if (count == 1) {
    allocInfo.descriptorSetCount = 1;
    Sets.resize(1);
    allocInfo.pSetLayouts = &layout;
    allocInfo.descriptorPool = pool_;
    VK_CHECK(vkAllocateDescriptorSets(
      blender::gpu::VKContext::get()->device_get(), &allocInfo, Sets.data()));
    return GHOST_kSuccess;
  }

  /// <summary>
  /// Plural by cloning layouts
  ///   
  /// </summary>
  Sets.resize(count);
  std::vector<VkDescriptorSetLayout> layouts(count);
  for (int i = 0; i < count; i++) {
    layouts[i] = layout;
  };

    allocInfo.descriptorPool = pool_;
    allocInfo.descriptorSetCount = count;
    allocInfo.pSetLayouts = layouts.data();
    VK_CHECK(vkAllocateDescriptorSets(
      blender::gpu::VKContext::get()->device_get(), &allocInfo, Sets.data()));

    return GHOST_kSuccess;
  };

/// <summary>
/// update poolsize description
/// </summary>
/// <param name="binding"></param>
/// <param name="btype"></param>
/// <returns>
/// new binding or not.
/// </returns>
bool VKShaderInterface::append_image(uint32_t binding, spirv_cross::SPIRType::BaseType btype)
{

#define INIT_STATE (cache_set[binding] == spirv_cross::SPIRType::Unknown)
#define DUPLI_STATE ((!INIT_STATE) && (cache_set[binding] == btype))

  BLI_assert(INIT_STATE || DUPLI_STATE);
  if (DUPLI_STATE) {
    return false;  /// nothing to do.
  }

  cache_set[binding] = btype;
  VkDescriptorPoolSize poolsize;
  /// <summary>
  /// VK_DESCRIPTOR_TYPE_STORAGE_IMAGE TODO
  /// </summary>
  auto POOL_INCR = [&](int N) {
    if (pool_image_index_[N] != -1)
      poolsize.descriptorCount += 1;
    else {
      pool_image_index_[N] = (int)poolsize_.size();
      poolsize.descriptorCount = 1;
      poolsize_.append(poolsize);
    }
  };

  switch (btype) {
    case spirv_cross::SPIRType::Sampler:
      poolsize.type = VK_DESCRIPTOR_TYPE_SAMPLER;
      POOL_INCR(0);
      break;
    case spirv_cross::SPIRType::SampledImage:
      poolsize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      POOL_INCR(1);
      break;
    case spirv_cross::SPIRType::Image:
      poolsize.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
      POOL_INCR(2);
      break;
    default:
      BLI_assert(false);
      break;
  }

  return true;
}
bool VKShaderInterface::append_ubo(uint32_t set,uint32_t binding, uint32_t block_size)
{

  BLI_assert(set <= 1);
  if (set == 1) {
    /* https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkDescriptorPoolSize.html */
    BLI_assert(block_size % 4 == 0);
  }

  spirv_cross::SPIRType::BaseType* cache = cache_set;
  if (set == 1) {
    cache = cache_set1;
  };

#define INIT_STATE_ (cache[binding] == spirv_cross::SPIRType::Unknown)
#define DUPLI_STATE_ ((!INIT_STATE_) && (cache[binding] == spirv_cross::SPIRType::Struct))

  BLI_assert(INIT_STATE_ || DUPLI_STATE_);
  if (DUPLI_STATE_) {
    return false;  /// nothing to do.
  }

  cache[binding] = spirv_cross::SPIRType::Struct;
  VkDescriptorPoolSize poolsize;



  switch (set) {
  case 0:
    poolsize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolsize.descriptorCount  = 1;
    break;
  case 1:
    poolsize.type = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
    poolsize.descriptorCount = block_size;
    /*Add max_inline_ubo_ when replicating uniform blocks.*/
    max_inline_ubo_++;
    break;
  default:
    BLI_assert(false);
    break;
  }

  poolsize_.append(poolsize);
#undef INIT_STATE_ 
#undef DUPLI_STATE_ 
  return true;
}
void VKShaderInterface::append_binding(uint32_t binding,
                                       const char *name,
                                       VkDescriptorType dtype,
                                       VkShaderStageFlagBits stage, uint32_t count, uint setNum)
{
  auto & setlayouts = setlayoutbindings_[setNum];
  auto size = setlayouts.size();
  if (size <= binding) {
    VkDescriptorSetLayoutBinding dslb = {0, (VkDescriptorType)0, 0, 0, NULL};
    for (int i = size; i < binding + 1; i++) {
      setlayouts.append(dslb);
    }
  }

  auto &slb = setlayoutbindings_[setNum][binding];
  slb.binding = binding;
  slb.descriptorType = dtype;
  if (slb.descriptorCount == 0)
    slb.stageFlags = stage;
  else
    slb.stageFlags |= stage;

  slb.descriptorCount = count;
  slb.pImmutableSamplers = NULL;

}
bool VKShaderInterface::alloc_inputs(Vector<_max_name_len> max_names, Vector<_len> lens)
{

  auto &dst_max = max_name_len_ = {0, 0, 0, 0, 0};
  auto &dst_len = len_ = {0, 0, 0, 0, 0};

  for (int i = 0; i < max_names.size(); i++) {
    auto &src_max = max_names[i];
    dst_max.attr = std::max(src_max.attr, dst_max.attr);
    dst_max.ubo = std::max(src_max.ubo, dst_max.ubo);
    dst_max.image = std::max(src_max.image, dst_max.image);
    dst_max.ssbo = std::max(src_max.ssbo, dst_max.ssbo);
    dst_max.push = std::max(src_max.push, dst_max.push);

    auto &src_len = lens[i];
    dst_len.attr = std::max(src_len.attr, dst_len.attr);
    dst_len.ubo = std::max(src_len.ubo, dst_len.ubo);
    dst_len.image = std::max(src_len.image, dst_len.image);
    dst_len.ssbo = std::max(src_len.ssbo, dst_len.ssbo);
    dst_len.push = std::max(src_len.push, dst_len.push);
  }

  attr_len_ = len_.attr;
  ubo_len_ = len_.ubo;
  ssbo_len_ = len_.ssbo;
  uniform_len_ = len_.push + len_.image;

  int input_tot_len = len_.attr + len_.ubo + len_.image +  len_.push + len_.ssbo;
  inputs_ = (blender::gpu::ShaderInput *)MEM_callocN(
      sizeof(blender::gpu::ShaderInput) * input_tot_len, __func__);

  auto name_buffer_len = len_.attr * max_name_len_.attr + len_.ubo * max_name_len_.ubo +
                         len_.push * max_name_len_.push + len_.ssbo * max_name_len_.ssbo +
                         len_.image * max_name_len_.image;

  name_buffer_ = (char *)MEM_mallocN(name_buffer_len, "name_buffer");
  memset(name_buffer_, 0, name_buffer_len);

  enabled_attr_mask_ = 0;
  enabled_tex_mask_ = 0;
  enabled_ima_mask_ = 0;

  return true;
}
const ShaderInput* VKShaderInterface::append_input(int index,
  const char* name,
  int name_len,
  int location,
  int binding,
  bool attr,
  bool tex,
  bool ima, 
  bool ubo)
{

  char* dst = name_buffer_ + name_buffer_offset_;
  memcpy(dst, name, name_len);
  dst[name_len] = '\0';
  blender::gpu::ShaderInput& input = inputs_[index];

  name_buffer_offset_ += Set_input_name(&input, dst, name_len);
  input.location = location;
  input.binding = binding;

  if (attr) {
    enabled_attr_mask_ |= (1 << input.location);
  }
  if (tex) {
    enabled_tex_mask_ |= (1llu << input.binding);
  }
  if (ima) {
    enabled_ima_mask_ |= (1 << input.binding);
  }
   if(ubo) {
    enabled_ubo_mask_ |= (1 << input.binding);
  }
  return &input;
};


  int VKShaderInterface::sortUniformLocation()
{
    /*  attr => default ubo => Legacy uniform [  inline ubo ( binding Ascending ) => image (binding Ascending) => push constants (location Ascending) ]  */
  auto ofs = len_.attr ;
  auto OffsetCompare1 = [](const void* p1, const void* p2) -> int {

    auto l1 = ((ShaderInput*)p1)->location;
    auto l2 = ((ShaderInput*)p2)->location;
    if (l1 == l2) {
      auto b1 = ((ShaderInput*)p1)->binding;
      auto b2 = ((ShaderInput*)p2)->binding;
      BLI_assert(b1 != b2);
      return (b1 < b2) ? -1 : 1;
    }
    else {
      return (l1 < l2) ? -1 : 1;
    }
  };
  
  qsort(&inputs_[ofs], len_.ubo, sizeof(ShaderInput), OffsetCompare1);

  ofs += len_.ubo;
  auto OffsetCompare = [](const void *p1, const void *p2) -> int {
    auto b1 = ((ShaderInput *)p1)->binding;
    auto b2 = ((ShaderInput *)p2)->binding;
    BLI_assert(b1 != b2);
    return (b1 < b2) ? -1 : 1;
  };
  qsort(&inputs_[ofs], len_.image +  len_.push , sizeof(ShaderInput), OffsetCompare);


  /*Now assign new locations.*/
  int i = len_.attr;

  push_loc_[0] = 1000;
  push_loc_[1] = -1000;
  int ubo_inline = 0;
  for (; i < ((int)len_.attr + (int)len_.ubo); i++)
  {
    if (inputs_[i].location == 1) {
      inputs_[i].location  = i;
      inputs_[i].binding += UNIFORM_SORT_OFS;
      if (inputs_[i].location < push_loc_[0]) {
        push_loc_[0] = inputs_[i].location;
      }
      push_loc_[1] = inputs_[i].location;
      ubo_inline++;
    }
  }
  for (; i < ((int)len_.attr + (int)len_.ubo  +(int)len_.image + (int)len_.push ) ; i++)
  {
    inputs_[i].location = i;
    if (inputs_[i].binding >= UNIFORM_SORT_OFS) {
      if (inputs_[i].location < push_loc_[0]) {
        push_loc_[0] = inputs_[i].location;
      }
      push_loc_[1] = inputs_[i].location;
      inputs_[i].binding -= UNIFORM_SORT_OFS;
    };
  };

  ubo_len_ -= ubo_inline;
  uniform_len_ += ubo_inline;


  return 1;

}
bool VKShaderInterface::apply(spirv_cross::CompilerBlender *vert,
                              spirv_cross::CompilerBlender *frag,
                              spirv_cross::CompilerBlender *geom)
{

  BLI_assert(vert);
  BLI_assert(frag);
  BLI_assert(geom == nullptr);
  
  alloc_inputs({vert->max_name_len_, frag->max_name_len_}, {vert->len_, frag->len_});


  int ofs = 0;
  ofs =  vert->parse_inputs(inputs_);
  BLI_assert(len_.ssbo == 0);
  ofs += len_.ssbo;

  if (len_.ubo  > 0) {
    ofs += vert->parse_ubo(inputs_, ofs);
    ofs += frag->parse_ubo(inputs_, ofs);
  }

  if (len_.image > 0) {

    ofs += vert->parse_images(inputs_,ofs);

    ofs += frag->parse_images(inputs_ ,ofs);
  }

  if (len_.push > 0) {
    ofs += vert->parse_pushconst(inputs_ ,ofs);
    ofs += frag->parse_pushconst(inputs_, ofs);
  }

  push_cache_ = (char *)MEM_mallocN(push_range_.size, "push_cache");
  BLI_assert( ofs == len_.attr + len_.ubo + len_.image +  len_.push + len_.ssbo);
  


  if (createPool() == GHOST_kFailure) {
      /* no descriptor set. */
    return true;
  }
  else {
    
    /*Swapchain image nums.*/
    const uint32_t SetNums = VKContext::get()->getImageViewNums();


    int i = 0;
    for (auto& slb : setlayoutbindings_) {
      if (i == 0) {
        createSetLayout(i);
        allocateDescriptorSets(sets_vec_[i], i, SetNums);
      }else{

        if (slb.size() > 0) {
          createSetLayout(i);
          allocateDescriptorSets(sets_vec_[i], i, SetNums);
        }
        else {
          
          for (int j = 0; j < SetNums; j++) {
            sets_vec_[i].append(VK_NULL_HANDLE);
          }
          setlayouts_[i] = VK_NULL_HANDLE;
        }


      }
      i++;
    }

  }



  return true;
};

GHOST_TSuccess VKShaderInterface::createPipelineLayout(
    VkPipelineLayout &layout,
    blender::Vector<VkPushConstantRange> pushConstantRange)
{
  blender::Vector<VkDescriptorSetLayout> pSetLayouts;
  for (auto& setlayout : setlayouts_) {
    if (setlayout != VK_NULL_HANDLE) {
      pSetLayouts.append(setlayout);
    }
  };

  if (layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(VK_DEVICE, layout, nullptr);
    layout = VK_NULL_HANDLE;
  };

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
  uint32_t pcSize = (uint32_t)(pushConstantRange.size());
  if (pcSize != 0) {
    pipelineLayoutCreateInfo.pushConstantRangeCount = pcSize;
    pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRange.data();
  }else{
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges =  nullptr;
  }

  pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCreateInfo.pSetLayouts =  (pSetLayouts.size()>0)?pSetLayouts.data(): nullptr;
  pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)pSetLayouts.size();
  VK_CHECK(vkCreatePipelineLayout(
      blender::gpu::VKContext::get()->device_get(), &pipelineLayoutCreateInfo, nullptr, &layout));
  return GHOST_kSuccess;
};

void VKShaderInterface::ref_remove(VKVaoCache* ref)
{
  for (int i = 0; i < refs_.size(); i++) {
    if (refs_[i] == ref) {
      refs_[i] = nullptr;
      break; /* cannot have duplicates */
    }
  }
}
void VKShaderInterface::ref_add(VKVaoCache* ref)
{
  for (int i = 0; i < refs_.size(); i++) {
    if (refs_[i] == nullptr) {
      refs_[i] = ref;
      return;
    }
  }
  refs_.append(ref);
}
};  // namespace blender::gpu

namespace spirv_cross {

  CompilerBlender::CompilerBlender(std::vector<uint32_t> &ir,
                                 blender::gpu::VKShaderInterface &iface,
                                 VkShaderStageFlagBits stage)
    : CompilerBlender::CompilerGLSL(ir), iface_(iface), stage_(stage)
{

  resources_ = get_shader_resources();

  auto MAX_NAME_LENGTH =
      [&](SmallVector<Resource> &res, uint32_t &len, uint32_t &max_len, bool push = false) {
        for (const auto &resource : res) {
          if (!push) {
            auto size = resource.name.size();
            BLI_assert(size > 0);
            if (size > max_len) {
              max_len = size;
            };
            len++;
          }
          else {
            const SPIRType spirv_type = this->get_type(resource.type_id);
            size_t num_value = spirv_type.member_types.size();
            for (uint32_t index = 0; index < num_value; ++index) {
              auto size = get_member_name(resource.base_type_id, index).size();
              BLI_assert(size > 0);
              if (size > max_len) {
                max_len = size;
              };
              len++;
            };
          };
        };
      };

  len_ = {0, 0, 0, 0,0};
  max_name_len_ = {0, 0, 0, 0, 0};

  if (stage_ == VK_SHADER_STAGE_VERTEX_BIT)
      MAX_NAME_LENGTH(resources_.stage_inputs, len_.attr, max_name_len_.attr);

  MAX_NAME_LENGTH(resources_.uniform_buffers, len_.ubo, max_name_len_.ubo);
  MAX_NAME_LENGTH(resources_.storage_buffers, len_.ssbo, max_name_len_.ssbo);
  MAX_NAME_LENGTH(resources_.sampled_images, len_.image, max_name_len_.image, false);
  MAX_NAME_LENGTH(resources_.separate_images, len_.image, max_name_len_.image, false);
  MAX_NAME_LENGTH(resources_.separate_samplers, len_.image, max_name_len_.image, false);
  MAX_NAME_LENGTH(resources_.push_constant_buffers, len_.push, max_name_len_.push, true);

  BLI_assert_msg(len_.ubo <= 16, "enabled_ubo_mask_ is uint16_t");
};



  /// <summary>
  /// 
  /// </summary>
  /// <param name="inputs"></param>
  /// <param name="ofs"></param>
  /// <returns>
  /// Nums added to ShaderInput.
  /// </returns>
  int  CompilerBlender::parse_inputs(blender::gpu::ShaderInput *inputs,int ofs )
  {

    iface_.desc_inputs_.resize(iface_.desc_inputs_.size() + 1);
    blender::gpu::VKDescriptorInputs &dinputs = iface_.desc_inputs_.last();

    uint32_t attrN = 0;
    uint32_t stride = 0;
    auto ASSERT_DEVELOP = [&](uint32_t &stride, uint32_t &attrN) {

      for (const auto &resource : resources_.stage_inputs) {
#ifdef _DEBUG
        uint32_t binding_ = get_decoration(resource.id, spv::DecorationBinding);
        BLI_assert(binding_ == 0);
#endif
        const SPIRType spirv_type = get_type(resource.type_id);
        BLI_assert(is_vector(spirv_type));
        BLI_assert(spirv_type.basetype == SPIRType::Float);

        attrN++;
        if (stride == 0)
          stride = spirv_type.width;
        else
          BLI_assert(stride == spirv_type.width);
      };

    };

    ASSERT_DEVELOP(stride, attrN);
    if (attrN == 0)
      return 0;
   


    bool block_attributes = true;
    /*TODO :: instance buffer*/
    dinputs.initialise(attrN, 0, block_attributes);

    attrN = 0;
    int inpN = 0;
    stride    = 0;
    for (const auto &resource : resources_.stage_inputs) {

      VkVertexInputAttributeDescription& attr = dinputs.attributes[attrN];
      attr.location = get_decoration(resource.id, spv::DecorationLocation);
      attr.binding = (block_attributes) ? get_decoration(resource.id, spv::DecorationBinding) : attr.location;
      attr.offset = (block_attributes) ? stride :0;/* get_decoration(resource.id, spv::DecorationOffset); */
      
      const SPIRType spirv_type = get_type(resource.type_id);
      uint32_t stride_attr = 0;
      switch (spirv_type.vecsize) {
        case 1:
          attr.format = VK_FORMAT_R32_SFLOAT;
          stride_attr = 4;
          break;
        case 2:
          attr.format = VK_FORMAT_R32G32_SFLOAT;
          stride_attr = 8;
          break;
        case 3:
          attr.format = VK_FORMAT_R32G32B32_SFLOAT;
          stride_attr = 12;
          break;
        case 4:
          attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
          stride_attr = 16;
          break;
        default:
          BLI_assert(false);
      }
      
      if (!block_attributes) {
        dinputs.append(stride_attr, attrN, true);
      }

      stride += stride_attr;
      attrN++;
      if (!iface_.attr_get(resource.name.data())) {
        iface_.append_input(ofs + inpN++,
                            resource.name.c_str(),
                            resource.name.size(),
                            attr.location,
                            attr.binding,
                            true,
                            false,
                            false);
      };

    };


    if (block_attributes) {
      dinputs.append(stride, 0, true);
    }
    dinputs.finalise();

    return inpN;
  }
 int   CompilerBlender::parse_images(blender::gpu::ShaderInput *inputs,int ofs)
  {

    uint32_t N = 0;
    auto ASSERT_WIP = [&](uint32_t &N, SmallVector<Resource> &res) {

      for (const auto &resource : res) {
#ifdef _DEBUG
        BLI_assert(get_decoration(resource.id, spv::DecorationLocation) == 0);
        const SPIRType spirv_type = get_type(resource.type_id);
        size_t num_value = spirv_type.member_types.size();
        BLI_assert(num_value == 0);
        BLI_assert(get_decoration(resource.id, spv::DecorationDescriptorSet) == 0);
#endif
        N++;
      };

    };


    int apdN = 0;
    auto traverse = [&](SmallVector<Resource> &res, VkDescriptorType dtype, int TID) {
      ASSERT_WIP(N, res);

      for (const auto &resource : res) {
        const SPIRType spirv_type = get_type(resource.type_id);
        uint32_t binding = get_decoration(resource.id, spv::DecorationBinding);
        iface_.append_image(get_decoration(resource.id, spv::DecorationBinding),
                            spirv_type.basetype);
        iface_.append_binding(binding, resource.name.c_str(), dtype,stage_);


        if ( !iface_.uniform_get(resource.name.c_str())) {
          auto& input = inputs[ofs + apdN];
          input.location = 0;
          input.binding = binding;

          bool ima  = false;
          bool tex  = true;
          if (TID == 2) {
            ima = true;
            tex = false;
          }
          iface_.append_input(ofs + apdN++,
                              resource.name.c_str(),
                              resource.name.size(),
                              input.location,
                              input.binding,
                              false,
                              tex,
                              ima);
        };

      }
    };

    traverse(resources_.sampled_images, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0);
    traverse(resources_.separate_images, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1);
    traverse(resources_.separate_samplers, VK_DESCRIPTOR_TYPE_SAMPLER, 2);

    return apdN;
  }

 int CompilerBlender::parse_ubo(blender::gpu::ShaderInput* inputs, int ofs)
 {

   uint32_t N = 0;
   auto ASSERT_WIP = [&](uint32_t& N, SmallVector<Resource>& res) {

     for (const auto& resource : res) {
#ifdef _DEBUG
       BLI_assert(get_decoration(resource.id, spv::DecorationLocation) == 0);
       const SPIRType spirv_type = get_type(resource.type_id);
       auto setNum = get_decoration(resource.id, spv::DecorationDescriptorSet);
       /*No.1 is an alternative to uniform,No.0 is normal ubo*/
       BLI_assert(  setNum <=1 );
       bool is_block = get_decoration_bitset(spirv_type.self).get(spv::DecorationBlock) || get_decoration_bitset(spirv_type.self).get(spv::DecorationBufferBlock);
       bool is_sized_block = is_block && (get_storage_class(resource.id) == spv::StorageClassUniform || get_storage_class(resource.id) == spv::StorageClassUniformConstant);
       BLI_assert(is_sized_block);
       auto ranges = get_active_buffer_ranges(resource.id);

       int i = 0;
       for (auto range : ranges) {
         const  std::string  name = get_member_name(resource.base_type_id, i++);
         printf(" UBO member   ===>   %s     index  %d     offset %zd    range %zd \n", name.c_str(),range.index ,range.offset,range.range);
       }
       if (setNum == 1) {
         BLI_assert(i <= 1);
       };

#endif
       N++;
     };
    };
   int apdN = 0;
   auto traverse = [&](SmallVector<Resource>& res) {

     ASSERT_WIP(N, res);

     for (const auto& resource : res) {
       const SPIRType spirv_type = get_type(resource.type_id);

       uint32_t binding = get_decoration(resource.id, spv::DecorationBinding);
       auto        setNum = get_decoration(resource.id, spv::DecorationDescriptorSet);
       VkDescriptorType dtype = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

       uint32_t block_size = 0;
       {
         auto& base_type = get_type(resource.base_type_id);
         block_size = uint32_t(get_declared_struct_size(base_type));
       }
       uint32_t desc_count = 1;
      
       std::string name = "";
       if (setNum == 1) {
         /*Uniform blocks are prefixed with ubo_.  #VKShader::resources_declare */
         dtype = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
         desc_count = block_size;
         name = get_member_name(resource.base_type_id, 0);
       }
       else {
         dtype = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
         name = resource.name;
       }

       iface_.append_ubo(setNum, binding, block_size);
       iface_.append_binding(binding, name.c_str(), dtype, stage_, desc_count, setNum);



       if (!iface_.ubo_get(name.c_str())) {
         auto& input = inputs[ofs + apdN];
         input.location = setNum;
         input.binding = binding;

         iface_.append_input(ofs + apdN++,
           name.c_str(),
           name.size(),
           input.location,
           input.binding,
           false,
           false,
           false,
           true);
       };

     };
   };

   traverse(resources_.uniform_buffers);


   return apdN;
 }



  int CompilerBlender::parse_pushconst(blender::gpu::ShaderInput *inputs,int ofs)
  {

    uint32_t N = 0;
    auto ASSERT_WIP = [&](uint32_t &N, SmallVector<Resource> &res) {

      for (const auto &resource : res) {
#ifdef _DEBUG
        BLI_assert(get_decoration(resource.id, spv::DecorationLocation) == 0);
        BLI_assert(get_decoration(resource.id, spv::DecorationDescriptorSet) == 0);
#endif
        N++;
      };
      BLI_assert(N == 1);

    };
   VkDeviceSize whole_size= 0;
    int apdN = 0;
    auto traverse = [&](SmallVector<Resource> &res) {
      ASSERT_WIP(N, res);

      const auto &resource = res[0];
      const SPIRType spirv_type = get_type(resource.type_id);
      size_t num_value = spirv_type.member_types.size();
      for (uint32_t index = 0; index < num_value; ++index) {
        auto &name_ = get_member_name(resource.base_type_id, index);
        auto offset = get_member_decoration(resource.base_type_id, index, spv::DecorationOffset);
        if (!iface_.uniform_get(name_.data())) {
          /*  Assign [ offset ] of pushconstants to [ binding ] . */
          iface_.append_input(ofs + apdN++,
                              name_.c_str(),
                              name_.size(),
                              get_member_decoration(resource.base_type_id, index, spv::DecorationLocation),
              offset + blender::gpu::UNIFORM_SORT_OFS,
                              false,
                              false,
                              false);
        };
    
        auto size = get_declared_struct_member_size(spirv_type, index);
        
        //auto arraysize = member_type.array.empty() ? 0 : member_type.array[0];
        whole_size = offset + size;
      };

      if (whole_size > 0)
       iface_.push_range_.stageFlags |= stage_;
     if (iface_.push_range_.size < whole_size)
       iface_.push_range_.size = whole_size;
    };

    traverse(resources_.push_constant_buffers);

    return apdN;
  }

  };
  namespace blender::gpu {
  bool VKShaderInterface::parse(ShaderModule &vcode, ShaderModule &fcode)
  {


  std::vector<uint32_t> Code;
  Code.resize(vcode.shaderModuleInfo.codeSize / 4);
  memcpy(Code.data(), vcode.shaderModuleInfo.pCode, vcode.shaderModuleInfo.codeSize);
  spirv_cross::CompilerBlender vcom(Code, *this, VK_SHADER_STAGE_VERTEX_BIT);
  Code.resize(fcode.shaderModuleInfo.codeSize / 4);
  memcpy(Code.data(), fcode.shaderModuleInfo.pCode, fcode.shaderModuleInfo.codeSize);
  spirv_cross::CompilerBlender fcom(Code, *this, VK_SHADER_STAGE_FRAGMENT_BIT);


  apply(&vcom, &fcom);


  this->sort_inputs();


  this->sortUniformLocation();



  /* Resize name buffer to save some memory. */
  if (name_buffer_offset_ <  name_buffer_len_) {
    name_buffer_ = (char *)MEM_reallocN(name_buffer_, name_buffer_offset_);
  }

    /* Builtin Uniforms */
  for (int32_t u_int = 0; u_int < GPU_NUM_UNIFORMS; u_int++) {
    GPUUniformBuiltin u = static_cast<GPUUniformBuiltin>(u_int);
    const ShaderInput *uni = this->uniform_get(builtin_uniform_name(u));
    builtins_[u] = (uni != nullptr) ? uni->location : -1;
  }

  /* Builtin Uniforms Blocks */
  for (int32_t u_int = 0; u_int < GPU_NUM_UNIFORM_BLOCKS; u_int++) {
    GPUUniformBlockBuiltin u = static_cast<GPUUniformBlockBuiltin>(u_int);
    const ShaderInput *block = this->ubo_get(builtin_uniform_block_name(u));
    builtin_blocks_[u] = (block != nullptr) ? block->binding : -1;
  }

  /* Builtin Storage Buffers */
  for (int32_t u_int = 0; u_int < GPU_NUM_STORAGE_BUFFERS; u_int++) {
    GPUStorageBufferBuiltin u = static_cast<GPUStorageBufferBuiltin>(u_int);
    const ShaderInput *block = this->ssbo_get(builtin_storage_block_name(u));
    builtin_buffers_[u] = (block != nullptr) ? block->binding : -1;
  }
  
  
  return true;
}
}  // namespace blender::gpu
