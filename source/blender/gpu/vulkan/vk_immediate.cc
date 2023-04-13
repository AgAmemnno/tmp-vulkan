/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */
/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 by Mike Erwin. All rights reserved. */



//#define VMA_IMPLEMENTATION
#define WIN32_LEAN_AND_MEAN


#include "GPU_immediate.h"
#include "GPU_matrix.h"


#include "gpu_context_private.hh"
#include "gpu_shader_private.hh"
#include "gpu_vertex_format_private.h"

#include "vk_immediate.hh"
#include "vk_context.hh"
#include "vk_debug.hh"
#include "vk_framebuffer.hh"
#include "vk_mem_alloc.h"
#include "vk_memory.hh"
#include "vk_shader.hh"
#include "vk_state_manager.hh"
#include "vk_vertex_array.hh"


#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)
#define VK_CHECK2(expr) \
  do { \
    if ((expr) < 0) { \
      assert(0 && #expr); \
      throw std::runtime_error(__FILE__ "(" LINE_STRING "): VkResult( " #expr " ) < 0"); \
    } \
  } while (false)

namespace blender::gpu {

VKImmediate::VKImmediate(VKContext *context)
    : context_(context){

          /// VmaAllocator mem_allocator = context_->mem_allocator_get();

          /// <summary>
          ///   buffer.buffer_size = DEFAULT_INTERNAL_BUFFER_SIZE;
          /*
          glGenBuffers(1, &buffer.vbo_id);
          glBindBuffer(GL_ARRAY_BUFFER, buffer.vbo_id);
          glBufferData(GL_ARRAY_BUFFER, buffer.buffer_size, nullptr,
          GL_DYNAMIC_DRAW);

          buffer_strict.buffer_size = DEFAULT_INTERNAL_BUFFER_SIZE;
          glGenBuffers(1, &buffer_strict.vbo_id);
          glBindBuffer(GL_ARRAY_BUFFER, buffer_strict.vbo_id);
          glBufferData(GL_ARRAY_BUFFER, buffer_strict.buffer_size, nullptr,
          GL_DYNAMIC_DRAW);
          */
          /// </summary>

          /// glGenVertexArrays(1, &vao_id_);
          /// glBindVertexArray(vao_id_); /* Necessary for glObjectLabel. */

          /// glBindBuffer(GL_ARRAY_BUFFER, 0);
          /// glBindVertexArray(0);

          // debug::object_label(GL_VERTEX_ARRAY, vao_id_, "Immediate");
          // debug::object_label(GL_BUFFER, buffer.vbo_id, "ImmediateVbo");
          // debug::object_label(GL_BUFFER, buffer_strict.vbo_id,
          // "ImmediateVboStrict");
      };

VKImmediate::~VKImmediate()
{
  buffer_.free(*context_);
  staging_buffer_.free(*context_);

  vao.vertexInputAttributes.clear();
  vao.vertexInputBindings.clear();
  /*
   glDeleteVertexArrays(1, &vao_id_);
  glDeleteBuffers(1, &buffer.vbo_id);
  glDeleteBuffers(1, &buffer_strict.vbo_id);
  */
}



uchar *VKImmediate::begin()
{
  context_->debug_capture_begin();

  VKFrameBuffer* fb = reinterpret_cast<VKFrameBuffer*>(context_->state_manager_get().active_fb);
  VKTexture& tex = (*(VKTexture*)fb->color_tex(0));
  if(!context_->command_buffer_get().ensure_render_pass()){
    context_->command_buffer_get().begin_render_pass(*fb, tex);
  }
  

  const size_t bytes_needed = vertex_buffer_size(&vertex_format, vertex_len);
  buffer_.create(*context_, bytes_needed, GPU_USAGE_DYNAMIC,  (VkBufferUsageFlagBits)(VK_BUFFER_USAGE_TRANSFER_SRC_BIT |VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ));

  context_->state_manager_get().set_prim_type(prim_type);

  VkBuffer buf = buffer_.vk_handle();
  (context_->command_buffer_get()).bind_vertex_buffers(0,1, &buf);

  return (uchar*)buffer_.mapped_memory_get();
}

void VKImmediate::end()
{

  BLI_assert(prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */


  if (vertex_len > 0) {

    VKContext* ctx = VKContext::get();
    ctx->state_manager->apply_state();



    /* We convert the offset in vertex offset from the buffer's start.
     * This works because we added some padding to align the first vertex. */
    uint v_first = 0;//buffer_offset() / vertex_format.stride;
    VKVertArray::update_bindings( vao, v_first, &vertex_format, reinterpret_cast<Shader *>(shader)->interface);
    ctx->bind_graphics_pipeline();
    ctx->state_manager_get().active_fb->apply_state();

    ctx->shader = nullptr;
    GPU_shader_bind(shader);

    reinterpret_cast<VKShader*>(shader)->pipeline_get().push_constants_get().update(*ctx);
    
    (ctx->command_buffer_get()).draw(vertex_len, 1, 0, 0);
    (ctx->command_buffer_get()).submit(true, false);
    //ctx->swapbuffers();

  }

  context_->debug_capture_end();

}
static std::vector<std::string> split(const std::string &s, char seperator)
{
  std::vector<std::string> output;

  std::string::size_type prev_pos = 0, pos = 0;

  while ((pos = s.find(seperator, pos)) != std::string::npos) {
    std::string substring(s.substr(prev_pos, pos - prev_pos));

    output.push_back(substring);

    prev_pos = ++pos;
  }

  output.push_back(s.substr(prev_pos, pos - prev_pos));  // Last word

  return output;
}
void VKImmediate::record()
{
  #if 0
  if (vertex_len <= 0)
    return;
  VKShader *vkshader = reinterpret_cast<VKShader *>(shader);
  VkPipeline &current_pipe = vkshader->get_pipeline();
  BLI_assert(current_pipe != VK_NULL_HANDLE);
  VKFrameBuffer *fb = static_cast<VKFrameBuffer *>(context_->active_fb);
  if (fb->is_swapchain_) {
    if (fb->is_blit_begin_) {
      fb->render_end();
    }
  }
  vkshader->current_cmd_ = VK_NULL_HANDLE;
  vkshader->current_cmd_ = fb->render_begin(vkshader->current_cmd_,
                                            VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  vkshader->update_descriptor_set(vkshader->current_cmd_, vkshader->current_layout_);
  auto vkinterface = (VKShaderInterface *)vkshader->interface;

  auto vert = vkbuffer_->get_vk_buffer();
  VkDeviceSize offsets[1] = {0};

  fb->set_dirty_render(true);
  vkCmdBindPipeline(vkshader->current_cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, current_pipe);

  VKStateManager::cmd_dynamic_state(vkshader->current_cmd_);

  if (vkinterface->push_range_.size > 0) {
    vkCmdPushConstants(vkshader->current_cmd_,
                       vkshader->current_layout_,
                       vkinterface->push_range_.stageFlags,
                       vkinterface->push_range_.offset,
                       vkinterface->push_range_.size,
                       vkinterface->push_cache_);
  }

  vkCmdBindVertexBuffers(vkshader->current_cmd_, 0, 1, &vert, offsets);

  debug::pushMarker(vkshader->current_cmd_, std::string("ImmDraw") + vkshader->name_get());
  vkCmdDraw(vkshader->current_cmd_, vertex_len, 1, 0, 0);
  debug::popMarker(vkshader->current_cmd_);

  fb->is_dirty_render_ = true;

  if (!fb->is_swapchain_) {
    fb->render_end();
  }
  else {
    // fb->move_pipe(current_pipe);
    fb->render_end();
  }

  static int cnt = 0;
  bool save = false;
  /*
  switch (cnt) {
    case
    default:
      break;
  }
  */
  if (cnt > 34 && cnt < 41) {
    save = true;
  }

  save = false;
  if (save) {
    std::string filename = std::string(fb->name_get()) + "_" + vkshader->name_get();
    auto ve = split(filename, '>');
    if (ve.size() == 1) {
      filename = ve[0] + "No." + std::to_string(cnt);
    }
    else {
      filename = ve[1] + "No." + std::to_string(cnt);
    }
    fb->save_current_frame(filename.c_str());
  }
  cnt++;
#endif
};

/** \} */

}  // namespace blender::gpu
