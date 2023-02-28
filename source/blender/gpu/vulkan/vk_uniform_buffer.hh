/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "gpu_uniform_buffer_private.hh"

namespace blender {
namespace gpu {

/**
 * Implementation of Uniform Buffers using OpenGL.
 **/
class VKUniformBuf : public UniformBuf {
 private:
  int slot_ = -1;
  uint32_t ubo_id_ = 0;
  VKBuffer *ubo = nullptr;
  VkDescriptorBufferInfo info;
  int setID = 0;

 public:
  VKUniformBuf(size_t size, const char *name);
  ~VKUniformBuf();

  void update(const void *data) override;
  void bind(int slot) override;
  void unbind(void) override;
  void clear_to_zero() override;
  void bind_as_ssbo(int slot) override;
  void setLayoutSetID(int id)
  {
    setID = id;
  };

 private:
  void init(void);

  MEM_CXX_CLASS_ALLOC_FUNCS("VKUniformBuf");
};

}  // namespace gpu
}  // namespace blender
