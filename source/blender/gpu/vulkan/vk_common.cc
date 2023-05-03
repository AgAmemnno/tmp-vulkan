/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "GPU_context.h"

#include "vk_common.hh"

namespace blender::gpu {
VkImageAspectFlagBits to_vk_image_aspect_flag_bits(const eGPUTextureFormat format)
{
  switch (format) {
    /* Formats texture & render-buffer */
    case GPU_RGBA32UI:
    case GPU_RG32UI:
    case GPU_R32UI:
    case GPU_RGBA16UI:
    case GPU_RG16UI:
    case GPU_R16UI:
    case GPU_RGBA8UI:
    case GPU_RG8UI:
    case GPU_R8UI:
    case GPU_RGBA32I:
    case GPU_RG32I:
    case GPU_R32I:
    case GPU_RGBA16I:
    case GPU_RG16I:
    case GPU_R16I:
    case GPU_RGBA8I:
    case GPU_RG8I:
    case GPU_R8I:
    case GPU_RGBA32F:
    case GPU_RG32F:
    case GPU_R32F:
    case GPU_RGBA16F:
    case GPU_RG16F:
    case GPU_R16F:
    case GPU_RGBA16:
    case GPU_RG16:
    case GPU_R16:
    case GPU_RGBA8:
    case GPU_RG8:
    case GPU_R8:
      return VK_IMAGE_ASPECT_COLOR_BIT;

    /* Special formats texture & render-buffer */
    case GPU_RGB10_A2:
    case GPU_RGB10_A2UI:
    case GPU_R11F_G11F_B10F:
    case GPU_SRGB8_A8:
      return VK_IMAGE_ASPECT_COLOR_BIT;
    case GPU_DEPTH32F_STENCIL8:
    case GPU_DEPTH24_STENCIL8:
      return static_cast<VkImageAspectFlagBits>(VK_IMAGE_ASPECT_DEPTH_BIT |
                                                VK_IMAGE_ASPECT_STENCIL_BIT);

    /* Depth Formats. */
    case GPU_DEPTH_COMPONENT32F:
    case GPU_DEPTH_COMPONENT24:
    case GPU_DEPTH_COMPONENT16:
      return VK_IMAGE_ASPECT_DEPTH_BIT;

    /* Texture only formats. */
    case GPU_RGB32UI:
    case GPU_RGB16UI:
    case GPU_RGB8UI:
    case GPU_RGB32I:
    case GPU_RGB16I:
    case GPU_RGB8I:
    case GPU_RGB16:
    case GPU_RGB8:
    case GPU_RGBA16_SNORM:
    case GPU_RGB16_SNORM:
    case GPU_RG16_SNORM:
    case GPU_R16_SNORM:
    case GPU_RGBA8_SNORM:
    case GPU_RGB8_SNORM:
    case GPU_RG8_SNORM:
    case GPU_R8_SNORM:
    case GPU_RGB32F:
    case GPU_RGB16F:
      return VK_IMAGE_ASPECT_COLOR_BIT;

    /* Special formats, texture only. */
    case GPU_SRGB8_A8_DXT1:
    case GPU_SRGB8_A8_DXT3:
    case GPU_SRGB8_A8_DXT5:
    case GPU_RGBA8_DXT1:
    case GPU_RGBA8_DXT3:
    case GPU_RGBA8_DXT5:
    case GPU_SRGB8:
    case GPU_RGB9_E5:
      return VK_IMAGE_ASPECT_COLOR_BIT;
  }
  BLI_assert_unreachable();
  return static_cast<VkImageAspectFlagBits>(0);
}

VkFormat to_vk_format(const eGPUTextureFormat format)
{
  switch (format) {
    /* Formats texture & render-buffer */
    case GPU_RGBA32UI:
      return VK_FORMAT_R32G32B32A32_UINT;
    case GPU_RG32UI:
      return VK_FORMAT_R32G32_UINT;
    case GPU_R32UI:
      return VK_FORMAT_R32_UINT;
    case GPU_RGBA16UI:
      return VK_FORMAT_R16G16B16A16_UINT;
    case GPU_RG16UI:
      return VK_FORMAT_R16G16_UINT;
    case GPU_R16UI:
      return VK_FORMAT_R16_UINT;
    case GPU_RGBA8UI:
      return VK_FORMAT_R8G8B8A8_UINT;
    case GPU_RG8UI:
      return VK_FORMAT_R8G8_UINT;
    case GPU_R8UI:
      return VK_FORMAT_R8_UINT;
    case GPU_RGBA32I:
      return VK_FORMAT_R32G32B32A32_SINT;
    case GPU_RG32I:
      return VK_FORMAT_R32G32_SINT;
    case GPU_R32I:
      return VK_FORMAT_R32_SINT;
    case GPU_RGBA16I:
      return VK_FORMAT_R16G16B16A16_SINT;
    case GPU_RG16I:
      return VK_FORMAT_R16G16_SINT;
    case GPU_R16I:
      return VK_FORMAT_R16_SINT;
    case GPU_RGBA8I:
      return VK_FORMAT_R8G8B8A8_SINT;
    case GPU_RG8I:
      return VK_FORMAT_R8G8_SINT;
    case GPU_R8I:
      return VK_FORMAT_R8_SINT;
    case GPU_RGBA32F:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case GPU_RG32F:
      return VK_FORMAT_R32G32_SFLOAT;
    case GPU_R32F:
      return VK_FORMAT_R32_SFLOAT;
    case GPU_RGBA16F:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    case GPU_RG16F:
      return VK_FORMAT_R16G16_SFLOAT;
    case GPU_R16F:
      return VK_FORMAT_R16_SFLOAT;
    case GPU_RGBA16:
      return VK_FORMAT_R16G16B16A16_UNORM;
    case GPU_RG16:
      return VK_FORMAT_R16G16_UNORM;
    case GPU_R16:
      return VK_FORMAT_R16_UNORM;
    case GPU_RGBA8:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case GPU_RG8:
      return VK_FORMAT_R8G8_UNORM;
    case GPU_R8:
      return VK_FORMAT_R8_UNORM;

    /* Special formats texture & render-buffer */
    case GPU_RGB10_A2:
      return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case GPU_RGB10_A2UI:
      return VK_FORMAT_A2B10G10R10_UINT_PACK32;
    case GPU_R11F_G11F_B10F:
      return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case GPU_SRGB8_A8:
      return VK_FORMAT_R8G8B8A8_SRGB;
    case GPU_DEPTH32F_STENCIL8:
      return VK_FORMAT_D32_SFLOAT_S8_UINT;
    case GPU_DEPTH24_STENCIL8:
      return VK_FORMAT_D24_UNORM_S8_UINT;

    /* Depth Formats. */
    case GPU_DEPTH_COMPONENT32F:
      return VK_FORMAT_D32_SFLOAT;
    case GPU_DEPTH_COMPONENT24:
      return VK_FORMAT_X8_D24_UNORM_PACK32;
    case GPU_DEPTH_COMPONENT16:
      return VK_FORMAT_D16_UNORM;

    /* Texture only formats. */
    case GPU_RGB32UI:
      return VK_FORMAT_R32G32B32_UINT;
    case GPU_RGB16UI:
      return VK_FORMAT_R16G16B16_UINT;
    case GPU_RGB8UI:
      return VK_FORMAT_R8G8B8_UINT;
    case GPU_RGB32I:
      return VK_FORMAT_R32G32B32_SINT;
    case GPU_RGB16I:
      return VK_FORMAT_R16G16B16_SINT;
    case GPU_RGB8I:
      return VK_FORMAT_R8G8B8_SINT;
    case GPU_RGB16:
      return VK_FORMAT_R16G16B16_UNORM;
    case GPU_RGB8:
      return VK_FORMAT_R8G8B8_UNORM;
    case GPU_RGBA16_SNORM:
      return VK_FORMAT_R16G16B16A16_SNORM;
    case GPU_RGB16_SNORM:
      return VK_FORMAT_R16G16B16_SNORM;
    case GPU_RG16_SNORM:
      return VK_FORMAT_R16G16_SNORM;
    case GPU_R16_SNORM:
      return VK_FORMAT_R16_SNORM;
    case GPU_RGBA8_SNORM:
      return VK_FORMAT_R8G8B8A8_SNORM;
    case GPU_RGB8_SNORM:
      return VK_FORMAT_R8G8B8_SNORM;
    case GPU_RG8_SNORM:
      return VK_FORMAT_R8G8_SNORM;
    case GPU_R8_SNORM:
      return VK_FORMAT_R8_SNORM;
    case GPU_RGB32F:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case GPU_RGB16F:
      return VK_FORMAT_R16G16B16_SFLOAT;

    /* Special formats, texture only. */
    case GPU_SRGB8_A8_DXT1:
      return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    case GPU_SRGB8_A8_DXT3:
      return VK_FORMAT_BC2_SRGB_BLOCK;
    case GPU_SRGB8_A8_DXT5:
      return VK_FORMAT_BC3_SRGB_BLOCK;
    case GPU_RGBA8_DXT1:
      return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case GPU_RGBA8_DXT3:
      return VK_FORMAT_BC2_UNORM_BLOCK;
    case GPU_RGBA8_DXT5:
      return VK_FORMAT_BC3_UNORM_BLOCK;
    case GPU_SRGB8:
      return VK_FORMAT_R8G8B8_SRGB;
    case GPU_RGB9_E5:
      return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
  }
  return VK_FORMAT_UNDEFINED;
}

VkImageType to_vk_image_type(const eGPUTextureType type)
{
  switch (type) {
    case GPU_TEXTURE_1D:
    case GPU_TEXTURE_BUFFER:
    case GPU_TEXTURE_1D_ARRAY:
      return VK_IMAGE_TYPE_1D;
    case GPU_TEXTURE_2D:
    case GPU_TEXTURE_2D_ARRAY:
      return VK_IMAGE_TYPE_2D;
    case GPU_TEXTURE_3D:
    case GPU_TEXTURE_CUBE:
    case GPU_TEXTURE_CUBE_ARRAY:
      return VK_IMAGE_TYPE_3D;

    case GPU_TEXTURE_ARRAY:
      /* GPU_TEXTURE_ARRAY should always be used together with 1D, 2D, or CUBE*/
      BLI_assert_unreachable();
      break;
  }

  return VK_IMAGE_TYPE_1D;
}

VkImageViewType to_vk_image_view_type(const eGPUTextureType type)
{
  switch (type) {
    case GPU_TEXTURE_1D:
    case GPU_TEXTURE_BUFFER:
      return VK_IMAGE_VIEW_TYPE_1D;
    case GPU_TEXTURE_2D:
      return VK_IMAGE_VIEW_TYPE_2D;
    case GPU_TEXTURE_3D:
      return VK_IMAGE_VIEW_TYPE_3D;
    case GPU_TEXTURE_CUBE:
      return VK_IMAGE_VIEW_TYPE_CUBE;
    case GPU_TEXTURE_1D_ARRAY:
      return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    case GPU_TEXTURE_2D_ARRAY:
      return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case GPU_TEXTURE_CUBE_ARRAY:
      return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;

    case GPU_TEXTURE_ARRAY:
      /* GPU_TEXTURE_ARRAY should always be used together with 1D, 2D, or CUBE*/
      BLI_assert_unreachable();
      break;
  }

  return VK_IMAGE_VIEW_TYPE_1D;
}

VkComponentMapping to_vk_component_mapping(const eGPUTextureFormat /*format*/)
{
  /* TODO: this should map to OpenGL defaults based on the eGPUTextureFormat. The implementation of
   * this function will be implemented when implementing other parts of VKTexture. */
  VkComponentMapping component_mapping;
  component_mapping.r = VK_COMPONENT_SWIZZLE_R;
  component_mapping.g = VK_COMPONENT_SWIZZLE_G;
  component_mapping.b = VK_COMPONENT_SWIZZLE_B;
  component_mapping.a = VK_COMPONENT_SWIZZLE_A;
  return component_mapping;
}

template<typename T> void copy_color(T dst[4], const T *src)
{
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

VkClearColorValue to_vk_clear_color_value(const eGPUDataFormat format, const void *data)
{
  VkClearColorValue result = {0.0f};
  switch (format) {
    case GPU_DATA_FLOAT: {
      const float *float_data = static_cast<const float *>(data);
      copy_color<float>(result.float32, float_data);
      break;
    }

    case GPU_DATA_INT: {
      const int32_t *int_data = static_cast<const int32_t *>(data);
      copy_color<int32_t>(result.int32, int_data);
      break;
    }

    case GPU_DATA_UINT: {
      const uint32_t *uint_data = static_cast<const uint32_t *>(data);
      copy_color<uint32_t>(result.uint32, uint_data);
      break;
    }

    case GPU_DATA_HALF_FLOAT:
    case GPU_DATA_UBYTE:
    case GPU_DATA_UINT_24_8:
    case GPU_DATA_10_11_11_REV:
    case GPU_DATA_2_10_10_10_REV: {
      BLI_assert_unreachable();
      break;
    }
  }
  return result;
}

const char *to_string(VkImageLayout layout)
{

#define FORMAT_IMAGE_LAYOUT(X) \
  case X: { \
    return "" #X; \
  }

  switch (layout) {
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_UNDEFINED)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_GENERAL)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_PREINITIALIZED)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
#ifdef VK_ENABLE_BETA_EXTENSIONS
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR)
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR)
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR)
#endif
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR)
#ifdef VK_ENABLE_BETA_EXTENSIONS
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR)
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR)
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR)
#endif
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR)
    FORMAT_IMAGE_LAYOUT(VK_IMAGE_LAYOUT_MAX_ENUM)

    default:
      return "Unknown Error";
  }

