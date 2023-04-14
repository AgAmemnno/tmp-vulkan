/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "vk_uniform_buffer.hh"
#include "vk_context.hh"
#include "vk_descriptor_set.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"

namespace blender::gpu {

void VKUniformBuffer::update(const void *data)
{
  if (!buffer_.is_allocated()) {
    VKContext &context = *VKContext::get();
    allocate(context);
  }
  buffer_.update(data);
}

void VKUniformBuffer::allocate(VKContext &context)
{
  buffer_.create(context, size_in_bytes_, GPU_USAGE_STATIC, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
}

void VKUniformBuffer::clear_to_zero() {}

void VKUniformBuffer::bind(int slot)
{
  VKContext &context = *VKContext::get();
  if (!buffer_.is_allocated()) {
    allocate(context);
  }
  VKShader *shader = static_cast<VKShader *>(context.shader);
  const VKShaderInterface &shader_interface = shader->interface_get();
  const VKDescriptorSet::Location binding = shader_interface.descriptor_set_location(slot);
  shader->pipeline_get().descriptor_set_get().bind(*this, binding);
}

void VKUniformBuffer::bind_as_ssbo(int /*slot*/) {}

void VKUniformBuffer::unbind() {}

}  // namespace blender::gpu
