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

#include "gpu_vertex_buffer_private.hh"

namespace blender {
namespace gpu {
struct vkPVISci {
  std::vector<VkVertexInputBindingDescription> vertexInputBindings;
  std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;
  VkPipelineVertexInputStateCreateInfo info;
};




class VKVertBuf : public VertBuf {
  friend class VKTexture; /* For buffer texture. */
  friend class VKContext;
 public:


  VKVertBuf() {};
  VKVertBuf(VKContext* context) : context_(context){};
  VKContext* context_;

  bool is_active() const;
  void bind();
  void bind_as_ssbo(uint binding) override;
  void bind_as_texture(uint binding) override;
  void update_sub(uint start, uint len, const void *data) override;

  const void *read() const override;
  void *unmap(const void *mapped_data) const override;
  void wrap_handle(uint64_t handle);

 protected:
  void acquire_data(void) override
  {
    MEM_SAFE_FREE(data);
    data = (uchar *)MEM_mallocN(sizeof(uchar) * this->size_alloc_get(), __func__);
  };
  void resize_data(void) override
  {
    data = (uchar *)MEM_reallocN(data, sizeof(uchar) * this->size_alloc_get());
  };
  void release_data(void) override;
  void upload_data(void) override;
  void duplicate_data(VertBuf *dst) override;

  MEM_CXX_CLASS_ALLOC_FUNCS("VKVertBuf");
};

}  // namespace gpu
}  // namespace blender