#undef FORMAT_IMAGE_LAYOUT
}

const char* to_string(VkFormat vk_format)
{
  #define VK_FORMAT_2_STRING(X) \
  case X: { \
    return "" #X; \
  }
  switch(vk_format)
  {
    VK_FORMAT_2_STRING(VK_FORMAT_UNDEFINED);
    VK_FORMAT_2_STRING(VK_FORMAT_R4G4_UNORM_PACK8);
    VK_FORMAT_2_STRING(VK_FORMAT_R4G4B4A4_UNORM_PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_B4G4R4A4_UNORM_PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_R5G6B5_UNORM_PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_B5G6R5_UNORM_PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_R5G5B5A1_UNORM_PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_B5G5R5A1_UNORM_PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_A1R5G5B5_UNORM_PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_R8_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R8_SNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R8_USCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_R8_SSCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_R8_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R8_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R8_SRGB);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8_SNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8_USCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8_SSCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8_SRGB);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8B8_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8B8_SNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8B8_USCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8B8_SSCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8B8_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8B8_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8B8_SRGB);
    VK_FORMAT_2_STRING(VK_FORMAT_B8G8R8_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_B8G8R8_SNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_B8G8R8_USCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_B8G8R8_SSCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_B8G8R8_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_B8G8R8_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_B8G8R8_SRGB);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8B8A8_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8B8A8_SNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8B8A8_USCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8B8A8_SSCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8B8A8_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8B8A8_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R8G8B8A8_SRGB);
    VK_FORMAT_2_STRING(VK_FORMAT_B8G8R8A8_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_B8G8R8A8_SNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_B8G8R8A8_USCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_B8G8R8A8_SSCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_B8G8R8A8_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_B8G8R8A8_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_B8G8R8A8_SRGB);
    VK_FORMAT_2_STRING(VK_FORMAT_A8B8G8R8_UNORM_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A8B8G8R8_SNORM_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A8B8G8R8_USCALED_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A8B8G8R8_SSCALED_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A8B8G8R8_UINT_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A8B8G8R8_SINT_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A8B8G8R8_SRGB_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A2R10G10B10_UNORM_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A2R10G10B10_SNORM_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A2R10G10B10_USCALED_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A2R10G10B10_SSCALED_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A2R10G10B10_UINT_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A2R10G10B10_SINT_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A2B10G10R10_SNORM_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A2B10G10R10_USCALED_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A2B10G10R10_SSCALED_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A2B10G10R10_UINT_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_A2B10G10R10_SINT_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_R16_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R16_SNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R16_USCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_R16_SSCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_R16_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R16_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R16_SFLOAT);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16_SNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16_USCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16_SSCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16_SFLOAT);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16B16_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16B16_SNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16B16_USCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16B16_SSCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16B16_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16B16_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16B16_SFLOAT);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16B16A16_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16B16A16_SNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16B16A16_USCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16B16A16_SSCALED);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16B16A16_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16B16A16_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R16G16B16A16_SFLOAT);
    VK_FORMAT_2_STRING(VK_FORMAT_R32_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R32_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R32_SFLOAT);
    VK_FORMAT_2_STRING(VK_FORMAT_R32G32_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R32G32_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R32G32_SFLOAT);
    VK_FORMAT_2_STRING(VK_FORMAT_R32G32B32_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R32G32B32_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R32G32B32_SFLOAT);
    VK_FORMAT_2_STRING(VK_FORMAT_R32G32B32A32_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R32G32B32A32_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R32G32B32A32_SFLOAT);
    VK_FORMAT_2_STRING(VK_FORMAT_R64_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R64_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R64_SFLOAT);
    VK_FORMAT_2_STRING(VK_FORMAT_R64G64_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R64G64_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R64G64_SFLOAT);
    VK_FORMAT_2_STRING(VK_FORMAT_R64G64B64_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R64G64B64_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R64G64B64_SFLOAT);
    VK_FORMAT_2_STRING(VK_FORMAT_R64G64B64A64_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R64G64B64A64_SINT);
    VK_FORMAT_2_STRING(VK_FORMAT_R64G64B64A64_SFLOAT);
    VK_FORMAT_2_STRING(VK_FORMAT_B10G11R11_UFLOAT_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_E5B9G9R9_UFLOAT_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_D16_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_X8_D24_UNORM_PACK32);
    VK_FORMAT_2_STRING(VK_FORMAT_D32_SFLOAT);
    VK_FORMAT_2_STRING(VK_FORMAT_S8_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_D16_UNORM_S8_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_D24_UNORM_S8_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_D32_SFLOAT_S8_UINT);
    VK_FORMAT_2_STRING(VK_FORMAT_BC1_RGB_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_BC1_RGB_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_BC1_RGBA_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_BC2_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_BC2_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_BC3_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_BC3_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_BC4_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_BC4_SNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_BC5_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_BC5_SNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_BC6H_UFLOAT_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_BC6H_SFLOAT_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_BC7_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_BC7_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_EAC_R11_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_EAC_R11_SNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_EAC_R11G11_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_EAC_R11G11_SNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_5x4_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_5x4_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_5x5_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_5x5_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_6x5_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_6x5_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_6x6_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_6x6_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_8x5_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_8x5_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_8x6_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_8x6_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_8x8_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_8x8_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_10x5_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_10x5_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_10x6_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_10x6_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_10x8_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_10x8_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_10x10_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_10x10_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_12x10_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_12x10_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_12x12_UNORM_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_12x12_SRGB_BLOCK);
    VK_FORMAT_2_STRING(VK_FORMAT_G8B8G8R8_422_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_B8G8R8G8_422_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_G8_B8R8_2PLANE_420_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_G8_B8R8_2PLANE_422_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_R10X6_UNORM_PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_R10X6G10X6_UNORM_2PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_R12X4_UNORM_PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_R12X4G12X4_UNORM_2PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16);
    VK_FORMAT_2_STRING(VK_FORMAT_G16B16G16R16_422_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_B16G16R16G16_422_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_G16_B16R16_2PLANE_420_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_G16_B16R16_2PLANE_422_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM);
    VK_FORMAT_2_STRING(VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG);
    VK_FORMAT_2_STRING(VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG);
    VK_FORMAT_2_STRING(VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG);
    VK_FORMAT_2_STRING(VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG);
    VK_FORMAT_2_STRING(VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG);
    VK_FORMAT_2_STRING(VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG);
    VK_FORMAT_2_STRING(VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG);
    VK_FORMAT_2_STRING(VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT);
    VK_FORMAT_2_STRING(VK_FORMAT_MAX_ENUM);
    default:
    BLI_assert_unreachable();
  };
  #undef VK_FORMAT_2_STRING
  return "NOT FOUND";
};

