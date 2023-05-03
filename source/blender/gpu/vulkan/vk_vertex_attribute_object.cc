/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

#include "vk_vertex_attribute_object.hh"

#include "vk_batch.hh"
#include "vk_context.hh"
#include "vk_immediate.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_vertex_buffer.hh"

#include "BLI_array.hh"

namespace blender::gpu {
VKVertexAttributeObject::VKVertexAttributeObject()
{
  clear();
}

void VKVertexAttributeObject::clear()
{
  is_valid = false;
  info.pNext = NULL;
  bindings.clear();
  attributes.clear();
  vbos.clear();
  buffers.clear();
}

VKVertexAttributeObject &VKVertexAttributeObject::operator=(const VKVertexAttributeObject &other)
{
  if (this == &other) {
    return *this;
  }

  is_valid = other.is_valid;
  info = other.info;
  bindings.clear();
  bindings.extend(other.bindings);
  attributes.clear();
  attributes.extend(other.attributes);
  vbos.clear();
  vbos.extend(other.vbos);
  buffers.clear();
  buffers.extend(other.buffers);
  return *this;
}

void VKVertexAttributeObject::bind(VKContext &context)
{
  Array<bool> visited_bindings(bindings.size());
  visited_bindings.fill(false);

  for (VkVertexInputAttributeDescription attribute : attributes) {
    if (visited_bindings[attribute.binding]) {
      continue;
    }
    visited_bindings[attribute.binding] = true;

    /* Bind VBOS from batches. */
    if (attribute.binding < vbos.size()) {
      BLI_assert(vbos[attribute.binding]);
      VKVertexBuffer &vbo = *vbos[attribute.binding];
      vbo.upload();
      context.command_buffer_get().bind(attribute.binding, vbo, 0);
    }

    /* Bind dynamic buffers from immediate mode. */
    if (attribute.binding < buffers.size()) {
      BLI_assert(buffers[attribute.binding]);
      VKBuffer &buffer = *buffers[attribute.binding];
      context.command_buffer_get().bind(attribute.binding, buffer.vk_handle(), 0);
    }
  }
}

void VKVertexAttributeObject::update_bindings(const VKContext &context, VKBatch &batch)
{
  clear();
  const VKShaderInterface &interface = unwrap(context.shader)->interface_get();
  AttributeMask occupied_attributes = 0;

  for (int v = 0; v < GPU_BATCH_INST_VBO_MAX_LEN; v++) {
    VKVertexBuffer *vbo = batch.instance_buffer_get(v);
    if (vbo) {
      update_bindings(
          vbo->format, vbo, nullptr, vbo->vertex_len, interface, occupied_attributes, true);
    }
  }

  for (int v = 0; v < GPU_BATCH_VBO_MAX_LEN; v++) {
    VKVertexBuffer *vbo = batch.vertex_buffer_get(v);
    if (vbo) {
      update_bindings(vbo->format, vbo, nullptr, vbo->vertex_len, interface, occupied_attributes, false);
    }
  }
  
  is_valid = true;

  //BLI_assert(interface.enabled_attr_mask_ == occupied_attributes);
  auto attr_mask = interface.enabled_attr_mask_ ^ occupied_attributes;
  if (attr_mask !=0) {
    VKVertexBuffer* vbo_dummy = VKContext::get()->default_vbo_dummy;
    for (uint16_t mask = 1, a = 0; a < 16; a++, mask <<= 1) {
      if (attr_mask & mask) {
        VkVertexInputBindingDescription vk_binding = {};
        VkVertexInputAttributeDescription vk_attribute = {};
        vk_attribute.location = a;
        vk_attribute.offset   = 0;
        vk_attribute.format  = VK_FORMAT_R32_SFLOAT;
        vk_binding.binding     = vk_attribute.binding  = bindings.size();
        vk_binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        vk_binding.stride = sizeof(float);
        bindings.append(vk_binding);
        vbos.append(vbo_dummy);
        attributes.append(vk_attribute);
      }
     }
  }

}

void VKVertexAttributeObject::update_bindings(VKImmediate &immediate)
{
  clear();
  const VKShaderInterface &interface = unwrap(unwrap(immediate.shader))->interface_get();
  AttributeMask occupied_attributes = 0;

  update_bindings(immediate.vertex_format,
                  nullptr,
                  &immediate.buffer_,
                  immediate.vertex_len,
                  interface,
                  occupied_attributes,
                  false);
  is_valid = true;
  BLI_assert(interface.enabled_attr_mask_ == occupied_attributes);
}

void VKVertexAttributeObject::update_bindings(const GPUVertFormat &vertex_format,
                                              VKVertexBuffer *vertex_buffer,
                                              VKBuffer *immediate_vertex_buffer,
                                              const int64_t vertex_len,
                                              const VKShaderInterface &interface,
                                              AttributeMask &r_occupied_attributes,
                                              const bool use_instancing)
{
  BLI_assert(vertex_buffer || immediate_vertex_buffer);
  BLI_assert(!(vertex_buffer && immediate_vertex_buffer));

  if (vertex_format.attr_len <= 0) {
    return;
  }

  uint32_t offset = 0;
  uint32_t stride = vertex_format.stride;

  /** When there is only one buffer, this method uses multiple bindings by offsetting them.
  *     In other words, the number of locations == the number of bindings.
  *     Multiple vertex-buffers are not supported.
  **/
  
  uint32_t binding = bindings.size() -1;

  for (uint32_t attribute_index = 0; attribute_index < vertex_format.attr_len; attribute_index++) {
    const GPUVertAttr &attribute = vertex_format.attrs[attribute_index];
    if (vertex_format.deinterleaved) {
      offset += ((attribute_index == 0) ? 0 : vertex_format.attrs[attribute_index - 1].size) *
                vertex_len;
      stride = attribute.size;
    }
    else {
      offset = attribute.offset;
    }
    bool attribute_used_by_shader = false;
    for (uint32_t name_index = 0; name_index < attribute.name_len; name_index++) {
      const char *name = GPU_vertformat_attr_name_get(&vertex_format, &attribute, name_index);
      const ShaderInput *shader_input = interface.attr_get(name);
      if (shader_input == nullptr || shader_input->location == -1) {
        continue;
      }

      /* Don't overwrite attributes that are already occupied. */
      AttributeMask attribute_mask = 1 << shader_input->location;
      if (r_occupied_attributes & attribute_mask) {
        continue;
      }
      r_occupied_attributes |= attribute_mask;
      attribute_used_by_shader = true;

      VkVertexInputAttributeDescription attribute_description = {};
      attribute_description.binding  = binding+1;
      attribute_description.location = shader_input->location;
      attribute_description.offset   = offset;
      attribute_description.format  = to_vk_format(static_cast<GPUVertCompType>(attribute.comp_type), attribute.size,(GPUVertFetchMode)attribute.fetch_mode);
      if(attribute.comp_len == 16){
        for(uint32_t  j =0;j<4;j++)
        {
          attribute_description.location = shader_input->location + j;
          attribute_description.offset   = offset +  j*(uint32_t)attribute.size/4;
          attributes.append(attribute_description);
        }
      }else{
        attributes.append(attribute_description);
      }

    }
    if (attribute_used_by_shader) {
      VkVertexInputBindingDescription vk_binding_descriptor = {};
      vk_binding_descriptor.binding = ++binding;
      vk_binding_descriptor.stride   = stride;
      vk_binding_descriptor.inputRate = use_instancing ? VK_VERTEX_INPUT_RATE_INSTANCE :
                                                         VK_VERTEX_INPUT_RATE_VERTEX;
      bindings.append(vk_binding_descriptor);
      if (vertex_buffer) {
        vbos.append(vertex_buffer);
      }
      if (immediate_vertex_buffer) {
        buffers.append(immediate_vertex_buffer);
      }
    }
  }
}

}  // namespace blender::gpu
