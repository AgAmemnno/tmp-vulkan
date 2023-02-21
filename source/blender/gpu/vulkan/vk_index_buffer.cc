/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_index_buffer.hh"
#include "vk_memory.hh"
#include "vk_context.hh"


namespace blender::gpu {

VKIndexBuf::~VKIndexBuf()
{
  VKContext::buf_free(ibo_id_);
}

void VKIndexBuf::bind()
{
  if (is_subrange_) {
    static_cast<VKIndexBuf *>(src_)->bind();
    return;
  }

  const bool allocate_on_device = ibo_id_ == nullptr;
  size_t size = 0; 

  if (allocate_on_device) {
    /* #glGenBuffers(1, &ibo_id_);*/

    VKResourceOptions options;
    options.setDeviceLocal( VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    if (data_ != nullptr) {
      size = this->size_get();
    }
    ibo_id_ = new VKVAOty_impl(size, 256, options);

  }

  if (data_ != nullptr || allocate_on_device) {
     
    /* Sends data to GPU.  #glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data_, GL_STATIC_DRAW);*/
    ibo_id_->Copy(data_, size);
    /* No need to keep copy of data in system memory. */
    MEM_SAFE_FREE(data_);
  }

}
void VKIndexBuf::vk_bind(VkCommandBuffer cmd, VkDeviceSize offset ) {
  VkBuffer buf = VK_NULL_HANDLE; 
  if (is_subrange_) {
    buf = static_cast<VKIndexBuf *>(src_)->ibo_id_->get_vk_buffer();
  }
  else {
    buf = ibo_id_->get_vk_buffer();
  }
  BLI_assert(buf != VK_NULL_HANDLE);
  vkCmdBindIndexBuffer(cmd, buf, offset, to_vk(index_type_));

}
void VKIndexBuf::bind_as_ssbo(uint binding)
{
  BLI_assert(false);
#if 0
  bind();
  BLI_assert(ibo_id_ != 0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, ibo_id_);
#endif
}

const uint32_t *VKIndexBuf::read() const
{
  /* host visible or devicelocal ?*/
  BLI_assert(false);

  return nullptr;
}

bool VKIndexBuf::is_active() const
{
  if (!ibo_id_) {
    return false;
  }
  /* Need for design. Are we querying the active bindings for each command buffer, or should the command buffer actually be in the command begin state? */
#if 0
  int active_ibo_id = 0;
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &active_ibo_id);
  return ibo_id_ == active_ibo_id;
#endif
  return true;

}

void VKIndexBuf::upload_data()
{
  bind();
}

void VKIndexBuf::update_sub(uint start, uint len, const void *data)
{
  /* glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, start, len, data); */
  BLI_assert(ibo_id_);
  ibo_id_->Copy((void*)data, len, start);

}

}  // namespace blender::gpu