VkPrimitiveTopology to_vk(const GPUPrimType prim_type)
{

  switch (prim_type) {
    case GPU_PRIM_POINTS:
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case GPU_PRIM_LINES:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case GPU_PRIM_LINES_ADJ:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
    case GPU_PRIM_LINE_LOOP:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case GPU_PRIM_LINE_STRIP:
      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case GPU_PRIM_LINE_STRIP_ADJ:
      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
    case GPU_PRIM_TRIS:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case GPU_PRIM_TRIS_ADJ:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
    case GPU_PRIM_TRI_FAN:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    case GPU_PRIM_TRI_STRIP:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case GPU_PRIM_NONE:
      return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
  };
  return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

VkFormat to_vk_format(const GPUVertCompType type, const uint32_t size)
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
VkFormat to_vk_format_float(const GPUVertCompType type, const uint32_t size)
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
          return VK_FORMAT_R32_SFLOAT;
        case 8:
          return VK_FORMAT_R32G32_SFLOAT;
        case 12:
          return VK_FORMAT_R32G32B32_SFLOAT;
        case 16:
          return VK_FORMAT_R32G32B32A32_SFLOAT;
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
VkFormat to_vk_format(const GPUVertCompType type, const uint32_t size,GPUVertFetchMode fetch_mode)
{
  switch (fetch_mode) {
    case GPU_FETCH_FLOAT:
    case GPU_FETCH_INT:
    case GPU_FETCH_INT_TO_FLOAT_UNIT: /* 127 (ubyte) -> 0.5 (and so on for other int types) */
      return to_vk_format(type,size);
      break;
    case GPU_FETCH_INT_TO_FLOAT:
      return to_vk_format_float(type,size);
    default:
      break;
  }
  BLI_assert_unreachable();
  return VK_FORMAT_R32_SFLOAT;
}
VkIndexType to_vk_index_type(const GPUIndexBufType index_type)
{
  switch (index_type) {
    case GPU_INDEX_U16:
      return VK_INDEX_TYPE_UINT16;
    case GPU_INDEX_U32:
      return VK_INDEX_TYPE_UINT32;
    default:
      BLI_assert_unreachable();
      break;
  }
  return VK_INDEX_TYPE_UINT16;
}

VkPrimitiveTopology to_vk_primitive_topology(const GPUPrimType prim_type)
{
  switch (prim_type) {
    case GPU_PRIM_POINTS:
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case GPU_PRIM_LINES:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case GPU_PRIM_TRIS:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case GPU_PRIM_LINE_STRIP:
      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case GPU_PRIM_LINE_LOOP:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case GPU_PRIM_TRI_STRIP:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case GPU_PRIM_TRI_FAN:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    case GPU_PRIM_LINES_ADJ:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
    case GPU_PRIM_TRIS_ADJ:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
    case GPU_PRIM_LINE_STRIP_ADJ:
      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;

    case GPU_PRIM_NONE:
      break;
  }
  BLI_assert_unreachable();
  return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
}

VkCullModeFlags to_vk_cull_mode_flags(const eGPUFaceCullTest cull_test)
{
  switch (cull_test) {
    case GPU_CULL_FRONT:
      return VK_CULL_MODE_FRONT_BIT;
    case GPU_CULL_BACK:
      return VK_CULL_MODE_BACK_BIT;
    case GPU_CULL_NONE:
      return VK_CULL_MODE_NONE;
  }
  BLI_assert_unreachable();
  return VK_CULL_MODE_NONE;
}

}  // namespace blender::gpu
