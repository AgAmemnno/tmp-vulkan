/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 by Mike Erwin. All rights reserved. */

/** \file
 * \ingroup gpu
 *
 * Mimics old style opengl immediate mode drawing.
 */
//#define VMA_IMPLEMENTATION
#define WIN32_LEAN_AND_MEAN

#include "vk_immediate.hh"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "gl_debug.hh"
#include "gpu_context_private.hh"
#include "gpu_shader_private.hh"
#include "gpu_vertex_format_private.h"
#include "vk_context.hh"
#include "vk_debug.hh"
#include "vk_framebuffer.hh"
#include "vk_mem_alloc.h"
#include "vk_memory.hh"
#include "vk_shader.hh"
#include "vk_state.hh"

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
          glBufferData(GL_ARRAY_BUFFER, buffer.buffer_size, nullptr, GL_DYNAMIC_DRAW);

          buffer_strict.buffer_size = DEFAULT_INTERNAL_BUFFER_SIZE;
          glGenBuffers(1, &buffer_strict.vbo_id);
          glBindBuffer(GL_ARRAY_BUFFER, buffer_strict.vbo_id);
          glBufferData(GL_ARRAY_BUFFER, buffer_strict.buffer_size, nullptr, GL_DYNAMIC_DRAW);
          */
          /// </summary>

          /// glGenVertexArrays(1, &vao_id_);
          /// glBindVertexArray(vao_id_); /* Necessary for glObjectLabel. */

          /// glBindBuffer(GL_ARRAY_BUFFER, 0);
          /// glBindVertexArray(0);

          // debug::object_label(GL_VERTEX_ARRAY, vao_id_, "Immediate");
          // debug::object_label(GL_BUFFER, buffer.vbo_id, "ImmediateVbo");
          // debug::object_label(GL_BUFFER, buffer_strict.vbo_id, "ImmediateVboStrict");
      };

VKImmediate::~VKImmediate()
{

  if (vkbuffer_)
    delete vkbuffer_;

  vao.vertexInputAttributes.clear();
  vao.vertexInputBindings.clear();
  /*
   glDeleteVertexArrays(1, &vao_id_);
  glDeleteBuffers(1, &buffer.vbo_id);
  glDeleteBuffers(1, &buffer_strict.vbo_id);
  */
}

static inline GLenum to_vk(GPUVertCompType type)
{
  switch (type) {
    case GPU_COMP_I8:
      return GL_BYTE;
    case GPU_COMP_U8:
      return GL_UNSIGNED_BYTE;
    case GPU_COMP_I16:
      return GL_SHORT;
    case GPU_COMP_U16:
      return GL_UNSIGNED_SHORT;
    case GPU_COMP_I32:
      return GL_INT;
    case GPU_COMP_U32:
      return GL_UNSIGNED_INT;
    case GPU_COMP_F32:
      return GL_FLOAT;
    case GPU_COMP_I10:
      return GL_INT_2_10_10_10_REV;
    default:
      BLI_assert(0);
      return GL_FLOAT;
  }
}

