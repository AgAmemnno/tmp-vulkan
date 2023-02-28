/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */


#include "vk_common.hh"


namespace blender::gpu {



uint32_t makeAccessMaskPipelineStageFlags(
    uint32_t accessMask,
    VkPipelineStageFlags supportedShaderBits
)
{
  static const uint32_t accessPipes[] = {
    VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
    VK_ACCESS_INDEX_READ_BIT,
    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
    VK_ACCESS_UNIFORM_READ_BIT,
    supportedShaderBits,
    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    VK_ACCESS_SHADER_READ_BIT,
    supportedShaderBits,
    VK_ACCESS_SHADER_WRITE_BIT,
    supportedShaderBits,
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
    VK_ACCESS_TRANSFER_READ_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_ACCESS_TRANSFER_WRITE_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_ACCESS_HOST_READ_BIT,
    VK_PIPELINE_STAGE_HOST_BIT,
    VK_ACCESS_HOST_WRITE_BIT,
    VK_PIPELINE_STAGE_HOST_BIT,
    VK_ACCESS_MEMORY_READ_BIT,
    0,
    VK_ACCESS_MEMORY_WRITE_BIT,
    0,
#if VK_NV_device_generated_commands
    VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV,
    VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
    VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV,
    VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
#endif
#if VK_NV_ray_tracing
    VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,
    VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV | supportedShaderBits |
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
    VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV,
    VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
#endif
  };
  if (!accessMask) {
    return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  }

  uint32_t pipes = 0;
#define NV_ARRAY_SIZE(X) (sizeof((X)) / sizeof((X)[0]))

  for (uint32_t i = 0; i < NV_ARRAY_SIZE(accessPipes); i += 2) {
    if (accessPipes[i] & accessMask) {
      pipes |= accessPipes[i + 1];
    }
  }
#undef NV_ARRAY_SIZE
  return pipes;
}

VkImageMemoryBarrier makeImageMemoryBarrier(VkImage img,
                                                   VkAccessFlags srcAccess,
                                                   VkAccessFlags dstAccess,
                                                   VkImageLayout oldLayout,
                                                   VkImageLayout newLayout,
                                                   VkImageAspectFlags aspectMask,
                                                   int basemip ,
                                                   int miplevel)
{
  VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.srcAccessMask = srcAccess;
  barrier.dstAccessMask = dstAccess;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = img;
  barrier.subresourceRange = {0};
  barrier.subresourceRange.baseMipLevel = basemip;
  barrier.subresourceRange.aspectMask = aspectMask;
  barrier.subresourceRange.levelCount = (miplevel == -1) ? VK_REMAINING_MIP_LEVELS : miplevel;
  barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

  return barrier;
}

uint32_t getMemoryTypeIndex(const VkPhysicalDeviceMemoryProperties *pMemoryProperties,uint32_t typeBits,
                            VkMemoryPropertyFlags properties)
{
  // Iterate over all memory types available for the device used in this example
  for (uint32_t i = 0; i < pMemoryProperties->memoryTypeCount; i++) {
    if ((typeBits & 1) == 1) {
      if ((pMemoryProperties->memoryTypes[i].propertyFlags & properties) == properties) {
        return i;
      }
    }
    typeBits >>= 1;
  }

  throw "Could not find a suitable memory type!";
}
uint32_t getMemoryType(const VkPhysicalDeviceMemoryProperties *pMemoryProperties,
                       uint32_t typeBits,
                       VkMemoryPropertyFlags properties,
                       VkBool32 *memTypeFound)
{

  for (uint32_t i = 0; i < pMemoryProperties->memoryTypeCount; i++) {
    if ((typeBits & 1) == 1) {
      if ((pMemoryProperties->memoryTypes[i].propertyFlags & properties) == properties) {
        if (memTypeFound) {
          *memTypeFound = true;
        }
        return i;
      }
    }
    typeBits >>= 1;
  }

  return -1;
  ///    throw  "Could not find a matching memory type";
}

}  // namespace blender::gpu


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
    //case GPU_RGB10_A2UI:
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

    /* Texture only formats.
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
      */
    case GPU_RGB16F:
      return VK_IMAGE_ASPECT_COLOR_BIT;

    /* Special formats, texture only. */
    case GPU_SRGB8_A8_DXT1:
    case GPU_SRGB8_A8_DXT3:
    case GPU_SRGB8_A8_DXT5:
    case GPU_RGBA8_DXT1:
    case GPU_RGBA8_DXT3:
    case GPU_RGBA8_DXT5:
    /*
    case GPU_SRGB8:
    case GPU_RGB9_E5:
    */
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
    /*
    case GPU_RGB10_A2UI:
      return VK_FORMAT_A2B10G10R10_UINT_PACK32;
    */
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

    /* Texture only formats.
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
      */
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
    /*
    case GPU_SRGB8:
      return VK_FORMAT_R8G8B8_SRGB;
    case GPU_RGB9_E5:
      return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
     */

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
  BLI_assert_unreachable();
  return VK_IMAGE_TYPE_MAX_ENUM;
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

  BLI_assert_unreachable();
  return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
}


VkComponentMapping to_vk_component_mapping(const eGPUTextureFormat /*format*/)
{
  /* TODO: this should map to OpenGL defaults based on the eGPUTextureFormat. The implementation of
   * this function will be implemented when implementing other parts of VKTexture.*/
  VkComponentMapping component_mapping;
  component_mapping.r = VK_COMPONENT_SWIZZLE_R;
  component_mapping.g = VK_COMPONENT_SWIZZLE_G;
  component_mapping.b = VK_COMPONENT_SWIZZLE_B;
  component_mapping.a = VK_COMPONENT_SWIZZLE_A;
  return component_mapping;
}

}  // namespace blender::gpu
