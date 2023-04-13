/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "GPU_debug.h"

#include "vk_batch.hh"
#include "vk_context.hh"
#include "vk_index_buffer.hh"
#include "vk_vertex_buffer.hh"

namespace blender::gpu {


void VKBatch::draw(int v_first, int v_count, int i_first, int i_count)
{
  if (flag & GPU_BATCH_DIRTY) {
    vao_cache_.clear();
    flag &= ~GPU_BATCH_DIRTY;
  }
  GPU_debug_capture_begin();
  VKContext &context = *VKContext::get();
  static int CNT = 0;
  CNT++;

  VKFrameBuffer* fb = context.active_framebuffer_get();
  
  int viewport[4];
  fb->viewport_get(viewport);
  printf(">>>>>>>>>>>>>>>>>>>>>>VKBatch Draw<<<<<<<<<<<<< CNT[%d]  FrameBuffer %llx Viewport (%d %d %d %d) \n",CNT,(uintptr_t)fb->vk_framebuffer_get(),viewport[0],viewport[1],viewport[2],viewport[3]);

  context.activate_framebuffer(*fb);

  context.state_manager->apply_state();
  VKVertexAttributeObject &vao = vao_cache_.vao_get(this);
  vao.update_bindings(context, *this);
  context.bind_graphics_pipeline(*this, vao);
  vao.bind(context);

  VKIndexBuffer *index_buffer = index_buffer_get();
  if (index_buffer) {
    index_buffer->upload_data();
    index_buffer->bind(context);
  }

  context.command_buffer_get().draw(v_first, v_count, i_first, i_count);
  context.command_buffer_get().submit(true, false);
  GPU_debug_capture_end();
  if(CNT >= 1){
    system("pause");
  }
}

void VKBatch::draw_indirect(GPUStorageBuf * /*indirect_buf*/, intptr_t /*offset*/) {}

void VKBatch::multi_draw_indirect(GPUStorageBuf * /*indirect_buf*/,
                                  int /*count*/,
                                  intptr_t /*offset*/,
                                  intptr_t /*stride*/)
{
}

VKVertexBuffer *VKBatch::vertex_buffer_get(int index)
{
  return unwrap(verts_(index));
}

VKVertexBuffer *VKBatch::instance_buffer_get(int index)
{
  return unwrap(inst_(index));
}

VKIndexBuffer *VKBatch::index_buffer_get()
{
  return unwrap(unwrap(elem));
}

}  // namespace blender::gpu
