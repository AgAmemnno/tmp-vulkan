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

#include "vk_vertex_array.hh"
#include "vk_framebuffer.hh"
#include "vk_shader_interface.hh"
#include "vk_shader.hh"


namespace blender::gpu {
  /* -------------------------------------------------------------------- */
/** \name Vertex Array Bindings
 * \{ */

  struct VKVao {
    
    VkPipelineVertexInputStateCreateInfo info = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL };


    VkPipelineVertexInputDivisorStateCreateInfoEXT divisorInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT, NULL };

    Vector<VkVertexInputBindingDivisorDescriptionEXT>  divisors;
    Vector<VkVertexInputBindingDescription>                    bindings;
    Vector<VkVertexInputAttributeDescription>             attributes;

    VKVao()
    {
      info.pNext = &divisorInfo;
      divisorInfo.vertexBindingDivisorCount = 0;
      divisorInfo.pVertexBindingDivisors = NULL;
    }
  };



/** Returns enabled vertex pointers as a bit-flag (one bit per attribute). */
static uint16_t vbo_bind(const ShaderInterface *interface,
                         const GPUVertFormat *format,
                         uint v_first,
                         uint v_len,
                         const bool use_instancing)
{
  uint16_t enabled_attrib = 0;
  const uint attr_len = format->attr_len;
  uint stride = format->stride;
  uint offset = 0;
  //GLuint divisor = (use_instancing) ? 1 : 0;

  for (uint a_idx = 0; a_idx < attr_len; a_idx++) {
    const GPUVertAttr *a = &format->attrs[a_idx];

    if (format->deinterleaved) {
      offset += ((a_idx == 0) ? 0 : format->attrs[a_idx - 1].size) * v_len;
      stride = a->size;
    }
    else {
      offset = a->offset;
    }
   
    /* This is in fact an offset in memory. */
   // const GLvoid *pointer    = (const GLubyte *)intptr_t(offset + v_first * stride);
    const   VkFormat  type  = to_vk(static_cast<GPUVertCompType>(a->comp_type));

    for (uint n_idx = 0; n_idx < a->name_len; n_idx++) {
      const char *name = GPU_vertformat_attr_name_get(format, a, n_idx);
      const ShaderInput *input = interface->attr_get(name);

      if (input == nullptr || input->location == -1) {
        continue;
      }


      enabled_attrib |= (1 << input->location);
      /*TODO  attrib pointer*/
#if 0
      BLI_assert(false);

      

      if (ELEM(a->comp_len, 16, 12, 8)) {
        BLI_assert(a->fetch_mode == GPU_FETCH_FLOAT);
        BLI_assert(a->comp_type == GPU_COMP_F32);
        for (int i = 0; i < a->comp_len / 4; i++) {
          glEnableVertexAttribArray(input->location + i);
          glVertexAttribDivisor(input->location + i, divisor);
          glVertexAttribPointer(
              input->location + i, 4, type, GL_FALSE, stride, (const GLubyte *)pointer + i * 16);
        }
      }
      else {
        glEnableVertexAttribArray(input->location);
        glVertexAttribDivisor(input->location, divisor);

        switch (a->fetch_mode) {
          case GPU_FETCH_FLOAT:
          case GPU_FETCH_INT_TO_FLOAT:
            glVertexAttribPointer(input->location, a->comp_len, type, GL_FALSE, stride, pointer);
            break;
          case GPU_FETCH_INT_TO_FLOAT_UNIT:
            glVertexAttribPointer(input->location, a->comp_len, type, GL_TRUE, stride, pointer);
            break;
          case GPU_FETCH_INT:
            glVertexAttribIPointer(input->location, a->comp_len, type, stride, pointer);
            break;
        }
      }
#endif
    }
  }



  return enabled_attrib;
}

void VKVertArray::update_bindings(const VKVAOty vao,
                                  const GPUBatch *batch_, /* Should be VKBatch. */
                                  const ShaderInterface *interface_,
                                  const int base_instance)
{
  printf("Update bindings  batch    %llx   \n", (uintptr_t)batch_);

  const VKBatch *batch = static_cast<const VKBatch *>(batch_);
  uint16_t attr_mask = interface_->enabled_attr_mask_;
 
  VKContext* context  = VKContext::get();
  VKFrameBuffer * fb = static_cast<VKFrameBuffer*> (context->active_fb);
  VkCommandBuffer  cmd = fb->render_begin(VK_NULL_HANDLE, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  VKShaderInterface* interface = (VKShaderInterface*)(const_cast<ShaderInterface*>(interface_));

  /* Reverse order so first VBO'S have more prevalence (in term of attribute override). */
  bool bind = false;

  for (int v = GPU_BATCH_VBO_MAX_LEN - 1; v > -1; v--) {
    VKVertBuf *vbo = static_cast<VKVertBuf*>(batch->verts_(v));
    if (vbo) {
      vbo->bind();
      attr_mask &= ~( interface->vbo_bind(vbo,cmd, &vbo->format, 0, vbo->vertex_len, false) );
      bind = true;
    }
  }




  for (int v = GPU_BATCH_INST_VBO_MAX_LEN - 1; v > -1; v--) {
    VKVertBuf *vbo = static_cast<VKVertBuf*>(batch->inst_(v));
    if (vbo) {
      /*TODO*/

      vbo->bind(); 
      attr_mask &= ~(interface->vbo_bind(vbo, cmd, &vbo->format, base_instance, vbo->vertex_len, true));
    }
  }

  if (batch->resource_id_buf) {
    const ShaderInput *input = interface->attr_get("drw_ResourceID");
    if (input) {
      BLI_assert(false);
      /*TODO
      dynamic_cast<VKStorageBuf *>(unwrap(batch->resource_id_buf))->bind_as(GL_ARRAY_BUFFER);
      glEnableVertexAttribArray(input->location);
      glVertexAttribDivisor(input->location, 1);
      glVertexAttribIPointer(
          input->location, 1, to_gl(GPU_COMP_I32), sizeof(uint32_t), (GLvoid *)nullptr);
      attr_mask &= ~(1 << input->location);
      */
    }
  }

  if (attr_mask != 0 && VKContext::vertex_attrib_binding_support) {
    for (uint16_t mask = 1, a = 0; a < 16; a++, mask <<= 1) {
      if (attr_mask & mask) {
        /*TODO*/
        continue;
      
        /* This replaces glVertexAttrib4f(a, 0.0f, 0.0f, 0.0f, 1.0f); with a more modern style.
         * Fix issues for some drivers (see T75069). 
        glBindVertexBuffer(a, ctx->default_attr_vbo_, intptr_t(0), intptr_t(0));
        glEnableVertexAttribArray(a);
        glVertexAttribFormat(a, 4, GL_FLOAT, GL_FALSE, 0);
        glVertexAttribBinding(a, a);
        */
      }
    }
  }

  if (batch->elem) {

    /* Binds the index buffer. This state is also saved in the VAO. */
    VKIndexBuf* elem = static_cast<VKIndexBuf*>(unwrap(batch->elem));
     elem->bind();
     elem->vk_bind(cmd, 0);
  }

  /*Check for unbound attributes.*/
  context->pipeline_state.active_shader->attr_mask_unbound_ &= attr_mask;

}

void VKVertArray::update_bindings(const VKVAOty vao,
                                  const uint v_first,
                                  const GPUVertFormat *format,
                                  const ShaderInterface *interface)
{

  /*#glBindVertexArray(vao);*/
  vbo_bind(interface, format, v_first, 0, false);
}

/** \} */

}  // namespace blender::gpu