uint16_t VKImmediate::descript_vao(const ShaderInterface *interface_,
                                   const GPUVertFormat *format,
                                   uint v_first,
                                   uint v_len,
                                   const bool use_instancing)
{
  /// Vertex Input Information <--> OpenGL VertexArray
  vao.vertexInputBindings.resize(1);
  auto &bindingDescription = vao.vertexInputBindings[0];
  bindingDescription.binding = 0;

  if (format->deinterleaved) {
    bindingDescription.stride = format->attrs[0].size;
  }
  else {
    bindingDescription.stride = format->stride;
  }
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  const uint attr_len = format->attr_len;
  uint stride = format->stride;

  uint16_t enabled_attrib = 0;
  uint offset;

  GLuint divisor = (use_instancing) ? 1 : 0;
  vao.vertexInputAttributes.resize(attr_len);
  vao.divisors.resize(attr_len);

  for (uint a_idx = 0; a_idx < attr_len; a_idx++) {
    const GPUVertAttr *a = &format->attrs[a_idx];

    BLI_assert((a->name_len == 1 && "TODO atrribute deinterleaved. "));
    auto &attr = vao.vertexInputAttributes[a_idx];
    auto &divs = vao.divisors[a_idx];
    attr.binding = divs.binding = bindingDescription.binding;

    if (format->deinterleaved) {
      BLI_assert((false && "TODO atrribute deinterleaved. "));
      if (a_idx == 0) {
        offset = (v_first * a->size);
      }
      else {
        if (a_idx == 1) {
          offset = format->attrs[a_idx - 1].size * v_len;
        }
        else
          offset += format->attrs[a_idx - 1].size * v_len;
      }

      stride = a->size;
    }
    else {
      offset = a->offset;
    }

    /* This is in fact an offset in memory. */
    /// const GLvoid *pointer = (const GLubyte *)intptr_t(offset + v_first * stride);
    /// const GLenum type = to_gl(static_cast<GPUVertCompType>(a->comp_type));

    attr.offset = offset;

    for (uint n_idx = 0; n_idx < a->name_len; n_idx++) {
      const char *name = GPU_vertformat_attr_name_get(format, a, n_idx);
      const ShaderInput *input = interface_->attr_get(name);

      if (input == nullptr || input->location == -1) {
        continue;
      }

      enabled_attrib |= (1 << input->location);

      if (ELEM(a->comp_len, 16, 12, 8)) {
        BLI_assert((false && "TODO atrribute deinterleaved. "));
        BLI_assert(a->fetch_mode == GPU_FETCH_FLOAT);
        BLI_assert(a->comp_type == GPU_COMP_F32);

        for (int i = 0; i < a->comp_len / 4; i++) {
          /// glEnableVertexAttribArray(input->location + i);
          attr.location = input->location + i;
          /// glVertexAttribDivisor(input->location + i, divisor);
          divs.divisor = divisor;
          attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
          /// glVertexAttribPointer( input->location + i, 4, type, GL_FALSE, stride, (const GLubyte
          /// *)pointer + i * 16);
        }
      }
      else {
        glEnableVertexAttribArray(input->location);
        glVertexAttribDivisor(input->location, divisor);
        attr.location = input->location;
        divs.divisor = divisor;

        switch (a->fetch_mode) {
          case GPU_FETCH_FLOAT:
          case GPU_FETCH_INT_TO_FLOAT:
            switch (a->comp_len) {
              case 4:
                attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
                break;
              case 3:
                attr.format = VK_FORMAT_R32G32B32_SFLOAT;
                break;
              case 2:
                attr.format = VK_FORMAT_R32G32_SFLOAT;
                break;
              case 1:
                attr.format = VK_FORMAT_R32_SFLOAT;
                break;
              default:
                BLI_assert((false && "Unreachable VAO Information"));
                break;
            }
            /// glVertexAttribPointer(input->location, a->comp_len, type, GL_FALSE, stride,
            /// pointer);
            break;
          case GPU_FETCH_INT_TO_FLOAT_UNIT:
            switch (a->comp_len) {
              case 4:
                attr.format = VK_FORMAT_R32G32B32A32_UINT;
                break;
              case 3:
                attr.format = VK_FORMAT_R32G32B32_UINT;
                break;
              case 2:
                attr.format = VK_FORMAT_R32G32_UINT;
                break;
              case 1:
                attr.format = VK_FORMAT_R32_UINT;
                break;
              default:
                BLI_assert((false && "Unreachable VAO Information"));
                break;
            }

            /// glVertexAttribPointer(input->location, a->comp_len, type, GL_TRUE, stride,
            /// pointer);
            break;
          case GPU_FETCH_INT:
            switch (a->comp_len) {
              case 4:
                attr.format = VK_FORMAT_R32G32B32A32_SINT;
                break;
              case 3:
                attr.format = VK_FORMAT_R32G32B32_SINT;
                break;
              case 2:
                attr.format = VK_FORMAT_R32G32_SINT;
                break;
              case 1:
                attr.format = VK_FORMAT_R32_SINT;
                break;
              default:
                BLI_assert((false && "Unreachable VAO Information"));
                break;
            }
            /// glVertexAttribIPointer(input->location, a->comp_len, type, stride, pointer);
            break;
        }
      }
    }
  }
  return enabled_attrib;
}

