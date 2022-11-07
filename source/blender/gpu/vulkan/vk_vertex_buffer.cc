/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 by Mike Erwin. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "GPU_texture.h"

#include "vk_context.hh"
#include "vk_vertex_buffer.hh"

namespace blender::gpu {




void VKVertBuf::release_data()
{
  /*
  if (is_wrapper_) {
    return;
  }
  if (vbo_id_ != 0) {
    GPU_TEXTURE_FREE_SAFE(buffer_texture_);
    GLContext::buf_free(vbo_id_);
    vbo_id_ = 0;
    memory_usage -= vbo_size_;
  }
  */

  MEM_SAFE_FREE(data);
  
}

void VKVertBuf::duplicate_data(VertBuf *dst_)
{
  /*
  BLI_assert(GLContext::get() != nullptr);
  GLVertBuf *src = this;
  GLVertBuf *dst = static_cast<GLVertBuf *>(dst_);
  dst->buffer_texture_ = nullptr;

  if (src->vbo_id_ != 0) {
    dst->vbo_size_ = src->size_used_get();

    glGenBuffers(1, &dst->vbo_id_);
    glBindBuffer(GL_COPY_WRITE_BUFFER, dst->vbo_id_);
    glBufferData(GL_COPY_WRITE_BUFFER, dst->vbo_size_, nullptr, to_gl(dst->usage_));

    glBindBuffer(GL_COPY_READ_BUFFER, src->vbo_id_);

    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, dst->vbo_size_);

    memory_usage += dst->vbo_size_;
  }

  if (data != nullptr) {
    dst->data = (uchar *)MEM_dupallocN(src->data);
  }
  */
}

void VKVertBuf::upload_data()
{
  if (usage_ == GPU_USAGE_STATIC) {
    MEM_SAFE_FREE(data);
  }
  this->bind();
}

void VKVertBuf::bind()
{
  BLI_assert(VKContext::get() != nullptr);

  VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

  uint32_t stride = 16;

  VkVertexInputBindingDescription vInputBindDescription0{0, stride, VK_VERTEX_INPUT_RATE_VERTEX};
  VkVertexInputBindingDescription vInputBindDescription1{1, stride, VK_VERTEX_INPUT_RATE_INSTANCE};

  std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
      vInputBindDescription0,
      vInputBindDescription1
  };

 

  std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;
  #if 0
  int i = 0;
  for (i = 0; i < geom->array.fieldNum; i++) {
    vertexInputAttributes.push_back(vka::plysm::vertexInputAttributeDescription(
        0, i, geom->array.format[i], geom->array.offset[i]));
  };

  for (int i1 = 0; i1 < insta->array.fieldNum; i1++) {
    vertexInputAttributes.push_back(vka::plysm::vertexInputAttributeDescription(
        1, i + i1, insta->array.format[i1], insta->array.offset[i1]));
  };

  log_obj2("InstancedObjects  Information   GeometryBlock Nums %zu    InstanceBlock Nums %zu   \n",
           geom->array.fieldNum,
           insta->array.fieldNum);

  Attributes[structType] = vertexInputAttributes;

  info.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
  info.pVertexBindingDescriptions = vertexInputBindings.data();
  info.vertexAttributeDescriptionCount = static_cast<uint32_t>(Attributes[structType].size());
  info.pVertexAttributeDescriptions = Attributes[structType].data();

  Info[structType] = info;
  if (vbo_id_ == 0) {
    glGenBuffers(1, &vbo_id_);
  }

  glBindBuffer(GL_ARRAY_BUFFER, vbo_id_);

  if (flag & GPU_VERTBUF_DATA_DIRTY) {
    vbo_size_ = this->size_used_get();
    // Orphan the vbo to avoid sync then upload data. 
    glBufferData(GL_ARRAY_BUFFER, vbo_size_, nullptr, to_gl(usage_));
    // Do not transfer data from host to device when buffer is device only.
    if (usage_ != GPU_USAGE_DEVICE_ONLY) {
      glBufferSubData(GL_ARRAY_BUFFER, 0, vbo_size_, data);
    }
    memory_usage += vbo_size_;

    if (usage_ == GPU_USAGE_STATIC) {
      MEM_SAFE_FREE(data);
    }
    flag &= ~GPU_VERTBUF_DATA_DIRTY;
    flag |= GPU_VERTBUF_DATA_UPLOADED;
  }
  #endif
}

void VKVertBuf::bind_as_ssbo(uint binding)
{
  bind();
  /*
  BLI_assert(vbo_id_ != 0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, vbo_id_);
  */
}

void VKVertBuf::bind_as_texture(uint binding)
{
  /*
  bind();
  BLI_assert(vbo_id_ != 0);
  if (buffer_texture_ == nullptr) {
    buffer_texture_ = GPU_texture_create_from_vertbuf("vertbuf_as_texture", wrap(this));
  }
  GPU_texture_bind(buffer_texture_, binding);
  */
}

const void *VKVertBuf::read() const
{
  /*
  BLI_assert(is_active());
  void *result = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
  return result;
  */
  return (void *)0;
}

void *VKVertBuf::unmap(const void *mapped_data) const
{
  /*
  void *result = MEM_mallocN(vbo_size_, __func__);
  memcpy(result, mapped_data, vbo_size_);
  return result;
  */
  return (void*)0;
}

void VKVertBuf::wrap_handle(uint64_t handle)
{
  /*
  BLI_assert(vbo_id_ == 0);
  BLI_assert(glIsBuffer(uint(handle)));
  is_wrapper_ = true;
  vbo_id_ = uint(handle);
  // We assume the data is already on the device, so no need to allocate or send it. 
  flag = GPU_VERTBUF_DATA_UPLOADED;
  */
}

bool VKVertBuf::is_active() const
{
  /*
  if (!vbo_id_) {
    return false;
  }
  int active_vbo_id = 0;
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &active_vbo_id);
  return vbo_id_ == active_vbo_id;
  */
  return false;
}

void VKVertBuf::update_sub(uint start, uint len, const void *data)
{
   //glBufferSubData(GL_ARRAY_BUFFER, start, len, data);
}

}  // namespace blender::gpu
