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




/** Returns enabled vertex pointers as a bit-flag (one bit per attribute). */


void VKVertArray::update_bindings( VKVao& vao,
                                  const GPUBatch *batch_, /* Should be VKBatch. */
                                  const ShaderInterface *interface_,
                                  const int base_instance)
{


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
      attr_mask &= ~( interface->vbo_bind(vao,vbo,cmd, &vbo->format, 0, vbo->vertex_len, false) );
      bind = true;
    }
  }

  for (int v = GPU_BATCH_INST_VBO_MAX_LEN - 1; v > -1; v--) {
    VKVertBuf *vbo = static_cast<VKVertBuf*>(batch->inst_(v));
    if (vbo) {
      /*TODO*/
      vbo->bind(); 
      attr_mask &= ~(interface->vbo_bind(vao,vbo, cmd, &vbo->format, base_instance, vbo->vertex_len, true));
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
    bool bind_apd = false;
    for (uint16_t mask = 1, a = 0; a < 16; a++, mask <<= 1) {
      if (attr_mask & mask) {
        VkVertexInputBindingDescription vk_bind = {};
        VkVertexInputAttributeDescription vk_attr = {};
        for( auto& attr : interface->desc_inputs_[0].attributes) {
          if (attr.location == a) {
            vk_attr.binding  =  vao.bindings.size();
            if (bind_apd) {
              vk_attr.binding -= 1;
            }
            vk_attr.location = attr.location;
            vk_attr.offset   = 0;
            vk_attr.format  = attr.format;
          }
        }


        if (!bind_apd) {
          vk_bind.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;// VK_VERTEX_INPUT_RATE_VERTEX;
          vk_bind.stride = 4 * sizeof(float);
          vk_bind.binding = vao.bindings.size();
          vao.bindings.append(vk_bind);
          bind_apd = true;
          vao.vbos.append( context->default_attr_vbo_);
        }

        vao.attributes.append(vk_attr);


        /* Commutative with glVertexAttribFormat(a, 4, GL_FLOAT, GL_FALSE, 0); */
       
        /* This replaces glVertexAttrib4f(a, 0.0f, 0.0f, 0.0f, 1.0f); with a more modern style.
         * Fix issues for some drivers (see T75069).
         *  glBindVertexBuffer(a, ctx->default_attr_vbo_, intptr_t(0), intptr_t(0));
             glEnableVertexAttribArray(a);
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

void VKVertArray::update_bindings(VKVao &vao,
                                  const uint v_first,
                                  const GPUVertFormat *format,
                                  const ShaderInterface *interface)
{
  BLI_assert(false);
  /*#glBindVertexArray(vao);*/
  //vbo_bind(interface, format, v_first, 0, false);
}

/** \} */

}  // namespace blender::gpu
