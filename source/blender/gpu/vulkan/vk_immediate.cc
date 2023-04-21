/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation */

/** \file
 * \ingroup gpu
 *
 * Mimics old style OpenGL immediate mode drawing.
 */

#include "GPU_debug.h"

#include "vk_immediate.hh"

#include "gpu_vertex_format_private.h"

namespace blender::gpu {

static void activate(VKContext& context,int CNT){
  context.debug_capture_title( (std::string("IMM") + std::to_string(CNT)).c_str());
  VKFrameBuffer *fb = context.active_framebuffer_get();
  int viewport[4];
  fb->viewport_get(viewport);
  printf(
      ">>>>>>>>>>>>>>>>>>>>>>VKBatch Draw<<<<<<<<<<<<< CNT[%d]  FrameBuffer %llx Viewport (%d %d "
      "%d %d) \n",
      CNT,
      (uintptr_t)fb->vk_framebuffer_get(),
      viewport[0],
      viewport[1],
      viewport[2],
      viewport[3]);
   context.activate_framebuffer(*fb);
}
VKImmediate::VKImmediate() {}
VKImmediate::~VKImmediate()
{
  buffer_.free(*VKContext::get());
}
  static int CNT = 0;
uchar *VKImmediate::begin()
{
  GPU_debug_capture_begin();

  VKContext &context = *VKContext::get();

  activate(context,CNT);

  const size_t bytes_needed = vertex_buffer_size(&vertex_format, vertex_len);
  if (!buffer_.is_allocated()) {
    buffer_.create(context,
                   DEFAULT_INTERNAL_BUFFER_SIZE,
                   GPU_USAGE_DYNAMIC,
                   static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT));
  }

  /* TODO: Resize buffer when more is needed. Currently assert as we haven't implemented it yet. */
  BLI_assert(bytes_needed < DEFAULT_INTERNAL_BUFFER_SIZE);

  uchar *data = static_cast<uchar *>(buffer_.mapped_memory_get());
  return data;
}

void VKImmediate::end()
{
  BLI_assert_msg(prim_type != GPU_PRIM_NONE, "Illegal state: not between an immBegin/End pair.");
  if (vertex_len == 0) {
    return;
  }

  VKContext &context = *VKContext::get();
  BLI_assert(context.shader == unwrap(shader));
  context.state_manager->apply_state();
  vertex_attributes_.update_bindings(*this);
  context.bind_graphics_pipeline(prim_type, vertex_attributes_);
  vertex_attributes_.bind(context);

  context.command_buffer_get().draw(0, vertex_len, 0, 1);
  context.command_buffer_get().submit(true, false);
  GPU_debug_capture_end();
  CNT++;
}

/** \} */

}  // namespace blender::gpu
