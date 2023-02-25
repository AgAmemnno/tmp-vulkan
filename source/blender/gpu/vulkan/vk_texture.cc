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
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#include "GPU_capabilities.h"
#include "GPU_framebuffer.h"
#include "GPU_platform.h"
#include "vk_context.hh"
#include "vk_memory.hh"

#include "vk_state.hh"
#include "vk_texture.hh"
#include "vk_debug.hh"
#include "vk_framebuffer.hh"

#include "intern/GHOST_ContextVK.h"

namespace blender::gpu {


struct ImgState {
  VkImage img;
  VkImageLayout layout;
  VkAccessFlags acs;
  VkPipelineStageFlags stg;
  VkImageAspectFlags iacs;
};


static bool check_canBlit(VkFormat srcFormat,VkFormat dstFormat)
{

  VKContext *context = VKContext::get();
  BLI_assert(context);
  auto pdevice = context->get_physical_device();
  VkFormatProperties formatProps;
  vkGetPhysicalDeviceFormatProperties(pdevice, srcFormat, &formatProps);
  if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
    std::cerr << "Device does not support blitting from optimal tiled images, using copy instead "
                 "of blit!"
              << std::endl;
    return false;
  }

  // Check if the device supports blitting to linear images
  vkGetPhysicalDeviceFormatProperties(pdevice, srcFormat, &formatProps);
  if (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
    std::cerr
        << "Device does not support blitting to linear tiled images, using copy instead of blit!"
        << std::endl;
    return false;
  }
  return true;
}

