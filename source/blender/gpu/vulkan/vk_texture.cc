/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "vk_texture.hh"

#include "vk_buffer.hh"
#include "vk_context.hh"
#include "vk_data_conversion.hh"
#include "vk_memory.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"

#include "BLI_math_vector.hh"

#include "DNA_userdef_types.h"

#include "BKE_global.h"

namespace blender::gpu {

VKTexture::~VKTexture()
{
  VK_ALLOCATION_CALLBACKS

  vmaDestroyImage(VKBackend::get().mem_allocator_get(), vk_image_, allocation_);
  vkDestroyImageView(VKBackend::get().mem_device_get(), vk_image_view_, vk_allocation_callbacks);
}
/* Samplers cache VulkanObjects directly, not information. */
VkSampler VKTexture::samplers_cache_[GPU_SAMPLER_EXTEND_MODES_COUNT]
                                    [GPU_SAMPLER_EXTEND_MODES_COUNT]
                                    [GPU_SAMPLER_FILTERING_TYPES_COUNT] = {};
VkSampler VKTexture::custom_samplers_cache_[GPU_SAMPLER_CUSTOM_TYPES_COUNT] = {};

static inline VkSamplerAddressMode to_vk(GPUSamplerExtendMode extend_mode)
{
  switch (extend_mode) {
    case GPU_SAMPLER_EXTEND_MODE_EXTEND:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case GPU_SAMPLER_EXTEND_MODE_REPEAT:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case GPU_SAMPLER_EXTEND_MODE_MIRRORED_REPEAT:
      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    default:
      BLI_assert_unreachable();
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  }
}

void VKTexture::samplers_init(VKContext *context)
{

  VkDevice vk_device = context->device_get();

  VkSamplerCreateInfo samplerCI = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerCI.pNext = VK_NULL_HANDLE;
  samplerCI.flags = 0;
  const float max_anisotropy = 16.f;
  const float aniso_filter = max_ff(max_anisotropy, U.anisotropic_filter);

  for (int extend_yz_i = 0; extend_yz_i < GPU_SAMPLER_EXTEND_MODES_COUNT; extend_yz_i++) {
    const GPUSamplerExtendMode extend_yz = static_cast<GPUSamplerExtendMode>(extend_yz_i);
    const VkSamplerAddressMode extend_t = to_vk(extend_yz);

    for (int extend_x_i = 0; extend_x_i < GPU_SAMPLER_EXTEND_MODES_COUNT; extend_x_i++) {
      const GPUSamplerExtendMode extend_x = static_cast<GPUSamplerExtendMode>(extend_x_i);
      const VkSamplerAddressMode extend_s = to_vk(extend_x);

      for (int filtering_i = 0; filtering_i < GPU_SAMPLER_FILTERING_TYPES_COUNT; filtering_i++) {
        const GPUSamplerFiltering filtering = GPUSamplerFiltering(filtering_i);
        samplerCI.minFilter = samplerCI.magFilter = (filtering & GPU_SAMPLER_FILTERING_LINEAR) ?
                                                        VK_FILTER_LINEAR :
                                                        VK_FILTER_NEAREST;
        samplerCI.mipmapMode = (filtering & GPU_SAMPLER_FILTERING_LINEAR) ?
                                   VK_SAMPLER_MIPMAP_MODE_LINEAR :
                                   VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerCI.maxAnisotropy = (filtering & GPU_SAMPLER_FILTERING_MIPMAP) ? aniso_filter :
                                                                               max_anisotropy;
        samplerCI.compareEnable = VK_FALSE;
        samplerCI.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerCI.addressModeU = extend_s;
        samplerCI.addressModeV = extend_t;
        samplerCI.addressModeW = extend_t;
        samplerCI.mipLodBias = 0.0f;
        samplerCI.minLod = -1000;
        samplerCI.maxLod = 1000;
        samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        samplerCI.unnormalizedCoordinates = VK_FALSE;
        VkSampler &sampler = samplers_cache_[extend_yz_i][extend_x_i][filtering_i];
        vkCreateSampler(vk_device, &samplerCI, nullptr, &sampler);
        const GPUSamplerState sampler_state = {filtering, extend_x, extend_yz};
        const std::string sampler_name = sampler_state.to_string();
        debug::object_label(context, sampler, sampler_name.c_str());
      }
    }
  }

  /* Compare sampler for depth textures. */
  VkSampler &compare_sampler = custom_samplers_cache_[GPU_SAMPLER_CUSTOM_COMPARE];
  samplerCI.minFilter = samplerCI.magFilter = VK_FILTER_LINEAR;
  samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerCI.maxAnisotropy = 1.f;
  samplerCI.compareEnable = VK_TRUE;
  samplerCI.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerCI.mipLodBias = 0.0f;
  samplerCI.minLod = -1000;
  samplerCI.maxLod = 1000;
  samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
  samplerCI.unnormalizedCoordinates = VK_FALSE;
  vkCreateSampler(vk_device, &samplerCI, nullptr, &compare_sampler);
  // glSamplerParameteri(compare_sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
  debug::object_label(context, compare_sampler, "compare");

  /* Custom sampler for icons. The icon texture is sampled within the shader using a -0.5f LOD
   * bias. */
  VkSampler &icon_sampler = custom_samplers_cache_[GPU_SAMPLER_CUSTOM_ICON];
  samplerCI.mipLodBias = -0.5f;
  samplerCI.minFilter = VK_FILTER_LINEAR;
  samplerCI.magFilter = VK_FILTER_LINEAR;
  samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  vkCreateSampler(vk_device, &samplerCI, nullptr, &icon_sampler);
  debug::object_label(context, icon_sampler, "icons");
}
/** Reconstruction cannot be partially done with update. If device is destroyed, it needs
 *to be rebuilt.
 **/
void VKTexture::samplers_update()
{
  samplers_free();
  samplers_init(VKContext::get());
}

void VKTexture::samplers_free()
{
  VkDevice device = VKContext::get()->device_get();
  VkSampler &icon_sampler = custom_samplers_cache_[GPU_SAMPLER_CUSTOM_ICON];
  VkSampler &compare_sampler = custom_samplers_cache_[GPU_SAMPLER_CUSTOM_COMPARE];
  vkDestroySampler(device, icon_sampler, nullptr);
  icon_sampler = VK_NULL_HANDLE;
  vkDestroySampler(device, compare_sampler, nullptr);
  compare_sampler = VK_NULL_HANDLE;

  for (int extend_yz_i = 0; extend_yz_i < GPU_SAMPLER_EXTEND_MODES_COUNT; extend_yz_i++) {
    for (int extend_x_i = 0; extend_x_i < GPU_SAMPLER_EXTEND_MODES_COUNT; extend_x_i++) {
      for (int filtering_i = 0; filtering_i < GPU_SAMPLER_FILTERING_TYPES_COUNT; filtering_i++) {
        VkSampler &sampler = samplers_cache_[extend_yz_i][extend_x_i][filtering_i];
        vkDestroySampler(device, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
      }
    }
  }
}
VkSampler VKTexture::get_sampler(const GPUSamplerState &sampler_state)
{
  /* Internal sampler states are signal values and do not correspond to actual samplers. */
  BLI_assert(sampler_state.type != GPU_SAMPLER_STATE_TYPE_INTERNAL);

  if (sampler_state.type == GPU_SAMPLER_STATE_TYPE_CUSTOM) {
    return custom_samplers_cache_[sampler_state.custom_type];
  }

  return samplers_cache_[sampler_state.extend_yz][sampler_state.extend_x][sampler_state.filtering];
}

void VKTexture::generate_mipmap() {}

void VKTexture::copy_to(Texture * /*tex*/) {}

void VKTexture::clear(eGPUDataFormat format, const void *data)
{
  if (!is_allocated()) {
    allocate();
  }
  int c_mip = current_mip_;
  current_mip_ = -1;
  VKContext &context = *VKContext::get();
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  VkClearColorValue clear_color = to_vk_clear_color_value(format, data);
  VkImageSubresourceRange range = {0};
  range.aspectMask = to_vk_image_aspect_flag_bits(format_);
  range.levelCount = VK_REMAINING_MIP_LEVELS;
  range.layerCount = VK_REMAINING_ARRAY_LAYERS;
  layout_ensure(context, VkTransitionState::VK_ENSURE_COPY_DST);

  command_buffer.clear(
      vk_image_, current_layout_get(), clear_color, Span<VkImageSubresourceRange>(&range, 1));
  current_mip_ = c_mip;
}

void VKTexture::swizzle_set(const char /*swizzle_mask*/[4]) {}

void VKTexture::stencil_texture_mode_set(bool /*use_stencil*/) {}

void VKTexture::mip_range_set(int /*min*/, int /*max*/) {}

void *VKTexture::read(int mip, eGPUDataFormat format)
{
  int c_mip = current_mip_;
  current_mip_ = mip;
  VKContext &context = *VKContext::get();
  layout_ensure(context, VkTransitionState::VK_ENSURE_COPY_SRC);

  /* Vulkan images cannot be directly mapped to host memory and requires a staging buffer. */
  VKBuffer staging_buffer;

  /* NOTE: mip_size_get() won't override any dimension that is equal to 0. */
  int extent[3] = {1, 1, 1};
  mip_size_get(mip, extent);
  size_t sample_len = extent[0] * extent[1] * extent[2];
  size_t device_memory_size = sample_len * to_bytesize(format_);
  size_t host_memory_size = sample_len * to_bytesize(format_, format);

  staging_buffer.create(
      context, device_memory_size, GPU_USAGE_DEVICE_ONLY, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  VkBufferImageCopy region = {};
  region.imageExtent.width = extent[0];
  region.imageExtent.height = extent[1];
  region.imageExtent.depth = extent[2];
  region.imageSubresource.aspectMask = to_vk_image_aspect_flag_bits(format_);
  region.imageSubresource.mipLevel = mip;
  region.imageSubresource.layerCount = 1;

  VKCommandBuffer &command_buffer = context.command_buffer_get();
  command_buffer.copy(staging_buffer, *this, Span<VkBufferImageCopy>(&region, 1));
  command_buffer.submit();

  current_mip_ = c_mip;
  void *data = MEM_mallocN(host_memory_size, __func__);
  convert_device_to_host(data, staging_buffer.mapped_memory_get(), sample_len, format, format_);
  return data;
}

void VKTexture::update_sub(
    int mip, int offset[3], int extent[3], eGPUDataFormat format, const void *data)
{
  if (!is_allocated()) {
    allocate();
  }
  extent[2] = (extent[2] == 0) ? 1 : extent[2];
  /* Vulkan images cannot be directly mapped to host memory and requires a staging buffer. */
  VKContext &context = *VKContext::get();
  VKBuffer staging_buffer;
  size_t sample_len = extent[0] * extent[1] * extent[2];
  size_t device_memory_size = sample_len * to_bytesize(format_);

  staging_buffer.create(context,
                        device_memory_size,
                        GPU_USAGE_DYNAMIC,
                        (VkBufferUsageFlagBits)(VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                VK_BUFFER_USAGE_TRANSFER_DST_BIT));
  convert_host_to_device(staging_buffer.mapped_memory_get(), data, sample_len, format, format_);

  VkBufferImageCopy region = {};
  region.imageExtent.width = extent[0];
  region.imageExtent.height = extent[1];
  region.imageExtent.depth = extent[2];
  region.imageOffset.x = offset[0];
  region.imageOffset.y = offset[1];
  region.imageOffset.z = offset[2];
  region.imageSubresource.aspectMask = to_vk_image_aspect_flag_bits(format_);
  region.imageSubresource.mipLevel = mip;
  region.imageSubresource.layerCount = 1;
  current_mip_ = mip;

  layout_ensure(context, VkTransitionState::VK_ENSURE_COPY_DST);
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  command_buffer.copy(*this, staging_buffer, Span<VkBufferImageCopy>(&region, 1));
  command_buffer.submit();

  layout_ensure(context, VkTransitionState::VK_ENSURE_TEXTURE);
}

void VKTexture::update_sub(int /*offset*/[3],
                           int /*extent*/[3],
                           eGPUDataFormat /*format*/,
                           GPUPixelBuffer * /*pixbuf*/)
{
}

/* TODO(fclem): Legacy. Should be removed at some point. */
uint VKTexture::gl_bindcode_get() const
{
  return 0;
}

bool VKTexture::init_internal()
{
  /* Initialization can only happen after the usage is known. By the current API this isn't
   * set at this moment, so we cannot initialize here. The initialization is postponed until
   * the allocation of the texture on the device. */

  /* TODO: return false when texture format isn't supported. */
  return true;
}

bool VKTexture::init_internal(GPUVertBuf * /*vbo*/)
{
  return false;
}

bool VKTexture::init_internal(const GPUTexture * /*src*/, int /*mip_offset*/, int /*layer_offset*/)
{
  return false;
}

void VKTexture::ensure_allocated()
{
  if (!is_allocated()) {
    allocate();
  }
}

bool VKTexture::is_allocated() const
{
  return vk_image_ != VK_NULL_HANDLE && allocation_ != VK_NULL_HANDLE;
}

static VkImageUsageFlagBits to_vk_image_usage(const eGPUTextureUsage usage,
                                              const eGPUTextureFormatFlag format_flag)
{
  VkImageUsageFlagBits result = static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_TRANSFER_DST_BIT);

  if (usage & GPU_TEXTURE_USAGE_SHADER_READ) {
    result = static_cast<VkImageUsageFlagBits>(result | VK_IMAGE_USAGE_SAMPLED_BIT);
  }
  if (usage & GPU_TEXTURE_USAGE_SHADER_WRITE) {
    result = static_cast<VkImageUsageFlagBits>(result | VK_IMAGE_USAGE_STORAGE_BIT |
                                               VK_IMAGE_USAGE_SAMPLED_BIT);
  }
  if (usage & GPU_TEXTURE_USAGE_ATTACHMENT) {
    if (format_flag & (GPU_FORMAT_NORMALIZED_INTEGER | GPU_FORMAT_COMPRESSED)) {
      /* These formats aren't supported as an attachment. When using
       * GPU_TEXTURE_USAGE_DEFAULT they are still being evaluated to be attachable. So we
       * need to skip them.*/
      result = static_cast<VkImageUsageFlagBits>(result | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    }
    else {
      if (format_flag & (GPU_FORMAT_DEPTH | GPU_FORMAT_STENCIL)) {
        result = static_cast<VkImageUsageFlagBits>(result |
                                                   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
      }
      else {
        result = static_cast<VkImageUsageFlagBits>(result | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
      }
    }
  }
  if (usage & GPU_TEXTURE_USAGE_HOST_READ) {
    result = static_cast<VkImageUsageFlagBits>(result | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
  }

  return result;
}

bool VKTexture::allocate()
{
  BLI_assert(!is_allocated());

  int extent[3] = {1, 1, 1};
  mip_size_get(0, extent);

  VKContext &context = *VKContext::get();
  VkImageCreateInfo image_info = {};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = to_vk_image_type(type_);
  image_info.extent.width = extent[0];
  image_info.extent.height = extent[1];
  image_info.extent.depth = extent[2];
  image_info.mipLevels = mipmaps_;
  image_info.arrayLayers = 1;
  image_info.format = to_vk_format(format_);

  current_layout_.resize(mipmaps_);
  for (auto &layout : current_layout_) {
    layout = VK_IMAGE_LAYOUT_UNDEFINED;
  };
  current_mip_ = 0;

  /* Some platforms (NVIDIA) requires that attached textures are always tiled optimal.
   *
   * As image data are always accessed via an staging buffer we can enable optimal tiling for
   * all texture. Tilings based on actual usages should be done in `VKFramebuffer`.
   */
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage = to_vk_image_usage(gpu_image_usage_flags_, format_flag_);
  image_info.usage = to_vk_image_usage(gpu_image_usage_flags_, format_flag_);
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;

  VkResult result;
  if (G.debug & G_DEBUG_GPU) {
    VkImageFormatProperties image_format = {};
    result = vkGetPhysicalDeviceImageFormatProperties(context.physical_device_get(),
                                                      image_info.format,
                                                      image_info.imageType,
                                                      image_info.tiling,
                                                      image_info.usage,
                                                      image_info.flags,
                                                      &image_format);
    if (result != VK_SUCCESS) {
      printf("Image type not supported on device.\n");
      return false;
    }
  }

  VmaAllocationCreateInfo allocCreateInfo = {};
  allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocCreateInfo.priority = 1.0f;
  result = vmaCreateImage(VKBackend::get().mem_allocator_get(),
                          &image_info,
                          &allocCreateInfo,
                          &vk_image_,
                          &allocation_,
                          nullptr);
  if (result != VK_SUCCESS) {
    return false;
  }

  /* Promote image to the correct layout. */
  // layout_ensure(context,VkTransitionState::VK_ENSURE_TEXTURE);

  VK_ALLOCATION_CALLBACKS
  VkImageViewCreateInfo image_view_info = {};
  image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_info.image = vk_image_;
  image_view_info.viewType = to_vk_image_view_type(type_);
  image_view_info.format = to_vk_format(format_);
  image_view_info.components = to_vk_component_mapping(format_);
  image_view_info.subresourceRange.aspectMask = to_vk_image_aspect_flag_bits(format_);
  image_view_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  image_view_info.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

  result = vkCreateImageView(
      context.device_get(), &image_view_info, vk_allocation_callbacks, &vk_image_view_);
  return result == VK_SUCCESS;
}

void VKTexture::image_bind(int binding)
{
  if (!is_allocated()) {
    allocate();
  }
  VKContext &context = *VKContext::get();
  VKShader *shader = static_cast<VKShader *>(context.shader);
  const VKShaderInterface &shader_interface = shader->interface_get();
  const VKDescriptorSet::Location location = shader_interface.descriptor_set_location(binding);

  shader->pipeline_get().descriptor_set_get().image_bind(*this, location);
}

void VKTexture::texture_bind(int binding, const GPUSamplerState &sampler_type)
{
#if 0
  if (!is_allocated()) {
    allocate();
  }
  VKContext &context = *VKContext::get();
  VKShader *shader = static_cast<VKShader *>(context.shader);
  const VKShaderInterface &shader_interface = shader->interface_get();
  const VKDescriptorSet::Location location = shader_interface.descriptor_set_location(binding);

  current_mip_ = -1;
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  command_buffer.image_transition(this, VkTransitionState::VK_ENSURE_TEXTURE, false, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,mipmaps_);

  shader->pipeline_get().descriptor_set_get().texture_bind(*this, location, sampler_type);
#endif
}

/* -------------------------------------------------------------------- */
/** \name Image Layout
 * \{ */

VkImageLayout VKTexture::current_layout_get() const
{
  if (current_mip_ == -1) {
    auto layout0 = current_layout_[0];
    for (int i = 1; i < mipmaps_; i++) {
      BLI_assert(layout0 == current_layout_[i]);
    }
    return layout0;
  }
  return current_layout_[current_mip_];
}

void VKTexture::current_layout_set(const VkImageLayout new_layout)
{
  if (current_mip_ == -1) {
    for (int i = 0; i < mipmaps_; i++) {
      current_layout_[i] = new_layout;
    }
    return;
  }
  current_layout_[current_mip_] = new_layout;
}

void VKTexture::layout_ensure(VKContext &context, const VkTransitionState requested_state)
{
  context.command_buffer_get().image_transition(this, requested_state, true);

#if 0
  const VkImageLayout current_layout = current_layout_get();
  if (current_layout == requested_layout) {
    return;
  }
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = current_layout;
  barrier.newLayout = requested_layout;
  barrier.image = vk_image_;
  barrier.subresourceRange.aspectMask = to_vk_image_aspect_flag_bits(format_);
  barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
  context.command_buffer_get().pipeline_barrier(Span<VkImageMemoryBarrier>(&barrier, 1));
  current_layout_set(requested_layout);
#endif
}
/** \} */

}  // namespace blender::gpu
