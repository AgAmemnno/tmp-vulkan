/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "gpu_immediate_private.hh"

#include "vk_mem_alloc.h"
#include "vk_context.hh"
#include "vk_vertex_array.hh"





namespace blender::gpu {

/* size of internal buffer */
#define DEFAULT_INTERNAL_BUFFER_SIZE (4 * 1024 * 1024)

struct VBO_INFO {
  /** Vulkan Handle for this buffer. */
  VkBuffer vbo_id = VK_NULL_HANDLE;
  /** Offset of the mapped data in data. */
  VkDeviceSize buffer_offset = 0;
  /** Size of the whole buffer in bytes. */
  VkDeviceSize buffer_size = 0;

  VmaAllocation allocation = nullptr;
};

class VKImmediate : public Immediate {
 public:
  void record();

 private:
  VkPipeline current_pipe_ = VK_NULL_HANDLE;

  /** Size in bytes of the mapped region. */
  size_t bytes_mapped_ = 0;
  /** Vertex array for this immediate mode instance. */
  /// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_vertex_attribute_divisor.html

  VKVertArray::VKVertexInput vao;


  uchar data_[4 * 1024 * 1024];
  VKContext *context_;
  VKBuffer staging_buffer_;
  VKBuffer buffer_;

 public:
  VKImmediate(VKContext *context_);
  ~VKImmediate();

  uchar *begin(void) override;
  void end(void) override;

  /// private:
};

}  // namespace blender::gpu
