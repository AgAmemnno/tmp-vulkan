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

#include "vk_texture.hh"



#include "intern/GHOST_ContextVK.h"

namespace blender::gpu {
  /// <summary>
  /// @SaschaWillems , Vulkan Samples-1
  /// https://github.com/Mandar-Shinde/Vulkan-Samples-1/blob/master/samples/api/texture_mipmap_generation/texture_mipmap_generation_tutorial.md
  /// </summary>
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

  void VKTexture::generate_mipmaps(const void *data)
{

    BLI_assert(vk_image_);
   BLI_assert(this->mipmaps_ > 1);




   VkFormatProperties formatProperties;
    auto pdevice = VKContext::get()->get_physical_device();
   vkGetPhysicalDeviceFormatProperties(pdevice, to_vk(format_), &formatProperties);
    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) ||
        !(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
      BLI_assert_msg(false,
          "Selected image format does not support blit source and destination");
    };
    /// <summary>
    /// Assume R8G8B8A8
    /// </summary>
    VkDeviceSize size = 4 * w_ * h_ ;

    VKStagingBufferManager *staging = VKContext::get()->buffer_manager_;

    VKBuffer* buffer = staging->Create(size,256);

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


     insert_image_memory_barrier(cmd,
                                 vk_image_,
                                 VK_ACCESS_TRANSFER_WRITE_BIT,
                                 VK_ACCESS_TRANSFER_READ_BIT,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

     staging->end();



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
       image_blit.dstOffsets[1].x = int32_t(w_ >> (i ));
       image_blit.dstOffsets[1].y = int32_t(h_ >> (i ));
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
     vk_image_layout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    staging->end();


     mip_max_ = mipmaps_;
}


VKTexture::VKTexture(const char *name,VKContext* context) : Texture(name)
{
  context_ = context;
  //VKContext::get();
  mip_max_ = 0;
  mip_min_ = 0;
  current_view_id_ = -1;
}

VKTexture::~VKTexture(void)
{
  VkDevice device = context_->device_get();
  for (VkImageView view : views_) {
    if (view != VK_NULL_HANDLE) {
      vkDestroyImageView(device, view, nullptr);
    }
  }
  if (vk_image_ != VK_NULL_HANDLE) {
    VmaAllocator mem_allocator = context_->mem_allocator_get();
    vmaDestroyImage(mem_allocator, vk_image_, vk_allocation_);
    vk_image_ = VK_NULL_HANDLE;
    ///vkDestroyImage(device, vk_image_, nullptr);
  }
}

static int VK_IMAGE_ALLOCATED_TIMES = 0;

bool VKTexture::init_internal(void)
{
  VmaAllocator mem_allocator = context_->mem_allocator_get();
  {
    VkImageCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    /// <summary>
    /// TODO storage image
    /// </summary>
    info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                    VK_IMAGE_USAGE_SAMPLED_BIT;
     /// <summary>
    /// TODO attachment
    /// </summary>
    ///usage |= (format_flag_ & GPU_FORMAT_DEPTH) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

 
    info.imageType = to_vk_image_type(type_);
    info.extent.width = static_cast<uint32_t>(w_);
    info.extent.height = static_cast<uint32_t>(h_);
    info.extent.depth =  1;

    if (d_ > 0) {
      info.extent.depth = static_cast<uint32_t>(d_);
    };
    

    this->mipmaps_ = info.mipLevels = static_cast<uint32_t>(floor(log2(__max(w_, h_))) + 1);
    info.arrayLayers = this->layer_count();
    info.format = to_vk(format_);
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    /* TODO(fclem) improve this. */
    info.tiling = VK_IMAGE_TILING_OPTIMAL;///VK_IMAGE_TILING_LINEAR;
    vk_image_layout_ =  info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    info.flags = 0;
    
    views_.resize(info.mipLevels * (info.arrayLayers + 1), VK_NULL_HANDLE);



        VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    alloc_info.flags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;

#define CHECK_VKIMAGE_MEMORY_TYPE
    #ifdef CHECK_VKIMAGE_MEMORY_TYPE
        info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        auto pdevice  =         VKContext::get()->get_physical_device();
        VkImageFormatProperties ImageProps = {};
        VK_CHECK(vkGetPhysicalDeviceImageFormatProperties(pdevice,
                                                          info.format,
                                                          info.imageType,
                                                          info.tiling,
                                                         info.usage,
                                                          info.flags,
                                                              &ImageProps));


        printf(
            " Image Props   maxLayers  %u    maxExtentW %u  maxMipLevels %u  maxResources %llu  "
            "sampleCount %u",
            ImageProps.maxArrayLayers,
            ImageProps.maxExtent.width,
            ImageProps.maxMipLevels,
            ImageProps.maxResourceSize,
            ImageProps.sampleCounts);

    VK_CHECK(vkCreateImage(VK_DEVICE, &info, nullptr, &vk_image_));

     VkMemoryRequirements mreq = {};
    vkGetImageMemoryRequirements(VK_DEVICE, vk_image_, &mreq);
     while (true) {
      int valid = getMemoryType(mreq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, nullptr);
      printf("VKIMAGE   try memory type  DEVICELOCAL ==  %d \n ", valid);
      if (valid >= 0) {
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        alloc_info.flags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
        break;
      }
      valid = getMemoryType(mreq.memoryTypeBits,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 nullptr);

       printf("VKIMAGE   try memory type  HOSTVISIBLE ==  %d \n ", valid);
       if (valid >= 0) {
         alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
         alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                            VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
         break;
       }

       BLI_assert_msg(false, "Not found a suitable memorytype.  ");
     };

     vkDestroyImage(VK_DEVICE, vk_image_, nullptr);
    #endif

    VmaAllocationInfo allocinfo = {};
    allocinfo.pName =  ("VKImage_ " + std::to_string(VK_IMAGE_ALLOCATED_TIMES++)).c_str();

    vmaCreateImage(mem_allocator, &info, &alloc_info, &vk_image_, &vk_allocation_, &allocinfo);

                            
  }
  needs_update_descriptor_ = true;
  return true;
}



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

void VKTexture::update_sub(
    int mip, int offset[3], int extent[3], eGPUDataFormat type, const void *data)
{
  if (mip== 0) generate_mipmaps(data);
}

void VKTexture::swizzle_set(const char swizzle_mask[4])
{
  vk_swizzle_.r = swizzle_to_vk(swizzle_mask[0]);
  vk_swizzle_.g = swizzle_to_vk(swizzle_mask[1]);
  vk_swizzle_.b = swizzle_to_vk(swizzle_mask[2]);
  vk_swizzle_.a = swizzle_to_vk(swizzle_mask[3]);

  /* The swizzling changed, we need to reconstruct all views. */
  VkDevice device = context_->device_get();
  for (VkImageView view : views_) {
    if (view != VK_NULL_HANDLE) {
      /* WARNING: This is potentially unsafe since the views might already be in used.
       * In practice, swizzle_set is always used just after initialization or before usage. */
      vkDestroyImageView(device, view, nullptr);
      view = VK_NULL_HANDLE;
    }
  }
}

VkImageView VKTexture::create_image_view(int mip, int layer)
{
  VkDevice device = context_->device_get();

  VkImageSubresourceRange range;
  range.aspectMask = to_vk(format_flag_);
  range.baseMipLevel = (mip > -1) ? mip : 0;
  range.baseArrayLayer = (layer > -1) ? layer : 0;
  range.levelCount = 1;///(mip > -1) ? 1 : VK_REMAINING_MIP_LEVELS;
  range.layerCount = 1;///(layer > -1) ? 1 : VK_REMAINING_ARRAY_LAYERS;

  VkImageViewCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  info.flags = 0;
  info.image = vk_image_;
  info.viewType = to_vk_image_view_type(type_);
  info.format = to_vk(format_);
  info.components = {VK_COMPONENT_SWIZZLE_R,
                     VK_COMPONENT_SWIZZLE_G,
                     VK_COMPONENT_SWIZZLE_B,
                     VK_COMPONENT_SWIZZLE_A};
  ///vk_swizzle_;
  info.subresourceRange = range;

  VkImageView view;
  vkCreateImageView(device, &info, nullptr, &view);
  
  return view;
}

VkImageView VKTexture::vk_image_view_get(int mip)
{
  return this->vk_image_view_get(mip, -1);
}

VkImageView VKTexture::vk_image_view_get(int mip, int layer)
{
  int view_id = mip * (layer_count() + 1) + layer + 1;
  VkImageView view = views_[view_id];
  if (current_view_id_ == view_id) {
    BLI_assert(view != VK_NULL_HANDLE);
    return view;
  }

  needs_update_descriptor_ = true;
  if (view == VK_NULL_HANDLE) {
    views_[view_id] = view = this->create_image_view(mip, layer);
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
  VKFrameBuffer *fb = static_cast<VKFrameBuffer *>(VKContext::get()->active_fb);
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


}  // namespace blender::gpu
