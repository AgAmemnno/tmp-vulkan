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
    : vk_descriptor_pool_(other.vk_descriptor_pool_), vk_descriptor_set_(other.vk_descriptor_set_),set_location_(other.set_location_)
{
  other.mark_freed();

}

VKDescriptorSet::~VKDescriptorSet()
{
  if (vk_descriptor_set_ != VK_NULL_HANDLE) {
    /* Handle should be given back to the pool. */
    VKBackend::get().descriptor_pools_get().free(*this);
    BLI_assert(vk_descriptor_set_ == VK_NULL_HANDLE);
  }
}

void VKDescriptorSet::mark_freed()
{
  set_location_ = 0;
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
  printf("UNIFORM BUFFER Write OUT  VkBuffer (%llx)  binding (%d)  SHADER  %s \n", (uintptr_t)binding.vk_buffer, (int)location ,shader->name_get());
  if(std::string(shader->name_get()) =="workbench_opaque_mesh_tex_none_no_clip"){
    printf("");
  }
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
  binding.vk_image_view = texture.vk_image_view_for_descriptor();
  binding.vk_sampler = VKTexture::get_sampler(sampler_type);
  if(location == VKDescriptorSet::Location(4)){
    printf("");
  }
  printf("IMAGE SAMPLER Write OUT  view (%llx)  binding (%d)  SHADER  %s \n",(uintptr_t)binding.vk_image_view, (int)location ,shader->name_get());
}

void VKDescriptorSetTracker::image_bind(VKTexture &texture,
                                        const VKDescriptorSet::Location location)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  binding.vk_image_view = texture.vk_image_view_for_descriptor();
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
                                     VkPipelineLayout vk_pipeline_layout,VkPipelineBindPoint bind_point)
{
  for(int i=0;i<3;i++)
  {
    if(bound_set_[i] != VK_NULL_HANDLE)
    {
      command_buffer.bind(bound_set_[i], i,vk_pipeline_layout, bind_point);
      last_bound_set_ = true;
    }
  }
}

bool VKDescriptorSetTracker::update(VKContext &context)
{

  bool bindings_exist = bindings_.size() > 0;
  if(!bindings_exist){
    if( last_bound_set_){
      return true;
    }
    return false;
  }

  VkDescriptorSet vk_descriptor_set[3];
  for(int i=0;i<3;i++)
  {
    if(layout_[i]!= VK_NULL_HANDLE){
     vk_descriptor_set[i] =  tracked_resource_for(context, !bindings_.is_empty(),i)->vk_handle();
    }
  }
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
    uint32_t set_location = binding.location.get_set();

    VkWriteDescriptorSet write_descriptor = {};
    write_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor.dstSet = vk_descriptor_set[set_location];
    write_descriptor.dstBinding = binding.location;
    write_descriptor.descriptorCount = 1;
    write_descriptor.descriptorType = binding.type;
    write_descriptor.pBufferInfo = &buffer_infos.last();
    descriptor_writes.append(write_descriptor);
    bound_set_[set_location] = write_descriptor.dstSet;

  }

  Vector<VkDescriptorImageInfo> image_infos;
  for (const Binding &binding : bindings_) {
    if (!binding.is_image() && !binding.is_texture()) {
      continue;
    }

    bool tex_t = binding.is_texture();
    uint32_t set_location = binding.location.get_set();
    VkDescriptorImageInfo image_info = {};
    image_info.imageView = binding.vk_image_view;
    image_info.sampler = binding.vk_sampler;
    image_info.imageLayout = (tex_t) ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL :
                                       VK_IMAGE_LAYOUT_GENERAL;
    image_infos.append(image_info);

    VkWriteDescriptorSet write_descriptor = {};
    write_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor.dstSet = vk_descriptor_set[set_location];
    write_descriptor.dstBinding = binding.location;
    write_descriptor.descriptorCount = 1;
    write_descriptor.descriptorType = binding.type;
    write_descriptor.pImageInfo = &image_infos.last();
    descriptor_writes.append(write_descriptor);
    bound_set_[set_location] = write_descriptor.dstSet;
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

std::unique_ptr<VKDescriptorSet> VKDescriptorSetTracker::create_resource(VKContext &/*context*/,int i)
{
  return VKBackend::get().descriptor_pools_get().allocate(layout_[i], i);
}

std::unique_ptr<VKDescriptorSet> VKDescriptorSetTracker::create_resource(VKContext &/*context*/)
{
  return VKBackend::get().descriptor_pools_get().allocate(layout_[0], 0);
}
}  // namespace blender::gpu