void VKImmediate::update_bindings(const uint v_first,
                                  const GPUVertFormat *format,
                                  const ShaderInterface *interface_)
{
  // glBindVertexArray(vao);

  descript_vao(interface_, format, v_first, 0, false);
}

uchar *VKImmediate::begin()
{

  const size_t bytes_needed = vertex_buffer_size(&vertex_format, vertex_len);
  GL_CHECK_RESOURCES("Immediate");
  vkstaging_ = context_->get_buffer_manager()->Create(bytes_needed, 256);

  bytes_mapped_ = bytes_needed;
  void *ptr = vkstaging_->get_host_ptr();
  BLI_assert(ptr);
  if (!vkbuffer_) {
    VKResourceOptions options;
    options.setDeviceLocal(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    vkbuffer_ = new VKBuffer(bytes_needed, 256, options);
  }
  else
    vkbuffer_->Resize(bytes_needed, 256);

  strict_vertex_len = false;
  VKStateManager::set_prim_type(prim_type);

  return (uchar *)ptr;
}

void VKImmediate::end()
{
  BLI_assert(prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */

  vkstaging_->unmap();
  if (vertex_len > 0) {
    VkBufferCopy region_ = {0, 0, vkbuffer_->get_buffer_size()};
    context_->get_buffer_manager()->Copy(*vkbuffer_, region_);

    context_->state_manager->apply_state();
    auto fb = static_cast<VKFrameBuffer *>(context_->active_fb);
    context_->pipeline_state.active_shader->CreatePipeline(fb->get_render_pass());

    record();
  }
}

void VKImmediate::record()
{

  if (vertex_len <= 0)
    return;
  VKShader *vkshader = reinterpret_cast<VKShader *>(shader);
  current_pipe_ = vkshader->get_pipeline();
  BLI_assert(current_pipe_ != VK_NULL_HANDLE);

  vkshader->update_descriptor_set();

  auto vkinterface = (VKShaderInterface *)vkshader->interface;
  auto descN = 0;
  for (auto &set : vkinterface->setlayouts_) {
    if (set != VK_NULL_HANDLE) {
      descN++;
    }
  }

  auto vert = vkbuffer_->get_vk_buffer();
  VkDeviceSize offsets[1] = {0};
  VKFrameBuffer *fb = static_cast<VKFrameBuffer *>(context_->active_fb);
  int swapID = context_->get_current_image_index();

  vkshader->current_cmd_ = VK_NULL_HANDLE;
  vkshader->current_cmd_ = fb->render_begin(vkshader->current_cmd_,
                                            VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  /* To know when we generate a command and start a record but don't use it. It might be better for
   * the state manager to do it. */
  fb->set_dirty_render(true);
  vkCmdBindPipeline(vkshader->current_cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, current_pipe_);

  VKStateManager::cmd_dynamic_state(vkshader->current_cmd_);

  if (vkinterface->push_range_.size > 0) {
    vkCmdPushConstants(vkshader->current_cmd_,
                       vkshader->current_layout_,
                       vkinterface->push_range_.stageFlags,
                       vkinterface->push_range_.offset,
                       vkinterface->push_range_.size,
                       vkinterface->push_cache_);
  }
  if (descN == 1) {
    vkCmdBindDescriptorSets(vkshader->current_cmd_,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            vkshader->current_layout_,
                            0,
                            1,
                            &vkinterface->sets_vec_[0][swapID],
                            0,
                            NULL);
  }
  else if (descN != 0)
    BLI_assert_msg(false, "Descriptor Set Number is only 0 for now.");

  vkCmdBindVertexBuffers(vkshader->current_cmd_, 0, 1, &vert, offsets);
  vkCmdDraw(vkshader->current_cmd_, vertex_len, 1, 0, 0);

  fb->render_end();
  if (false)
  {
    static int cnt = 1000;
    std::string filename = "vk_frame_" + std::to_string(fb->get_height()) +
                           std::to_string(fb->get_width()) + "No" + std::to_string(cnt) + ".ppm";
    fb->save_current_frame(filename.c_str());
    cnt++;
  }
};

/** \} */

}  // namespace blender::gpu
