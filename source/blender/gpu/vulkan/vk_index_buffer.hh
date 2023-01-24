/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_index_buffer_private.hh"
#include "vk_context.hh"


namespace blender::gpu {

class VKBuffer;

class VKIndexBuf : public IndexBuf {
  friend class GLBatch;
  friend class GLDrawList;
  friend class GLShader; /* For compute shaders. */
 private:
  VKBuffer* ibo_id_ = nullptr;

 public:

  ~VKIndexBuf();

  void bind();
  void *offset_ptr(uint additional_vertex_offset) const
  {
    additional_vertex_offset += index_start_;
    if (index_type_ == GPU_INDEX_U32) {
      return reinterpret_cast<void *>(intptr_t(additional_vertex_offset) * sizeof(uint32_t));
    }
    return reinterpret_cast<void *>(intptr_t(additional_vertex_offset) * sizeof(uint16_t));
  }

  unsigned int restart_index() const
  {
    return (index_type_ == GPU_INDEX_U16) ? 0xFFFFu : 0xFFFFFFFFu;
  }

  void bind_as_ssbo(uint binding) override;
  const uint32_t *read() const override;
  void upload_data() override;
  void update_sub(uint start, uint len, const void *data) override;

 private:
   bool is_active() const;
   void strip_restart_indices() override
   {
     /* No-op. */
   }

};

static inline VkIndexType  to_vk(GPUIndexBufType type)
{

  return (type == GPU_INDEX_U32) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
}


}  // namespace blender::gpu
