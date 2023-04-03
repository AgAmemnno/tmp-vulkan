/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_batch.h"
#include "vk_vertex_buffer.hh"

namespace blender {
namespace gpu {

struct VKVao {
  bool is_valid = false;
  VkPipelineVertexInputStateCreateInfo info = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL};

  VkPipelineVertexInputDivisorStateCreateInfoEXT divisorInfo = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT, NULL};

  Vector<VkVertexInputBindingDivisorDescriptionEXT> divisors;
  Vector<VkVertexInputBindingDescription> bindings;
  Vector<VkVertexInputAttributeDescription> attributes;
  Vector<VKVertexBuffer *> vbos;
  VKVao()
  {
    clear();
  };
  void clear()
  {
    is_valid = false;
    info.pNext = NULL;  //&divisorInfo;
    divisorInfo.vertexBindingDivisorCount = 0;
    divisorInfo.pVertexBindingDivisors = NULL;
    bindings.clear();
    attributes.clear();
    vbos.clear();
  }
  // Copy assignment operator.
  VKVao &operator=(const VKVao &other)
  {
    if (this != &other) {
      is_valid = other.is_valid;
      info = other.info;
      bindings.resize(other.bindings.size());
      int i = 0;
      for (auto e : other.bindings) {
        bindings[i++] = e;
      }
      i = 0;
      attributes.resize(other.attributes.size());
      for (auto e : other.attributes) {
        attributes[i++] = e;
      }
      i = 0;
      vbos.resize(other.vbos.size());
      for (auto e : other.vbos) {
        vbos[i++] = e;
      }
    }
    return *this;
  }
};
};  // namespace gpu
};  // namespace blender

#include "vk_shader_interface.hh"

namespace blender {
namespace gpu {

namespace VKVertArray {
struct VKVertexInput {
  VkPipelineVertexInputStateCreateInfo info = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL};

  VkPipelineVertexInputDivisorStateCreateInfoEXT divisorInfo = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT, NULL};

  Vector<VkVertexInputBindingDivisorDescriptionEXT> divisors;
  Vector<VkVertexInputBindingDescription> vertexInputBindings;
  Vector<VkVertexInputAttributeDescription> vertexInputAttributes;

  VKVertexInput()
  {
    info.pNext = &divisorInfo;
    divisorInfo.vertexBindingDivisorCount = 0;
    divisorInfo.pVertexBindingDivisors = NULL;
  }

  void clear(){
    vertexInputBindings.clear();
    vertexInputAttributes.clear();
    divisors.clear();
  };

  void append(VkVertexInputBindingDescription& bind){
     vertexInputBindings.append(bind);
  }

  void append(VkVertexInputAttributeDescription& bind){
     vertexInputAttributes.append(bind);
  }
  VkPipelineVertexInputStateCreateInfo& get() {
    info.flags = 0;
    info.pNext = VK_NULL_HANDLE;
    info.pVertexAttributeDescriptions = vertexInputAttributes.data();
    info.vertexAttributeDescriptionCount = vertexInputAttributes.size();
    info.pVertexBindingDescriptions = vertexInputBindings.data();
    info.vertexBindingDescriptionCount = vertexInputBindings.size();
    return info;
  }


};
/**
 * Update the Attribute Binding of the currently bound VAO.
 */
void update_bindings(VKVao &vao,
                     const GPUBatch *batch,
                     const ShaderInterface *interface,
                     int base_instance);

/**
 * Another version of update_bindings for Immediate mode.
 */
void update_bindings(VKVertexInput& input,
                     uint v_first,
                     const GPUVertFormat *format,
                     const ShaderInterface *interface);

}  // namespace VKVertArray

}  // namespace gpu
}  // namespace blender
