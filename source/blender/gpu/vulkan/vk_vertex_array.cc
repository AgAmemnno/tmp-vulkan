/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 by Mike Erwin. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "gpu_shader_interface.hh"
#include "gpu_vertex_buffer_private.hh"

#include "vk_batch.hh"
#include "vk_context.hh"
#include "vk_index_buffer.hh"
/*TODO :: #include "vk_storage_buffer.hh" */
#include "vk_vertex_buffer.hh"

#include "vk_framebuffer.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_vertex_array.hh"

namespace blender::gpu {
/* -------------------------------------------------------------------- */
/** \name Vertex Array Bindings
 * \{ */

/** Returns enabled vertex pointers as a bit-flag (one bit per attribute). */
static uint16_t vbo_bind(VKVertArray::VKVertexInput &vk_input,
                         const ShaderInterface *interface,
                         const GPUVertFormat *format,
                         uint /*v_first*/,
                         uint v_len,
                         const bool use_instancing)
{
  vk_input.clear();

  uint16_t enabled_attrib = 0;
  const uint attr_len = format->attr_len;
  uint stride = format->stride;
  uint offset = 0;

  VkVertexInputBindingDescription vk_bindings;
  vk_bindings.binding = 0;
  vk_bindings.inputRate = (use_instancing) ? VK_VERTEX_INPUT_RATE_INSTANCE :
                                             VK_VERTEX_INPUT_RATE_VERTEX;
  vk_bindings.stride = 0;

  VkVertexInputAttributeDescription vk_attrib;
  vk_attrib.binding = 0;
  // VkVertexInputBindingDivisorDescriptionEXT divisors;

  for (uint a_idx = 0; a_idx < attr_len; a_idx++) {
    const GPUVertAttr *a = &format->attrs[a_idx];

    if (format->deinterleaved) {
      offset += ((a_idx == 0) ? 0 : format->attrs[a_idx - 1].size) * v_len;
      stride = a->size;
    }
    else {
      offset = a->offset;
    }

    vk_attrib.offset = offset;

    /* This is in fact an offset in memory. */
    // const GLvoid *pointer = (const GLubyte *)intptr_t(offset + v_first * stride);
    const VkFormat type = to_vk(static_cast<GPUVertCompType>(a->comp_type), a->size);
    vk_attrib.format = type;

    for (uint n_idx = 0; n_idx < a->name_len; n_idx++) {
      const char *name = GPU_vertformat_attr_name_get(format, a, n_idx);
      const ShaderInput *input = interface->attr_get(name);

      if (input == nullptr || input->location == -1) {
        continue;
      }

      vk_attrib.location = input->location;

      enabled_attrib |= (1 << input->location);

      vk_input.append(vk_attrib);
      vk_bindings.stride += a->size;
    }
  }

  if (enabled_attrib) {
    vk_input.append(vk_bindings);
  }

  return enabled_attrib;
}

void VKVertArray::update_bindings(VKVertexInput &input,
                                  const uint v_first,
                                  const GPUVertFormat *format,
                                  const ShaderInterface *interface)
{
  /*#glBindVertexArray(vao);*/
  vbo_bind(input, interface, format, v_first, 0, false);
  // VKContext::get()->state_manager_get().set_vertex_input(input);
  BLI_assert_unreachable();
}

/** \} */

}  // namespace blender::gpu
