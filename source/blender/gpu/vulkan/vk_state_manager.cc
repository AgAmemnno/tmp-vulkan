/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "vk_state_manager.hh"
#include "vk_context.hh"
#include "vk_pipeline.hh"
#include "vk_shader.hh"
#include "vk_texture.hh"


namespace blender::gpu {


void VKStateManager::apply_state()
{
  VKContext &context = *VKContext::get();
  if(context.shader==nullptr)
  {
    return;
  }
  VKShader &shader = unwrap(*context.shader);
  
  VKPipeline &pipeline = shader.pipeline_get();
  pipeline.state_manager_get().set_state(state, mutable_state);


  BLI_assert(context.active_fb);
  VKFrameBuffer* framebuffer = reinterpret_cast<VKFrameBuffer*>(context.active_fb);
  pipeline.state_manager_get().color_blend_from_framebuffer(framebuffer);
}

void VKStateManager::force_state()
{
  VKContext &context = *VKContext::get();
  VKShader &shader = unwrap(*context.shader);
  VKPipeline &pipeline = shader.pipeline_get();
  pipeline.state_manager_get().force_state(state, mutable_state);
}

void VKStateManager::issue_barrier(eGPUBarrier /*barrier_bits*/)
{
  VKContext &context = *VKContext::get();
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  /* TODO: Pipeline barriers should be added. We might be able to extract it from
   * the actual pipeline, later on, but for now we submit the work as barrier. */
  command_buffer.submit();
}

void VKStateManager::texture_bind(Texture * tex, GPUSamplerState sampler, int slot)
{
  VKTexture *vk_tex = static_cast<VKTexture *>(tex);
  vk_tex->texture_bind(slot, sampler);

}

void VKStateManager::texture_unbind(Texture * /*tex*/) {}

void VKStateManager::texture_unbind_all() {}

void VKStateManager::image_bind(Texture *tex, int binding)
{
  VKTexture *texture = unwrap(tex);
  texture->image_bind(binding);
}

void VKStateManager::image_unbind(Texture * /*tex*/) {}

void VKStateManager::image_unbind_all() {}

void VKStateManager::texture_unpack_row_length_set(uint len)
{
  texture_unpack_row_length_ = len;
}

uint VKStateManager::texture_unpack_row_length_get() const
{
  return texture_unpack_row_length_;
}
}  // namespace blender::gpu
