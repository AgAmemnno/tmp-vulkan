/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 by Mike Erwin. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "GPU_texture.h"

#include "vk_context.hh"
#include "vk_vertex_buffer.hh"
#include "vk_memory.hh"

namespace blender::gpu {


  size_t VKVertBuf::memory_usage_gpu_ = 0;

  VKVertBuf::~VKVertBuf() {
    release_data();
  }

void VKVertBuf::acquire_data()
{
  if (usage_ == GPU_USAGE_DEVICE_ONLY) {
    return;
  }
  /* Discard previous data if any. */
  MEM_SAFE_FREE(data);
  data = (uchar*)MEM_mallocN(sizeof(uchar) * this->size_alloc_get(), __func__);
}
void VKVertBuf::resize_data(void) 
{
  if (usage_ == GPU_USAGE_DEVICE_ONLY) {
    return;
  }

  data = (uchar*)MEM_reallocN(data, sizeof(uchar) * this->size_alloc_get());
};
void VKVertBuf::release_data()
{
  if (is_wrapper_) {
    return;
  }
 
  if (vbo_id_ != 0) {  
    size_t  vbo_size_ = vbo_id_->get_buffer_size();
    size_t  vbo_size_gpu = vbo_id_->get_size();
    VKContext::buf_free(vbo_id_);
    /*TODO :: #GPU_TEXTURE_FREE_SAFE(buffer_texture_);*/
    vbo_id_ = nullptr;
    memory_usage          -= vbo_size_;
    memory_usage_gpu_ -= vbo_size_gpu;
  }

  MEM_SAFE_FREE(data);
  
}

void VKVertBuf::duplicate_data(VertBuf *dst_)
{
  VKContext* context = VKContext::get();
  BLI_assert(context);
  BLI_assert(context->buffer_manager_);

  VKVertBuf* src = this;
  VKVertBuf* dst = static_cast<VKVertBuf*>(dst_);
  /*TODO :: dst->buffer_texture_ = nullptr; */

  VKBuffer* read   =  src->get_vbo();
  VKBuffer* write  =  dst->get_vbo();
  BLI_assert(read);
  BLI_assert(write);

  /*It is the size of src, but is it necessary to resize it?*/
  {
    VkBufferCopy  region = {};
    region.size = src->size_used_get();
    
    region.dstOffset = region.srcOffset = 0;
    context->buffer_manager_->CopyBufferSubData(read,write, region);
    memory_usage += region.size;
    memory_usage_gpu_ += src->get_vbo()->get_size();
  }

  if (data != nullptr) {
    dst->data = (uchar*)MEM_dupallocN(src->data);
  }
}

void VKVertBuf::upload_data()
{
  this->bind();
}

void VKVertBuf::bind()
{
  BLI_assert(VKContext::get() != nullptr);
  if (flag & GPU_VERTBUF_DATA_DIRTY) {
    size_t vbo_size_ = this->size_used_get();
    auto usage  = to_vk(usage_);
    /* Do not transfer data from host to device when buffer is device only. */
    if (vbo_id_ == nullptr) {
      VKResourceOptions options;
      if (usage  ==  VMA_MEMORY_USAGE_CPU_TO_GPU) {
        options.setHostVisible(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
      }
      else {
        options.setDeviceLocal(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
      }
      vbo_id_ = new VKVAOty_impl(vbo_size_, VK_BUFFER_DEFAULT_ALIGNMENT, options);
    }
    else {
      vbo_id_->Resize(vbo_size_);
    }
   
    /* Do not transfer data from host to device when buffer is device only. */
    if (usage_ != GPU_USAGE_DEVICE_ONLY) {
      vbo_id_->Copy(data, vbo_size_);
      /* #glBufferSubData(GL_ARRAY_BUFFER, 0, vbo_size_, data);*/

    }
    memory_usage += vbo_size_;
    memory_usage_gpu_ += vbo_id_->get_size();

    if (usage_ == GPU_USAGE_STATIC) {
      MEM_SAFE_FREE(data);
    }
  
    flag &= ~GPU_VERTBUF_DATA_DIRTY;
    flag |= GPU_VERTBUF_DATA_UPLOADED;
  }
}

void VKVertBuf::bind_as_ssbo(uint binding)
{
  BLI_assert(false);
  bind();
  /*
  BLI_assert(vbo_id_ != 0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, vbo_id_);
  */
}

void VKVertBuf::bind_as_texture(uint binding)
{
  BLI_assert(false);
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

   BLI_assert(vbo_id_);
   auto usage = to_vk(usage_);
   if (usage == VMA_MEMORY_USAGE_CPU_TO_GPU) {
     return vbo_id_->get_host_ptr();
   }
  return nullptr;

}

void *VKVertBuf::unmap(const void *mapped_data) const
{
  BLI_assert(vbo_id_);
  VkDeviceSize  vbo_size_ = vbo_id_->get_buffer_size();
  if (vbo_size_ > 0) {
    void* result = MEM_mallocN(vbo_size_, __func__);
    memcpy(result, mapped_data, vbo_size_);
    return result;
  }
  return nullptr;
}

void VKVertBuf::wrap_handle(uint64_t handle_)
{
  BLI_assert(vbo_id_ == nullptr);
  BLI_assert(handle_);

  VKBuffer* handle = reinterpret_cast <VKBuffer*>(handle_);

  BLI_assert(handle->get_vk_buffer() != VK_NULL_HANDLE);

  is_wrapper_ = true;
  vbo_id_ =  handle;
  /* We assume the data is already on the device, so no need to allocate or send it. */
  flag = GPU_VERTBUF_DATA_UPLOADED;

}

bool VKVertBuf::is_active() const
{
  
  if (!vbo_id_) {
    return false;
  }
  /* Need for design. Are we querying the active bindings for each command buffer, or should the command buffer actually be in the command begin state? */
  /*
  int active_vbo_id = 0;
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &active_vbo_id);
  return vbo_id_ == active_vbo_id;
  */
  return true;
}

void VKVertBuf::update_sub(uint start, uint len, const void *data)
{
  BLI_assert(vbo_id_ );
  vbo_id_->Copy((void*)data, len, start);
}

}  // namespace blender::gpu