static  void image_barrier_copy(VkCommandBuffer copyCmd, VKTexture *tex, ImgState &state, bool is_src,int mip) {
  state.img = tex->get_image();
  state.layout = tex->get_image_layout(mip);

  if (state.layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    state.acs = (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    state.stg = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    state.iacs = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  else if (state.layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {

    state.acs = (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    state.stg = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    state.iacs = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  else {
    state.acs = VK_ACCESS_MEMORY_READ_BIT;
    state.stg = VK_PIPELINE_STAGE_TRANSFER_BIT;
    if (state.layout == VK_IMAGE_LAYOUT_UNDEFINED) {
      if (tex->info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        state.iacs = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      }
      else {
        state.iacs = VK_IMAGE_ASPECT_COLOR_BIT;
      }
    }
    
  };
  insert_image_memory_barrier(
      copyCmd,
      state.img,
      state.acs,
      (is_src) ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT,
      state.layout,
      (is_src) ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      state.stg,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VkImageSubresourceRange{state.iacs, (uint32_t)mip, 1, 0, 1});

};

static void image_barrier2_copy(VkCommandBuffer copyCmd, ImgState &state, bool is_src,int mip) {
  if (state.layout == VK_IMAGE_LAYOUT_UNDEFINED) {

    if (state.iacs == VK_IMAGE_ASPECT_COLOR_BIT) {
      state.acs = (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
      state.stg = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      state.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    else {
      state.acs = (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                   VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
      state.stg = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      state.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

  }

  insert_image_memory_barrier(
      copyCmd,
      state.img,
      (is_src) ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT,
      state.acs,
      (is_src) ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      state.layout,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      state.stg,
      VkImageSubresourceRange{state.iacs, (uint32_t)mip, 1, 0, 1});
};




static void sampler2attachment(VKTexture *tex,
                               int mip,
                               VkImageLayout dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
{
  VkImageLayout src_layout = tex->get_image_layout(mip);
  if (src_layout == dst_layout) {
    return;
  }

  VkAccessFlags src_acs_flag; 
  const VkImageLayout Shader_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  if (src_layout & VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    src_acs_flag = VK_ACCESS_SHADER_READ_BIT;
  }
  else if (src_layout & VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
    src_acs_flag = VK_ACCESS_MEMORY_READ_BIT;
  }

  BLI_assert((dst_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) ||
                (dst_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

  VkAccessFlags acs_flag = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  if (dst_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    acs_flag = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  }

  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VKContext *context = VKContext::get();

  VkImage src_image = tex->get_image();

  context->begin_submit_simple(cmd);

  blender::vulkan::GHOST_ImageTransition(cmd,
                                         src_image,
                                         tex->info.format,
                                         dst_layout,
                                         acs_flag,
                                         VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM,
                                         src_acs_flag,
                                         src_layout);

  context->end_submit_simple();
  tex->set_image_layout(dst_layout, mip);


};



    /// <summary>
/// @SaschaWillems , Vulkan Samples-1
/// https://github.com/Mandar-Shinde/Vulkan-Samples-1/blob/master/samples/api/texture_mipmap_generation/texture_mipmap_generation_tutorial.md
/// </summary>
///
///

void insert_image_memory_barrier(VkCommandBuffer command_buffer,
                                 VkImage image,
                                 VkAccessFlags src_access_mask,
                                 VkAccessFlags dst_access_mask,
                                 VkImageLayout old_layout,
                                 VkImageLayout new_layout,
                                 VkPipelineStageFlags src_stage_mask,
                                 VkPipelineStageFlags dst_stage_mask,
                                 VkImageSubresourceRange subresource_range)
{
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.srcAccessMask = src_access_mask;
  barrier.dstAccessMask = dst_access_mask;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.image = image;
  barrier.subresourceRange = subresource_range;

  vkCmdPipelineBarrier(
      command_buffer, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
};
static VkDeviceSize get_size_fromformat(VkFormat format, uint32_t w_, uint32_t h_, uint32_t d_)
{

  VkDeviceSize size = 0;
  switch (format) {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
      size = w_ * h_ * d_;
      break;
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R16_SFLOAT:
      size = 2 * w_ * h_ * d_;
      break;
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8_USCALED:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8_SRGB:
      size = 3 * w_ * h_ * d_;
      break;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_R32_SFLOAT:
      size = 4 * w_ * h_ * d_;
      break;

    default:
      /*TODO :: rest type*/
      BLI_assert(false);
      return 0;
      break;
  }
  return size;
}
void VKTexture::generate_mipmaps(const void *data)
{

  BLI_assert(vk_image_);
  BLI_assert(this->mipmaps_ > 0);

  VkFormatProperties formatProperties;
  auto pdevice = context_->get_physical_device();
  vkGetPhysicalDeviceFormatProperties(pdevice, to_vk(format_), &formatProperties);
  if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) ||
      !(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
    BLI_assert_msg(false, "Selected image format does not support blit source and destination");
  };

  VkDeviceSize size = get_size_fromformat(info.format, w_, h_, d_);

  VKStagingBufferManager *staging = context_->buffer_manager_;

  VKBuffer *buffer = staging->Create(size, 256);

  memcpy(buffer->get_host_ptr(), data, size);
  buffer->unmap();

  auto cmd = staging->begin();

  insert_image_memory_barrier(cmd,
                              vk_image_,
                              0,
                              VK_ACCESS_TRANSFER_WRITE_BIT,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  // Copy the first mip of the chain, remaining mips will be generated
  VkBufferImageCopy buffer_copy_region = {};
  buffer_copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  buffer_copy_region.imageSubresource.mipLevel = 0;
  buffer_copy_region.imageSubresource.baseArrayLayer = 0;
  buffer_copy_region.imageSubresource.layerCount = 1;
  buffer_copy_region.imageExtent.width = w_;
  buffer_copy_region.imageExtent.height = h_;
  buffer_copy_region.imageExtent.depth = 1;
  vkCmdCopyBufferToImage(cmd,
                         buffer->get_vk_buffer(),
                         vk_image_,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1,
                         &buffer_copy_region);

  vk_image_layout_.resize(mipmaps_);


  if (this->mipmaps_ > 1) {
    insert_image_memory_barrier(cmd,
                                vk_image_,
                                VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_ACCESS_TRANSFER_READ_BIT,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
  }
  else {
    // After the loop, all mip layers are in TRANSFER_SRC layout, so transition all to SHADER_READ
    insert_image_memory_barrier(cmd,
                                vk_image_,
                                VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_ACCESS_SHADER_READ_BIT,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                {VK_IMAGE_ASPECT_COLOR_BIT, 0, (uint32_t)this->mipmaps_, 0, 1});


    for (auto &layout : vk_image_layout_) {
       layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

  }

  staging->end();

  if (this->mipmaps_ > 1) {

    staging->begin();

    // Copy down mips from n-1 to n
    for (uint32_t i = 1; i < this->mipmaps_; i++) {
      VkImageBlit image_blit{};

      // Source
      image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      image_blit.srcSubresource.layerCount = 1;
      image_blit.srcSubresource.mipLevel = i - 1;
      image_blit.srcOffsets[1].x = int32_t(w_ >> (i - 1));
      image_blit.srcOffsets[1].y = int32_t(h_ >> (i - 1));
      image_blit.srcOffsets[1].z = 1;

      // Destination
      image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      image_blit.dstSubresource.layerCount = 1;
      image_blit.dstSubresource.mipLevel = i;
      image_blit.dstOffsets[1].x = int32_t(w_ >> (i));
      image_blit.dstOffsets[1].y = int32_t(h_ >> (i));
      image_blit.dstOffsets[1].z = 1;

      // Prepare current mip level as image blit destination
      insert_image_memory_barrier(cmd,
                                  vk_image_,
                                  0,
                                  VK_ACCESS_TRANSFER_WRITE_BIT,
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  {VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1});

      // Blit from previous level
      vkCmdBlitImage(cmd,
                     vk_image_,
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     vk_image_,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     1,
                     &image_blit,
                     VK_FILTER_LINEAR);

      // Prepare current mip level as image blit source for next level
      insert_image_memory_barrier(cmd,
                                  vk_image_,
                                  VK_ACCESS_TRANSFER_WRITE_BIT,
                                  VK_ACCESS_TRANSFER_READ_BIT,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  {VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1});
    }

    // After the loop, all mip layers are in TRANSFER_SRC layout, so transition all to SHADER_READ
    insert_image_memory_barrier(cmd,
                                vk_image_,
                                VK_ACCESS_TRANSFER_READ_BIT,
                                VK_ACCESS_SHADER_READ_BIT,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                {VK_IMAGE_ASPECT_COLOR_BIT, 0, (uint32_t)this->mipmaps_, 0, 1});



    for (auto &layout : vk_image_layout_) {
     layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    staging->end();
  }

  mip_max_ = mipmaps_;
}

void VKTexture::TextureSubImage(int mip, int offset[3], int extent[3], const void *data)
{

  uint32_t row_pitch = ((VKStateManager*)VKContext::get()->state_manager)->unpack_row_length;

  int dim = dimensions_count();

  BLI_assert(vk_image_);
  BLI_assert(this->mipmaps_ > 0 && mip < this->mipmaps_);
  auto &vk_image_layout = vk_image_layout_[mip];

  if (dim == 1) {
    extent[1] = extent[2] = 1;
    offset[1] = offset[2] = 0;
  }
  else if (dim == 2) {
    extent[2] = 1;
    offset[2] = 0;
  }
  VkDeviceSize size = 0; 
  if (row_pitch != 0 ) {
    size = get_size_fromformat(info.format, row_pitch, extent[1], extent[2]);
  }
  else {
    size = get_size_fromformat(info.format, extent[0], extent[1], extent[2]);
  }

  VkImageAspectFlagBits aspect_flag = VK_IMAGE_ASPECT_COLOR_BIT;
  VkImageLayout dst_layout;
  VkAccessFlags dst_access = 0;
  VkPipelineStageFlagBits dst_stage = VK_PIPELINE_STAGE_NONE_KHR;
  /* Restrictions on how flags can be combined.
   * https://vulkan.lunarg.com/doc/view/1.3.231.1/windows/1.3-extensions/vkspec.html#VkRenderPassFragmentDensityMapCreateInfoEXT*/
  if (format_flag_ & GPU_FORMAT_DEPTH) {
    BLI_assert(info.format == VK_FORMAT_X8_D24_UNORM_PACK32);
    aspect_flag = VK_IMAGE_ASPECT_DEPTH_BIT;
    dst_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    dst_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dst_stage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  }
  else {
    if ((info.usage & VK_IMAGE_USAGE_SAMPLED_BIT) ||
        (info.usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)) {
      dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      dst_access = VK_ACCESS_SHADER_READ_BIT;
      dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if ( ( info.usage & VK_IMAGE_USAGE_STORAGE_BIT ) )
    {
      dst_layout  = VK_IMAGE_LAYOUT_GENERAL;
      dst_access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
      dst_stage   = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if ((info.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) {
      dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      dst_access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else {
      BLI_assert(false);
    }
  };

  VKStagingBufferManager *staging = context_->buffer_manager_;

  VKBuffer *buffer = staging->Create(size, 256);

  memcpy(buffer->get_host_ptr(), data, size);
  buffer->unmap();

  auto cmd = staging->begin();

  insert_image_memory_barrier(cmd,
                              vk_image_,
                              (vk_image_layout == dst_layout) ? dst_access : 0,
                              VK_ACCESS_TRANSFER_WRITE_BIT,
                              vk_image_layout,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              (vk_image_layout == dst_layout) ? dst_stage :
                                                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              {(uint32_t)aspect_flag, (uint32_t)mip, 1, 0, 1});

  // Copy the first mip of the chain, remaining mips will be generated
  VkBufferImageCopy buffer_copy_region = {};
  buffer_copy_region.imageSubresource.aspectMask = aspect_flag;
  buffer_copy_region.imageSubresource.mipLevel = mip;
  buffer_copy_region.imageSubresource.baseArrayLayer = 0;
  buffer_copy_region.imageSubresource.layerCount = 1;
  buffer_copy_region.imageExtent.width = extent[0];
  buffer_copy_region.imageExtent.height = extent[1];
  buffer_copy_region.imageExtent.depth = extent[2];
  buffer_copy_region.imageOffset.x = offset[0];
  buffer_copy_region.imageOffset.y = offset[1];
  buffer_copy_region.imageOffset.z = offset[2];
  buffer_copy_region.bufferRowLength = row_pitch;


  vkCmdCopyBufferToImage(cmd,
                         buffer->get_vk_buffer(),
                         vk_image_,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1,
                         &buffer_copy_region);

  /* After the loop, all mip layers are in TRANSFER_SRC layout, so transition all to SHADER_READ */
  insert_image_memory_barrier(cmd,
                              vk_image_,
                              VK_ACCESS_TRANSFER_WRITE_BIT,
                              dst_access,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              dst_layout,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              dst_stage,
                              {(uint32_t)aspect_flag, (uint32_t)mip, 1, 0, 1});
  vk_image_layout = dst_layout;

  staging->end();
};

VKTexture::VKTexture(const char *name, VKContext *context) : Texture(name)
{

  context_ = context;

  mip_max_ = 1;
  mip_min_  = 0;
  current_view_id_ = -1;
  views_ = VK_NULL_HANDLE;
}

VKTexture::~VKTexture(void)
{
  VkDevice device = context_->device_get();

 if (views_ != VK_NULL_HANDLE) {
      vkDestroyImageView(device, views_, nullptr);
      views_ = VK_NULL_HANDLE;
 }
  
 

  if (vk_image_ != VK_NULL_HANDLE) {
    VmaAllocator mem_allocator = context_->mem_allocator_get();
    vmaDestroyImage(mem_allocator, vk_image_, vk_allocation_);
    vk_image_ = VK_NULL_HANDLE;
    /// vkDestroyImage(device, vk_image_, nullptr);
  }
}

static int VK_IMAGE_ALLOCATED_TIMES = 0;

bool VKTexture::init_internal(void)
{

  mip_min_ = 0;
  mip_max_ = mipmaps_;
  BLI_assert(views_ == VK_NULL_HANDLE);

  /*blender type check.*/
  target_type_ = to_vk_image_type(type_);
  target_view_type_ = to_vk_image_view_type(type_);

  /*image conf*/
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  {
    /*internal_format & gl_format & gl_type(OpenGL)*/
    info.format = to_vk(format_);
    info.imageType = to_vk_image_type(type_);
    info.extent.width = static_cast<uint32_t>(w_);
    if (h_ == 0) {
      h_ = 1;
    }
    info.extent.height = static_cast<uint32_t>(h_);
    if (d_ == 0) {
      d_ = 1;
    }
    info.extent.depth = static_cast<uint32_t>(d_);


    info.arrayLayers = this->layer_count();
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    /* TODO(fclem) improve this. */
    /* VK_IMAGE_TILING_LINEAR listrictions ==>
     * https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkImageCreateInfo.html#_description
     */
    /* Is it better to do it with VkBuffer? */
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.mipLevels = mipmaps_;
    /*KTX texture Compressed Texture sample
     * https://github.com/KhronosGroup/Vulkan-Samples/blob/master/samples/performance/texture_compression_comparison/texture_compression_comparison.cpp*/

    info.usage = to_vk_usage(gpu_image_usage_flags_);
    /*need a little more detailed flag.
    if (name_[0] == 'o' && name_[1] == 'f' && name_[2] == 's') {
      if (format_flag_ & GPU_FORMAT_DEPTH) {
        info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      }
      else
        info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    else {
      if (format_flag_ & GPU_FORMAT_DEPTH) {
        info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      }
      else {
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
      }
    }
    */

   
    info.flags = 0;

    if (type_ == GPU_TEXTURE_CUBE) {
      info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    if (target_view_type_ == VK_IMAGE_VIEW_TYPE_2D_ARRAY) {
      info.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
    }
  }

  if (!proxy_check(info)) {
    return false;
  };

  VmaAllocator mem_allocator = context_->mem_allocator_get();
  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  alloc_info.flags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;

  VmaAllocationInfo allocinfo = {};
  std::string pName = (name_ + std::to_string(VK_IMAGE_ALLOCATED_TIMES++)).c_str();


  vmaCreateImage(mem_allocator, &info, &alloc_info, &vk_image_, &vk_allocation_, &allocinfo);
  needs_update_descriptor_ = true;
  vmaSetAllocationName(mem_allocator, vk_allocation_, pName.c_str());
                                                     
  auto state_manager = reinterpret_cast<VKStateManager *>(context_->state_manager);
  state_manager->texture_bind_temp(this);


  desc_info_.imageView = VK_NULL_HANDLE;
  desc_info_.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  desc_info_.sampler = VK_NULL_HANDLE;
  vk_image_layout_.resize(mipmaps_);

  for (auto &layout : vk_image_layout_) {
    layout  = VK_IMAGE_LAYOUT_UNDEFINED;
  }

 
  debug::object_vk_label(VKContext::get()->device_get(), vk_image_, name_); 
  return true;
}

bool VKTexture::proxy_check(VkImageCreateInfo &info)
{
  /*TODO :: Skip if Check is unnecessary*/
  auto pdevice = context_->get_physical_device();

  VkImageFormatProperties ImageProps = {};

  VK_CHECK(vkGetPhysicalDeviceImageFormatProperties(
      pdevice, info.format, info.imageType, info.tiling, info.usage, info.flags, &ImageProps));

  if (type_ & GPU_TEXTURE_ARRAY) {
    if (info.arrayLayers > ImageProps.maxArrayLayers) {
      return false;
    }
  }

  if (info.mipLevels > ImageProps.maxMipLevels) {
    return false;
  }

  auto max_ext = ImageProps.maxExtent;
  if (type_ == GPU_TEXTURE_3D) {
    if (info.extent.width > max_ext.width || info.extent.height > max_ext.height ||
        info.extent.depth > max_ext.depth) {
      return false;
    }
  }
  else if ((type_ & ~GPU_TEXTURE_ARRAY) == GPU_TEXTURE_2D) {
    if (info.extent.width > max_ext.width || info.extent.height > max_ext.height) {
      return false;
    }
  }
  else if ((type_ & ~GPU_TEXTURE_ARRAY) == GPU_TEXTURE_1D) {
    if (info.extent.width > max_ext.width) {
      return false;
    }
  }
  else if ((type_ & ~GPU_TEXTURE_ARRAY) == GPU_TEXTURE_CUBE) {
    if (info.extent.width > max_ext.width || info.extent.height > max_ext.height) {
      return false;
    }
  }

  /*TODO :: All Hardware Check Required*/
  if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_WIN, GPU_DRIVER_OFFICIAL)) {
    return true;
  }

  /*TODO :: Check memory type, tiling, etc.*/
  return true;
};
/* Fetch the raw buffer data from a texture and copy to CPU host ptr. */
void VKTexture::read_internal(int mip,
                              int x_off,
                              int y_off,
                              int z_off,
                              int width,
                              int height,
                              int depth,
                              eGPUDataFormat desired_output_format,
                              int num_output_components,
                              int debug_data_size,
                              void *r_data)
{
  /* Verify textures are baked. */
  printf("TODO read attachment.\n");
  BLI_assert(false);
}

void VKTexture::update_sub_direct_state_access(int mip,
                                               int offset[3],
                                               int extent[3],
                                               const void *data)
{
  if (format_flag_ & GPU_FORMAT_COMPRESSED) {
    BLI_assert(false);
    /*NIL*/
#if 0
    size_t size = ((extent[0] + 3) / 4) * ((extent[1] + 3) / 4) * to_block_size(format_);
    switch (this->dimensions_count()) {
    default:
    case 1:
      glCompressedTextureSubImage1D(tex_id_, mip, offset[0], extent[0], format, size, data);
      break;
    case 2:
      glCompressedTextureSubImage2D(
        tex_id_, mip, UNPACK2(offset), UNPACK2(extent), format, size, data);
      break;
    case 3:
      glCompressedTextureSubImage3D(
        tex_id_, mip, UNPACK3(offset), UNPACK3(extent), format, size, data);
      break;
    }
#endif
  }
  else {

    TextureSubImage(mip, offset, extent, data);
  }

  has_pixels_ = true;
}
void VKTexture::update_sub(
    int mip, int offset[3], int extent[3], eGPUDataFormat type, const void *data)
{
  BLI_assert(validate_data_format(format_, type));
  BLI_assert(data != nullptr);
  if (mip >= mipmaps_) {
    fprintf(stderr, "Updating a miplvl on a texture too small to have this many levels.");
    return;
  }

  /* Some drivers have issues with cubemap & glTextureSubImage3D even if it is correct. */
  // if (GLContext::direct_state_access_support &&
  if (type_ != GPU_TEXTURE_CUBE) {
    this->update_sub_direct_state_access(mip, offset, extent, data);
    return;
  }

  /*NIL*/
  BLI_assert(false);
#if 0
  GLContext::state_manager_active_get()->texture_bind_temp(this);
  if (type_ == GPU_TEXTURE_CUBE) {
    for (int i = 0; i < extent[2]; i++) {
      GLenum target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + offset[2] + i;
      glTexSubImage2D(target, mip, UNPACK2(offset), UNPACK2(extent), gl_format, gl_type, data);
    }
  }
  else if (format_flag_ & GPU_FORMAT_COMPRESSED) {
    size_t size = ((extent[0] + 3) / 4) * ((extent[1] + 3) / 4) * to_block_size(format_);
    switch (dimensions) {
    default:
    case 1:
      glCompressedTexSubImage1D(target_, mip, offset[0], extent[0], gl_format, size, data);
      break;
    case 2:
      glCompressedTexSubImage2D(
        target_, mip, UNPACK2(offset), UNPACK2(extent), gl_format, size, data);
      break;
    case 3:
      glCompressedTexSubImage3D(
        target_, mip, UNPACK3(offset), UNPACK3(extent), gl_format, size, data);
      break;
    }
  }
  else {
    switch (dimensions) {
    default:
    case 1:
      glTexSubImage1D(target_, mip, offset[0], extent[0], gl_format, gl_type, data);
      break;
    case 2:
      glTexSubImage2D(target_, mip, UNPACK2(offset), UNPACK2(extent), gl_format, gl_type, data);
      break;
    case 3:
      glTexSubImage3D(target_, mip, UNPACK3(offset), UNPACK3(extent), gl_format, gl_type, data);
      break;
    }
  }

  has_pixels_ = true;
#endif
}

void VKTexture::swizzle_set(const char swizzle_mask[4])
{
  vk_swizzle_.r = swizzle_to_vk(swizzle_mask[0]);
  vk_swizzle_.g = swizzle_to_vk(swizzle_mask[1]);
  vk_swizzle_.b = swizzle_to_vk(swizzle_mask[2]);
  vk_swizzle_.a = swizzle_to_vk(swizzle_mask[3]);

  /* The swizzling changed, we need to reconstruct all views. */
  VkDevice device = context_->device_get();

    if (views_ != VK_NULL_HANDLE) {
      /* WARNING: This is potentially unsafe since the views might already be in used.
       * In practice, swizzle_set is always used just after initialization or before usage. */
      vkDestroyImageView(device, views_, nullptr);
      views_ = VK_NULL_HANDLE;
    }

}

void VKTexture::copy_to(Texture *dst_){


    int mip = 0;
    VKTexture *dst = static_cast<VKTexture *>(dst_);
    VKTexture *src = this;
  VKContext *context = VKContext::get();
  BLI_assert(context);


  auto srcFormat = src->info.format;
  auto dstFormat = dst->info.format;
  BLI_assert(srcFormat == dstFormat);
  bool supportsBlit = check_canBlit(srcFormat, dstFormat);


  VkImage srcImage = src->get_image();
  VkImage dstImage = dst->get_image();

  VkCommandBuffer copyCmd = VK_NULL_HANDLE;

  context->begin_submit_simple(copyCmd);

  ImgState src_state,dst_state;



  image_barrier_copy(copyCmd,src, src_state, true,mip);
  image_barrier_copy(copyCmd,dst, dst_state, false,mip);

  if (supportsBlit) {
    // Define the region to blit (we will blit the whole swapchain image)
    VkOffset3D blitSize;
    blitSize.x = src->info.extent.width;
    blitSize.y = src->info.extent.height;
    blitSize.z = 1;
    VkImageBlit imageBlitRegion{};
    imageBlitRegion.srcSubresource.aspectMask = src_state.iacs;
    imageBlitRegion.srcSubresource.layerCount = 1;
    imageBlitRegion.srcOffsets[1] = blitSize;
    imageBlitRegion.dstSubresource.aspectMask = dst_state.iacs;
    imageBlitRegion.dstSubresource.layerCount = 1;
    imageBlitRegion.dstOffsets[1] = blitSize;

    // Issue the blit command
    vkCmdBlitImage(copyCmd,
                   srcImage,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   dstImage,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &imageBlitRegion,
                   VK_FILTER_NEAREST);
  }
  else {
    // Otherwise use image copy (requires us to manually flip components)
    VkImageCopy imageCopyRegion{};
    imageCopyRegion.srcSubresource.aspectMask = src_state.iacs;
    imageCopyRegion.srcSubresource.layerCount = 1;
    imageCopyRegion.dstSubresource.aspectMask = dst_state.iacs;
    imageCopyRegion.dstSubresource.layerCount = 1;
    imageCopyRegion.extent.width = src->info.extent.width;
    imageCopyRegion.extent.height = src->info.extent.height;
    imageCopyRegion.extent.depth = 1;

    // Issue the copy command
    vkCmdCopyImage(copyCmd,
                   srcImage,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   dstImage,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &imageCopyRegion);
  }


  image_barrier2_copy(copyCmd, src_state, true, mip);
  image_barrier2_copy(copyCmd, dst_state, false, mip);

  context->end_submit_simple();


}



VkImageView VKTexture::create_image_view(int mip, int layer, int mipcount = 1, int levelcount = 1)
{
  BLI_assert(mip >= 0);
  BLI_assert(layer >= 0);
  VkDevice device = context_->device_get();
  VkImageView &view = views_;
  if (view != VK_NULL_HANDLE) {
    vkDestroyImageView(device, view, nullptr);
    view = VK_NULL_HANDLE;
  }

  VkImageSubresourceRange range;
  range.aspectMask = to_vk(format_flag_);
  range.baseMipLevel = mip;
  range.baseArrayLayer = layer;
  range.levelCount = mipcount;
  range.layerCount = levelcount;

  VkImageViewCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  info.flags = 0;
  info.image = vk_image_;
  info.viewType = to_vk_image_view_type(type_);
  info.format = to_vk(format_);
  info.components = {VK_COMPONENT_SWIZZLE_R,
                     VK_COMPONENT_SWIZZLE_G,
                     VK_COMPONENT_SWIZZLE_B,
                     VK_COMPONENT_SWIZZLE_A};

  info.subresourceRange = range;

  vkCreateImageView(device, &info, nullptr, &view);


  desc_info_.imageView = view;

  return view;
}

VkImageView VKTexture::vk_image_view_get(int mip)
{
  return this->vk_image_view_get(mip, 0);
}

VkImageView VKTexture::vk_image_view_get(int mip, int layer,bool force )
{
  int view_id = mipmaps_ * (layer_count() + 1) + layer + 1;
  VkDevice device = context_->device_get();
  VkImageView& view = views_;
  if (force) {
    if (view != VK_NULL_HANDLE) {
      vkDestroyImageView(device, view, nullptr);
      view = VK_NULL_HANDLE;
    };
  }
  else {
    if (current_view_id_ == view_id) {
      BLI_assert(view != VK_NULL_HANDLE);
      return view;
    }
  }

  needs_update_descriptor_ = true;
  if (view == VK_NULL_HANDLE) {
    views_ = view = this->create_image_view(0,layer, mipmaps_,1);
  }
  current_view_id_ = view_id;
  return view;
}

void VKTexture::check_feedback_loop()
{
  /* Recursive down sample workaround break this check.
   * See #recursive_downsample() for more information. */
  if (GPU_mip_render_workaround()) {
    return;
  }

#if 0
  /* Do not check if using compute shader. */
  VKShader *sh = dynamic_cast<VKShader *>(Context::get()->shader);
  if (sh && sh->is_compute()) {
    return;
  }
  VKFrameBuffer *fb = static_cast<VKFrameBuffer *>(context_->active_fb);
  for (int i = 0; i < ARRAY_SIZE(fb_); i++) {
    if (fb_[i] == fb) {
      GPUAttachmentType type = fb_attachment_[i];
      GPUAttachment attachment = fb->attachments_[type];
      if (attachment.mip <= mip_max_ && attachment.mip >= mip_min_) {
        char msg[256];
        SNPRINTF(msg,
                 "Feedback loop: Trying to bind a texture (%s) with mip range %d-%d but mip %d is "
                 "attached to the active framebuffer (%s)",
                 name_,
                 mip_min_,
                 mip_max_,
                 attachment.mip,
                 fb->name_);
        debug::raise_gl_error(msg);
      }
      return;
    }
  }
#endif
}
VKAttachment::VKAttachment(VKFrameBuffer *fb)
{
  BLI_assert(fb != nullptr);
  clear();
  fb_ = fb;
  vview_.resize(1);
  vtex_.clear();
  vtex_ds_.clear();
}
VKAttachment::~VKAttachment()
{
  clear();
};

void VKAttachment::bind(bool force )
{

  bool tran = force;

  int mip = 0;
  for (auto &tex : vtex_) {
    auto layout = tex->get_image_layout(0);
    if (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ||
        (layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)) {
      sampler2attachment(tex, mip, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      tran = true;
    };
  }

  for (auto &tex : vtex_ds_) {
    auto layout = tex->get_image_layout(0);
    if (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      sampler2attachment(tex, mip, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
      tran = true;
    };
  }

  if (tran) {
    fb_->update_attachments();
  }


};
void VKAttachment::clear()
{

  if (context_) {
    auto device = context_->device_get();

    if (renderpass_ != VK_NULL_HANDLE) {
      vkDestroyRenderPass(device, renderpass_, nullptr);
      renderpass_ = VK_NULL_HANDLE;
    };
    for (auto framebuffer : framebuffer_) {
      if (framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
        framebuffer = VK_NULL_HANDLE;
      };
    }
  }
  vdesc_.clear();
  vref_color_.clear();
  vref_depth_stencil_.clear();
  vview_.clear();
  vview_.resize(1);
  num_ = 0;
  extent_ = {0, 0, 0};
  renderpass_ = VK_NULL_HANDLE;
  framebuffer_.clear();
  vtex_.clear();
  vtex_ds_.clear();
};

/* naive implementation. Can subpath be used effectively? */
void VKAttachment::append(GPUAttachment &attach, VkImageLayout layout)
{
  int currentImageID = 0;

  VKTexture *tex = static_cast<VKTexture *>(unwrap(attach.tex));
  VkAttachmentDescription desc = {};
  desc.format = tex->info.format;
  desc.samples = VK_SAMPLE_COUNT_1_BIT;
  desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // VK_ATTACHMENT_LOAD_OP_CLEAR;
  desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  if (extent_.width == 0) {
    extent_ = tex->info.extent;
    mip_ = attach.mip;
    int size[3];
    GPU_texture_get_mipmap_size(attach.tex, attach.mip, size);
    fb_->size_set(size[0], size[1]);
  }

  BLI_assert(attach.mip == mip_ && extent_.width == tex->info.extent.width &&
             extent_.height == tex->info.extent.height && extent_.depth == tex->info.extent.depth);

  if (layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    vref_color_.append({num_, layout});
    fb_->set_srgb(GPU_texture_format(attach.tex) == GPU_SRGB8_A8);
    vtex_.append(tex);

    desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }
  else {
    vref_depth_stencil_.append({num_, layout});
    vtex_ds_.append(tex);
    desc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  }

  vdesc_.append(desc);
  BLI_assert(mip_ == 0);
  vview_[currentImageID].append(tex->create_image_view(mip_, 0, 1, tex->info.arrayLayers));

  num_++;
};

uint32_t VKAttachment::get_nums()
{
  return num_;
}
void VKAttachment::create_framebuffer()
{

  auto device = context_->device_get();

  if (renderpass_ == VK_NULL_HANDLE) {
    if (vref_color_.size() == 0) {
      return;
    }
    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = static_cast<uint32_t>(vref_color_.size());
    subpassDescription.pColorAttachments = vref_color_.data();
    subpassDescription.pDepthStencilAttachment = (vref_depth_stencil_.size() == 0) ?
                                                     nullptr :
                                                     vref_depth_stencil_.data();


    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;
    int depCnt = 1;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask      = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask     = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags   = VK_DEPENDENCY_BY_REGION_BIT;

    if (vref_depth_stencil_.size() > 0) {

      depCnt++;
      dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
      dependencies[1].dstSubpass = 0;
      dependencies[1].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      dependencies[1].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      dependencies[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
      dependencies[1].dependencyFlags = 0;

    }
    /*
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    */

    // Create the actual renderpass
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(vdesc_.size());
    renderPassInfo.pAttachments = vdesc_.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = depCnt;
    renderPassInfo.pDependencies = dependencies.data();
    VK_CHECK2(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderpass_));
    
    debug::object_vk_label(device, renderpass_ , std::string(fb_->name_get()) +"_Renderpass"); 
    BLI_assert(framebuffer_.size() == 0);

    framebuffer_.resize(vview_.size());
  };


  BLI_assert(framebuffer_.size() == vview_.size());


  int i = 0;
  for (auto & view_ : vview_) {

    VkFramebufferCreateInfo fb_create_info = {};
    fb_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_create_info.renderPass = renderpass_;
    fb_create_info.attachmentCount = static_cast<uint32_t> ( view_.size() );
    fb_create_info.pAttachments = view_.data();
    fb_create_info.width = extent_.width;
    fb_create_info.height = extent_.height;
    fb_create_info.layers = 1;
   
    VK_CHECK2(vkCreateFramebuffer(device, &fb_create_info, nullptr, &framebuffer_[i]));
    debug::object_vk_label(device,framebuffer_[i],  std::string(fb_->name_get()) +"_Framebuffer");
    i++;
  };

  return;
}

void VKAttachment::append_from_swapchain(int swapchain_idx)
{

  clear();

  fbo_id_ = swapchain_idx;

  auto device_ = context_->device_get();

  auto size = context_->getImageViewNums();
  

  framebuffer_.resize(size);
  vview_.resize(size);

  for (int i = 0; i < size; i++) {
    VkImageView view = VK_NULL_HANDLE;
    context_->getImageView(view, i);
    vview_[i].append(view);
  }

  VkExtent2D ext = {};
  context_->getRenderExtent(ext);

  extent_.width = ext.width;
  extent_.height = ext.height;
  extent_.depth = 1;

  VkAttachmentDescription colorAttachment = {};
  colorAttachment.format = context_->getImageFormat();
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // VK_ATTACHMENT_LOAD_OP_CLEAR;  // 
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;  // VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  vdesc_.append(colorAttachment);

  VkAttachmentReference colorAttachmentRef = {};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDependency subpassDependencies[2] = {};

  // Transition from final to initial (VK_SUBPASS_EXTERNAL refers to all commmands executed
  // outside of the actual renderpass)
  subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  subpassDependencies[0].dstSubpass = 0;
  subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  subpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  // Transition from initial to final
  subpassDependencies[1].srcSubpass = 0;
  subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  subpassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkSubpassDescription subpassDescription = {};

  subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpassDescription.flags = 0;
  subpassDescription.inputAttachmentCount = 0;
  subpassDescription.pInputAttachments = NULL;
  subpassDescription.colorAttachmentCount = 1;
  subpassDescription.pColorAttachments = &colorAttachmentRef;
  subpassDescription.pResolveAttachments = NULL;
  subpassDescription.pDepthStencilAttachment = NULL;
  subpassDescription.preserveAttachmentCount = 0;
  subpassDescription.pPreserveAttachments = NULL;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpassDescription;
  renderPassInfo.dependencyCount = 2;
  renderPassInfo.pDependencies = subpassDependencies;
  VK_CHECK2(vkCreateRenderPass(device_, &renderPassInfo, NULL, &renderpass_));
  debug::object_vk_label(device_, renderpass_ , std::string(fb_->name_get()) +"_Renderpass"); 
  num_++;
};

}  // namespace blender::gpu
