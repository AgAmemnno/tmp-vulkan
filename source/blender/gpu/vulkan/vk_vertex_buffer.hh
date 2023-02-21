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


class VKVertBuf : public VertBuf {
  friend class VKTexture; /* For buffer texture. */
  friend class VKContext;
private:

  VKContext* context_  = nullptr;
  VKBuffer* vbo_id_     = nullptr;
  bool is_wrapper_        = false;
  static size_t memory_usage_gpu_ ;

public:

  VKVertBuf() {};
  VKVertBuf(VKContext* context) : context_(context){};
  ~VKVertBuf();
  bool is_active() const;
  void bind();
  void bind_as_ssbo(uint binding) override;
  void bind_as_texture(uint binding) override;
  void update_sub(uint start, uint len, const void *data) override;

  const void *read() const override;
  void *unmap(const void *mapped_data) const override;
  void wrap_handle(uint64_t handle) override;
  VkBuffer get_vk_buffer() {
    BLI_assert(vbo_id_);
    return vbo_id_->get_vk_buffer();
  }
  VKBuffer* get_vbo() {
    return vbo_id_;
  }
 protected:
   void acquire_data(void) override;
   void resize_data(void) override;
  void release_data(void) override;
  void upload_data(void) override;
  void duplicate_data(VertBuf *dst) override;

  MEM_CXX_CLASS_ALLOC_FUNCS("VKVertBuf");
};
static inline VmaMemoryUsage to_vk(GPUUsageType type)
{
  /*What is the difference between DynDrawDraw and StreamDraw?*/
  switch (type) {
  case GPU_USAGE_STREAM:
  case GPU_USAGE_DYNAMIC:
    return VMA_MEMORY_USAGE_CPU_TO_GPU;
  case GPU_USAGE_STATIC:
  case GPU_USAGE_DEVICE_ONLY:
    return VMA_MEMORY_USAGE_GPU_ONLY;
  default:
    BLI_assert(0);
    return VMA_MEMORY_USAGE_GPU_ONLY;
  }
}


static inline VkFormat to_vk(GPUVertCompType type, uint size)
{

  switch (type) {
  case GPU_COMP_I8:
      switch (size) {
      case 1:
        return VK_FORMAT_R8_SNORM;
      case 2:
        return VK_FORMAT_R8G8_SNORM;
      case 3:
        return VK_FORMAT_R8G8B8_SNORM;
      case 4:
        return VK_FORMAT_R8G8B8A8_SNORM;
      default:
        BLI_assert(false);
        return VK_FORMAT_R32_SFLOAT;
      }
  case GPU_COMP_U8:
    switch (size) {
      case 1:
        return VK_FORMAT_R8_UNORM;
      case 2:
        return VK_FORMAT_R8G8_UNORM;
      case 3:
        return VK_FORMAT_R8G8B8_UNORM;
      case 4:
        return VK_FORMAT_R8G8B8A8_UNORM;
      default:
        BLI_assert(false);
        return VK_FORMAT_R32_SFLOAT;
    }

  case GPU_COMP_I16:
    switch (size) {
      case 2:
        return VK_FORMAT_R16_SNORM;
      case 4:
        return VK_FORMAT_R16G16_SNORM;
      case 6:
        return VK_FORMAT_R16G16B16_SNORM;
      case 8:
        return VK_FORMAT_R16G16B16A16_SNORM;
      default:
        BLI_assert(false);
        return VK_FORMAT_R32_SFLOAT;
    }

  case GPU_COMP_U16:
    switch (size) {
      case 2:
        return VK_FORMAT_R16_UNORM;
      case 4:
        return VK_FORMAT_R16G16_UNORM;
      case 6:
        return VK_FORMAT_R16G16B16_UNORM;
      case 8:
        return VK_FORMAT_R16G16B16A16_UNORM;
      default:
        BLI_assert(false);
        return VK_FORMAT_R32_SFLOAT;
    }
  case GPU_COMP_I32:
    switch (size) {
      case 4:
        return VK_FORMAT_R32_SINT;
      case 8:
        return VK_FORMAT_R32G32_SINT;
      case 12:
        return VK_FORMAT_R32G32B32_SINT;
      case 16:
        return VK_FORMAT_R32G32B32A32_SINT;
      default:
        BLI_assert(false);
        return VK_FORMAT_R32_SFLOAT;
    }
  case GPU_COMP_U32:
    switch (size) {
      case 4:
        return VK_FORMAT_R32_UINT;
      case 8:
        return VK_FORMAT_R32G32_UINT;
      case 12:
        return VK_FORMAT_R32G32B32_UINT;
      case 16:
        return VK_FORMAT_R32G32B32A32_UINT;
      default:
        BLI_assert(false);
        return VK_FORMAT_R32_SFLOAT;
    }
  case GPU_COMP_F32:
    switch (size) {
      case 4:
        return VK_FORMAT_R32_SFLOAT;
      case 8:
        return VK_FORMAT_R32G32_SFLOAT;
      case 12:
        return VK_FORMAT_R32G32B32_SFLOAT;
      case 16:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
      case 64:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
      default:
        BLI_assert(false);
        return VK_FORMAT_R32_SFLOAT;
    }
  case GPU_COMP_I10:
    BLI_assert(size == 4);
    //return VK_FORMAT_A2B10G10R10_SINT_PACK32;
    return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
 
  default:
    BLI_assert(0);
    return VK_FORMAT_R32_SFLOAT;
  }
}


}  // namespace gpu
}  // namespace blender
