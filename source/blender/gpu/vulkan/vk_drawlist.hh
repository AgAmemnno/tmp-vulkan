/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_drawlist_private.hh"
#include "vk_context.hh"
#include "vk_batch.hh"

namespace blender::gpu {
typedef signed long long int  GLintptr;
typedef signed long long int  GLsizeiptr;

typedef uint GLuint;
class VKDrawList : public DrawList {
 public:
  VKDrawList(int length);
  ~VKDrawList();
  void append(GPUBatch *batch, int i_first, int i_count) override;
  void submit() override;

 private:
  void init();

  /** Batch for which we are recording commands for. */
  VKBatch *batch_;
  /** Mapped memory bounds. */
  int8_t *data_;
  /** Length of the mapped buffer (in byte). */
  VkDeviceSize data_size_;
  /** Current offset inside the mapped buffer (in byte). */
  GLintptr command_offset_;
  /** Current number of command recorded inside the mapped buffer. */
  uint command_len_;
  /** Is UINT_MAX if not drawing indexed geom. Also Avoid dereferencing batch. */
  GLuint base_index_;
  /** Also Avoid dereferencing batch. */
  GLuint v_first_, v_count_;

  /** VK Indirect Buffer id. Nullptr means MultiDrawIndirect is not supported/enabled. */
  VKBuffer* buffer_id_ = nullptr;
  /** Length of whole the buffer (in byte). */
  GLsizeiptr buffer_size_;
  /** Offset of `data_` inside the whole buffer (in byte). */
  GLintptr data_offset_;

  /** To free the buffer_id_. */
  VKContext *context_;

  MEM_CXX_CLASS_ALLOC_FUNCS("VKDrawList");
};

}  // namespace blender::gpu
