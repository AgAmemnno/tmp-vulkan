/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#ifdef __APPLE__
#  include <MoltenVK/vk_mvk_moltenvk.h>
#else
#  include <vulkan/vulkan.h>
#endif

#include "gpu_texture_private.hh"

namespace blender::gpu {

VkImageAspectFlagBits to_vk_image_aspect_flag_bits(const eGPUTextureFormat format);
VkFormat to_vk_format(const eGPUTextureFormat format);
VkComponentMapping to_vk_component_mapping(const eGPUTextureFormat format);
VkImageViewType to_vk_image_view_type(const eGPUTextureType type);
VkImageType to_vk_image_type(const eGPUTextureType type);

uint32_t getMemoryTypeIndex(const VkPhysicalDeviceMemoryProperties *pMemoryProperties,
                            uint32_t typeBits,
                            VkMemoryPropertyFlags properties);
uint32_t getMemoryType(const VkPhysicalDeviceMemoryProperties *pMemoryProperties,
                       uint32_t typeBits,
                       VkMemoryPropertyFlags properties,
                       VkBool32 *memTypeFound = nullptr);
uint32_t makeAccessMaskPipelineStageFlags(
    uint32_t accessMask,
    VkPipelineStageFlags supportedShaderBits = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    // VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
    // VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
    // VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
    // VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)
);

VkImageMemoryBarrier makeImageMemoryBarrier(VkImage img,
                                            VkAccessFlags srcAccess,
                                            VkAccessFlags dstAccess,
                                            VkImageLayout oldLayout,
                                            VkImageLayout newLayout,
                                            VkImageAspectFlags aspectMask,
                                            int basemip = 0,
                                            int miplevel = -1);

}  // namespace blender::gpu
