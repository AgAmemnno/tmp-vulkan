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
  Vector<VKVertBuf *>                                 vbos;
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
};
};  // namespace gpu
};  // namespace blender

#include "vk_shader_interface.hh"

namespace blender {
namespace gpu {

namespace VKVertArray {


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
void update_bindings(VKVao &vao,
                     uint v_first,
                     const GPUVertFormat *format,
                     const ShaderInterface *interface);

}  // namespace GLVertArray

}  // namespace gpu
}  // namespace blender
