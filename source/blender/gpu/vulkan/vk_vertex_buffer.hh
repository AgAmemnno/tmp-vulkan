/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_vertex_buffer_private.hh"

#include "vk_buffer.hh"

namespace blender::gpu {

class VKVertexBuffer : public VertBuf {
  VKBuffer buffer_;

 public:
  ~VKVertexBuffer();

  void bind_as_ssbo(uint binding) override;
  void bind_as_texture(uint binding) override;
  void wrap_handle(uint64_t handle) override;

  void update_sub(uint start, uint len, const void *data) override;
  void read(void *data) const override;

  void bind();

  VkBuffer vk_handle() const
  {
    BLI_assert(buffer_.is_allocated());
    return buffer_.vk_handle();
  }

 protected:
  void acquire_data() override;
  void resize_data() override;
  void release_data() override;
  void upload_data() override;
  void duplicate_data(VertBuf *dst) override;

 private:
  void allocate(VKContext &context);
};

static inline VKVertexBuffer *unwrap(VertBuf *vertex_buffer)
{
  return static_cast<VKVertexBuffer *>(vertex_buffer);
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
      // return VK_FORMAT_A2B10G10R10_SINT_PACK32;
      return VK_FORMAT_A2B10G10R10_UNORM_PACK32;

    default:
      BLI_assert(0);
      return VK_FORMAT_R32_SFLOAT;
  }
}
}  // namespace blender::gpu
