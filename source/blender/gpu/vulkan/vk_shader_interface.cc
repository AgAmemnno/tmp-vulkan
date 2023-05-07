/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. */

/** \file
 * \ingroup gpu
 */

#include "vk_shader_interface.hh"
#include "vk_context.hh"

namespace blender::gpu {

void VKShaderInterface::init(const shader::ShaderCreateInfo &info)
{

  static char PUSH_CONSTANTS_FALLBACK_NAME[] = "push_constants_fallback";
  static size_t PUSH_CONSTANTS_FALLBACK_NAME_LEN = strlen(PUSH_CONSTANTS_FALLBACK_NAME);


  using namespace blender::gpu::shader;
  uint ubo_push_len_ = 0;
  uint push_len_ = info.push_constants_.size();
  attr_len_ = info.vertex_inputs_.size();
  uniform_len_ = push_len_;
  ssbo_len_ = 0;
  ubo_len_ = 0;
  image_offset_ = -1;

  Vector<const ShaderCreateInfo::Resource *> all_resources;


  for (int i = 0; i < info.pass_resources_.size(); i++) {
    all_resources.append(&info.pass_resources_[i]);
  }
  for (int i = 0; i < info.batch_resources_.size(); i++) {
    all_resources.append(&info.batch_resources_[i]);
  }

  /* Images are treated as descriptor sets, so they are counted separately. #uniform_image_len_ */
  for (const ShaderCreateInfo::Resource *res : all_resources) {
    switch (res->bind_type) {
      case ShaderCreateInfo::Resource::BindType::IMAGE:
        uniform_len_++;
        break;
      case ShaderCreateInfo::Resource::BindType::SAMPLER:
        image_offset_ = max_ii(image_offset_, res->slot);
        uniform_len_++;
        break;
      case ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
        ubo_len_++;
        break;
      case ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
        ssbo_len_++;
        break;
    }
  }

  /* Reserve 1 uniform buffer for push constants fallback. */
  /* Access to push constants is never done as a uniform block. So we don't need to count it in
   * ubo_len.*/
  size_t names_size = info.interface_names_size_;
  VKContext &context = *VKContext::get();
  const VKPushConstants::StorageType push_constants_storage_type =
      VKPushConstants::Layout::determine_storage_type(info, context.physical_device_limits_get());
  if (push_constants_storage_type == VKPushConstants::StorageType::UNIFORM_BUFFER) {
    names_size += PUSH_CONSTANTS_FALLBACK_NAME_LEN + 1;
  }

  /* Make sure that the image slots don't overlap with the sampler slots. */
  image_offset_++;

  int32_t input_tot_len = attr_len_ + ubo_len_ + uniform_len_ + ssbo_len_;
  inputs_ = static_cast<ShaderInput *>(
      MEM_calloc_arrayN(input_tot_len, sizeof(ShaderInput), __func__));
  ShaderInput *input = inputs_;

  name_buffer_ = (char *)MEM_mallocN(names_size, "name_buffer");
  uint32_t name_buffer_offset = 0;

  /* Attributes */
  for (const ShaderCreateInfo::VertIn &attr : info.vertex_inputs_) {
    copy_input_name(input, attr.name, name_buffer_, name_buffer_offset);
    input->location = input->binding = attr.index;
    if (input->location != -1) {
      enabled_attr_mask_ |= (1 << input->location);

      /* Used in `GPU_shader_get_attribute_info`. */
      attr_types_[input->location] = uint8_t(attr.type);
    }

    input++;
  }
  /* Ubos */
  for (const ShaderCreateInfo::Resource *res : all_resources) {
    input->binding = res->slot;
    switch (res->bind_type) {
        case ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
          copy_input_name(input, res->image.name, name_buffer_, name_buffer_offset);
          input->location = buffer_set_;
          break;
        default:
          continue;
      }
    descriptor_set_locations_[input->location][input->binding] = res;
    input++;
  }
  /* Images and Samplers */
  for (const ShaderCreateInfo::Resource *res : all_resources) {
    input->binding = res->slot;
    switch (res->bind_type) {
        case ShaderCreateInfo::Resource::BindType::IMAGE:
          copy_input_name(input, res->image.name, name_buffer_, name_buffer_offset);
          input->location = image_set_;
          break;
        case ShaderCreateInfo::Resource::BindType::SAMPLER:
          copy_input_name(input, res->sampler.name, name_buffer_, name_buffer_offset);
          input->location =  image_set_;
          break;
        default:
          continue;
      }
    descriptor_set_locations_[input->location][input->binding] = res;
    input++;
  }

  /* Push constants. */
  ubo_push_len_ = (int)(push_constants_storage_type == VKPushConstants::StorageType::UNIFORM_BUFFER);
  int32_t push_constant_location = 1024;
  for (const ShaderCreateInfo::PushConst &push_constant : info.push_constants_) {
    copy_input_name(input, push_constant.name, name_buffer_, name_buffer_offset);
    input->location = push_constant_location++;
    input->binding = -1;
    input++;
  }
  /* SSBOs */
  for (const ShaderCreateInfo::Resource *res : all_resources) {
  input->binding = res->slot;
  switch (res->bind_type) {
      case ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
        copy_input_name(input, res->storagebuf.name, name_buffer_, name_buffer_offset);
        input->location = buffer_set_;
        break;
      default:
        continue;
    }
  descriptor_set_locations_[input->location][input->binding] = res;
  input++;
}
  sort_inputs();
   /* Builtin Uniforms */
  for (int32_t u_int = 0; u_int < GPU_NUM_UNIFORMS; u_int++) {
    GPUUniformBuiltin u = static_cast<GPUUniformBuiltin>(u_int);
    const ShaderInput *uni = this->uniform_get(builtin_uniform_name(u));
    builtins_[u] = (uni != nullptr) ? ( ( uni->binding == -1 ) ? uni->location : uni->binding ) : -1;
  }

  /* Builtin Uniforms Blocks */
  for (int32_t u_int = 0; u_int < GPU_NUM_UNIFORM_BLOCKS; u_int++) {
    GPUUniformBlockBuiltin u = static_cast<GPUUniformBlockBuiltin>(u_int);
    const ShaderInput *block = this->ubo_get(builtin_uniform_block_name(u));
    builtin_blocks_[u] = (block != nullptr) ? block->binding : -1;
  }

  /* Post initializing push constants. */
  /* Determine the binding location of push constants fallback buffer. */
  
  if (push_constants_storage_type == VKPushConstants::StorageType::UNIFORM_BUFFER) {
    descriptor_set_locations_[push_constant_buffer_set_][0]  = nullptr;
  }
  push_constants_layout_.init(info, *this, push_constants_storage_type, VKDescriptorSet::Location( (ubo_push_len_==0)?-1:push_constant_buffer_set_,0));
}

static int32_t shader_input_index(const ShaderInput *shader_inputs,
                                  const ShaderInput *shader_input)
{
  int32_t index = (shader_input - shader_inputs);
  return index;
}

void VKShaderInterface::descriptor_set_location_update(
    const shader::ShaderCreateInfo::Resource */*resource*/, const VKDescriptorSet::Location /*location*/)
{
   BLI_assert(false);
}

/* This is a redundant function. */
const VKDescriptorSet::Location VKShaderInterface::descriptor_set_location(int slot) const
{
  BLI_assert(false);
  return slot;
}

const VKDescriptorSet::Location VKShaderInterface::descriptor_set_location(
    const shader::ShaderCreateInfo::Resource *resource) const
{ 
  for (int i =0;i<3;i++)
  {
    const auto set = descriptor_set_locations_[i];
    for (int binding =0;binding<16;binding++) {
    {
      const auto res = set[binding];
      if (res == resource) {
        return VKDescriptorSet::Location(i,binding);
      };
    }
  }
  }
  /*not found*/
  BLI_assert(false);
  return 0;
}
/** Now only set number 0 is used. So the binding number is a unique integer. This function was not
 *used. For example, this function makes sense if we associate a bind type with a set number.
 **/
const VKDescriptorSet::Location VKShaderInterface::descriptor_set_location(
    const shader::ShaderCreateInfo::Resource::BindType &bind_type, int slot) const
{
  switch (bind_type) {
      case shader::ShaderCreateInfo::Resource::BindType::IMAGE:
      case shader::ShaderCreateInfo::Resource::BindType::SAMPLER:
          return VKDescriptorSet::Location(image_set_,slot);
        break;
      case shader::ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
      case shader::ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
          return VKDescriptorSet::Location(buffer_set_,slot);
        break;
      default:
        BLI_assert_unreachable();
        break;
    }

  BLI_assert_unreachable();
  return VKDescriptorSet::Location(-1,-1);
}

const ShaderInput *VKShaderInterface::shader_input_get(
    const shader::ShaderCreateInfo::Resource &resource) const
{
  return shader_input_get(resource.bind_type, resource.slot);
}

const ShaderInput *VKShaderInterface::shader_input_get(
    const shader::ShaderCreateInfo::Resource::BindType &bind_type, int binding) const
{

  switch (bind_type) {
    case shader::ShaderCreateInfo::Resource::BindType::IMAGE:
      return texture_get(binding + image_offset_);
    case shader::ShaderCreateInfo::Resource::BindType::SAMPLER:
      return texture_get(binding);
    case shader::ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
      return ssbo_get(binding);
    case shader::ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
      return ubo_get(binding);
  }
  return nullptr;
}

}  // namespace blender::gpu
