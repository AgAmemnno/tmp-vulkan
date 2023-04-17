/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "vk_descriptor_set.hh"
#include "vk_index_buffer.hh"
#include "vk_shader.hh"
#include "vk_storage_buffer.hh"
#include "vk_texture.hh"
#include "vk_uniform_buffer.hh"
#include "vk_vertex_buffer.hh"

#include "BLI_assert.h"

namespace blender::gpu {

VKDescriptorSet::VKDescriptorSet(VKDescriptorSet &&other)
    : vk_descriptor_pool_(other.vk_descriptor_pool_), vk_descriptor_set_(other.vk_descriptor_set_)
{
  other.mark_freed();
}

VKDescriptorSet::~VKDescriptorSet()
{
  if (vk_descriptor_set_ != VK_NULL_HANDLE) {
    /* Handle should be given back to the pool. */
    VKContext &context = *VKContext::get();
    context.descriptor_pools_get().free(*this);
    BLI_assert(vk_descriptor_set_ == VK_NULL_HANDLE);
  }
}

void VKDescriptorSet::mark_freed()
{
  vk_descriptor_set_ = VK_NULL_HANDLE;
  vk_descriptor_pool_ = VK_NULL_HANDLE;
}

void VKDescriptorSetTracker::bind(VKStorageBuffer &buffer,
                                  const VKDescriptorSet::Location location)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  binding.vk_buffer = buffer.vk_handle();
  binding.buffer_size = buffer.size_in_bytes();
}

void VKDescriptorSetTracker::bind_as_ssbo(VKVertexBuffer &buffer,
                                          const VKDescriptorSet::Location location)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  binding.vk_buffer = buffer.vk_handle();
  binding.buffer_size = buffer.size_used_get();
}

void VKDescriptorSetTracker::bind(VKUniformBuffer &buffer,
                                  const VKDescriptorSet::Location location)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  binding.vk_buffer = buffer.vk_handle();
  binding.buffer_size = buffer.size_in_bytes();
}

void VKDescriptorSetTracker::bind_as_ssbo(VKIndexBuffer &buffer,
                                          const VKDescriptorSet::Location location)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  binding.vk_buffer = buffer.vk_handle();
  binding.buffer_size = buffer.size_get();
}

void VKDescriptorSetTracker::texture_bind(VKTexture &texture,
                                          VKDescriptorSet::Location location,
                                          const GPUSamplerState &sampler_type)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binding.vk_image_view = texture.vk_image_view_handle();
  binding.vk_sampler = VKTexture::get_sampler(sampler_type);
}

void VKDescriptorSetTracker::image_bind(VKTexture &texture,
                                        const VKDescriptorSet::Location location)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  binding.vk_image_view = texture.vk_image_view_handle();
  binding.vk_sampler = VK_NULL_HANDLE;
}

VKDescriptorSetTracker::Binding &VKDescriptorSetTracker::ensure_location(
    const VKDescriptorSet::Location location)
{
  for (Binding &binding : bindings_) {
    if (binding.location == location) {
      return binding;
    }
  }

  Binding binding = {};
  binding.location = location;
  bindings_.append(binding);
  return bindings_.last();
}

void VKDescriptorSetTracker::bindcmd(VKCommandBuffer &command_buffer,
                                     VkPipelineLayout vk_pipeline_layout)
{
  std::unique_ptr<VKDescriptorSet> &descriptor_set = active_descriptor_set();

  command_buffer.bind(*descriptor_set.get(), vk_pipeline_layout, VK_PIPELINE_BIND_POINT_GRAPHICS);
}

bool VKDescriptorSetTracker::update(VKContext &context)
{
  bool bindings_exist = bindings_.size() > 0;
  if(!bindings_exist){
    return false;
  }
  tracked_resource_for(context, !bindings_.is_empty());
  std::unique_ptr<VKDescriptorSet> &descriptor_set = active_descriptor_set();
  VkDescriptorSet vk_descriptor_set = descriptor_set->vk_handle();

  Vector<VkDescriptorBufferInfo> buffer_infos;
  Vector<VkWriteDescriptorSet> descriptor_writes;

  for (const Binding &binding : bindings_) {
    if (!binding.is_buffer()) {
      continue;
    }
    VkDescriptorBufferInfo buffer_info = {};
    buffer_info.buffer = binding.vk_buffer;
    buffer_info.range = binding.buffer_size;
    buffer_infos.append(buffer_info);

    VkWriteDescriptorSet write_descriptor = {};
    write_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor.dstSet = vk_descriptor_set;
    write_descriptor.dstBinding = binding.location;
    write_descriptor.descriptorCount = 1;
    write_descriptor.descriptorType = binding.type;
    write_descriptor.pBufferInfo = &buffer_infos.last();
    descriptor_writes.append(write_descriptor);
  }

  Vector<VkDescriptorImageInfo> image_infos;
  for (const Binding &binding : bindings_) {
    if (!binding.is_image() && !binding.is_texture()) {
      continue;
    }

    bool tex_t = binding.is_texture();

    VkDescriptorImageInfo image_info = {};
    image_info.imageView = binding.vk_image_view;
    image_info.sampler = binding.vk_sampler;
    image_info.imageLayout = (tex_t) ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL :
                                       VK_IMAGE_LAYOUT_GENERAL;
    image_infos.append(image_info);

    VkWriteDescriptorSet write_descriptor = {};
    write_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor.dstSet = vk_descriptor_set;
    write_descriptor.dstBinding = binding.location;
    write_descriptor.descriptorCount = 1;
    write_descriptor.descriptorType = binding.type;
    write_descriptor.pImageInfo = &image_infos.last();
    descriptor_writes.append(write_descriptor);
  }

  BLI_assert_msg(image_infos.size() + buffer_infos.size() == descriptor_writes.size(),
                 "Not all changes have been converted to a write descriptor. Check "
                 "`Binding::is_buffer` and `Binding::is_image`.");

  VkDevice vk_device = context.device_get();
  vkUpdateDescriptorSets(
      vk_device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);

  bindings_.clear();
   return true;
}

std::unique_ptr<VKDescriptorSet> VKDescriptorSetTracker::create_resource(VKContext &context)
{
  return context.descriptor_pools_get().allocate(layout_);
}

}  // namespace blender::gpu
