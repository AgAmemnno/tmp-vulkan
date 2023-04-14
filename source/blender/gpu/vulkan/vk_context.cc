/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "DNA_userdef_types.h"

#include "vk_context.hh"
#include "vk_debug.hh"

#include "vk_backend.hh"
#include "vk_framebuffer.hh"
#include "vk_immediate.hh"
#include "vk_memory.hh"
#include "vk_shader.hh"
#include "vk_state_manager.hh"

#include "GHOST_C-api.h"

#include "intern/GHOST_ContextVk.h"

namespace blender::gpu {

VKContext::VKContext(void *ghost_window, void *ghost_context)
{
  VK_ALLOCATION_CALLBACKS;
  ghost_window_ = ghost_window;
  if (ghost_window) {
    ghost_context = GHOST_GetDrawingContext((GHOST_WindowHandle)ghost_window);
  }
  ghost_context_ = ghost_context;

  GHOST_GetVulkanHandles((GHOST_ContextHandle)ghost_context,
                         &vk_instance_,
                         &vk_physical_device_,
                         &vk_device_,
                         &vk_queue_family_,
                         &vk_queue_);

  debug::init_callbacks(this,vkGetInstanceProcAddr);

  ((GHOST_ContextVK *)(ghost_context_))->initializeDevice(vk_device_, vk_queue_, vk_queue_family_);

  init_physical_device_limits();
  /*Issue Memory Leak */
  #if 0
  /* Initialize the memory allocator. */
  VmaAllocatorCreateInfo info = {};
  /* Should use same vulkan version as GHOST (1.2), but set to 1.0 as 1.2 requires
   * correct extensions and functions to be found by VMA, which isn't working as expected and
   * requires more research. To continue development we lower the API to version 1.0. */
  info.vulkanApiVersion = VK_API_VERSION_1_0;
  info.physicalDevice = vk_physical_device_;
  info.device = vk_device_;
  info.instance = vk_instance_;
  info.pAllocationCallbacks = vk_allocation_callbacks;
  vmaCreateAllocator(&info, &mem_allocator_);
  #endif
  descriptor_pools_.init(vk_device_);

  VKBackend::capabilities_init(*this);

  state_manager = new VKStateManager(this);
  imm = new VKImmediate(this);

  /* For off-screen contexts. Default frame-buffer is empty. */
  back_left = new VKFrameBuffer("back_left");
  active_fb = back_left;
  vk_swap_chain_images_.resize(vk_im_prop.nums);

    /* Initialize samplers. */
  for (uint i = 0; i < GPU_SAMPLER_MAX; i++) {
    VKSamplerState state;
    state.state = static_cast<eGPUSamplerState>(i);
    sampler_state_cache_[i] = this->generate_sampler_from_state(state);
  }

  vk_in_frame_ = false;
}

VkSampler VKContext::generate_sampler_from_state(VKSamplerState sampler_state) {

  /* Check if sampler already exists for given state. */
  VkSamplerCreateInfo samplerCI = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

  VkSamplerAddressMode clamp_type =
      (sampler_state.state & GPU_SAMPLER_CLAMP_BORDER)
          ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
          : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  VkSamplerAddressMode repeat_type =
      (sampler_state.state & GPU_SAMPLER_MIRROR_REPEAT)
          ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
          : VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerCI.addressModeU =
      (sampler_state.state & GPU_SAMPLER_REPEAT_R) ? repeat_type : clamp_type;
  samplerCI.addressModeV =
      (sampler_state.state & GPU_SAMPLER_REPEAT_S) ? repeat_type : clamp_type;
  samplerCI.addressModeW =
      (sampler_state.state & GPU_SAMPLER_REPEAT_T) ? repeat_type : clamp_type;
  samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK; // VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
                                                                   // //

  samplerCI.unnormalizedCoordinates =
      VK_FALSE; /// descriptor.normalizedCoordinates = true;

  samplerCI.minFilter = (sampler_state.state & GPU_SAMPLER_FILTER)
                            ? VK_FILTER_LINEAR
                            : VK_FILTER_NEAREST;

  samplerCI.magFilter = (sampler_state.state & GPU_SAMPLER_FILTER)
                            ? VK_FILTER_LINEAR
                            : VK_FILTER_NEAREST;

  samplerCI.mipmapMode = (sampler_state.state & GPU_SAMPLER_MIPMAP)
                             ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                             : VK_SAMPLER_MIPMAP_MODE_NEAREST;

  samplerCI.mipLodBias = 0.0f;

  samplerCI.minLod = -1000;
  samplerCI.maxLod = 1000;

  float aniso_filter = max_ff(16, U.anisotropic_filter);
  samplerCI.maxAnisotropy =
      (sampler_state.state & GPU_SAMPLER_MIPMAP) ? aniso_filter : 1;
  samplerCI.compareEnable =
      (sampler_state.state & GPU_SAMPLER_COMPARE) ? VK_TRUE : VK_FALSE;
  samplerCI.compareOp = (sampler_state.state & GPU_SAMPLER_COMPARE)
                            ? VK_COMPARE_OP_EQUAL
                            : VK_COMPARE_OP_ALWAYS;

  /* Custom sampler for icons. */
  if (sampler_state.state == GPU_SAMPLER_ICON) {
    samplerCI.mipLodBias = -0.5f;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  }


  vkCreateSampler(  vk_device_, &samplerCI, nullptr,
                            &sampler_state_cache_[(uint)sampler_state]);

  return sampler_state_cache_[(uint)sampler_state];
}

VkSampler VKContext::get_default_sampler_state() {
if (default_sampler_state_ == VK_NULL_HANDLE) {
  default_sampler_state_ =
      this->get_sampler_from_state(DEFAULT_SAMPLER_STATE);
}
return default_sampler_state_;
}

VkSampler VKContext::get_sampler_from_state(VKSamplerState sampler_state) {
BLI_assert((uint)sampler_state >= 0 &&
            ((uint)sampler_state) < GPU_SAMPLER_MAX);
return sampler_state_cache_[(uint)sampler_state];
}

VKContext::~VKContext()
{

  for (int i = 0; i < GPU_SAMPLER_MAX; i++) {
    if (sampler_state_cache_[i] != VK_NULL_HANDLE)
      vkDestroySampler(vk_device_, sampler_state_cache_[i], nullptr);
  };

  VKBackend::desable_gpuctx(this,descriptor_pools_);
}

void VKContext::init_physical_device_limits()
{
  BLI_assert(vk_physical_device_ != VK_NULL_HANDLE);
  VkPhysicalDeviceProperties properties = {};
  vkGetPhysicalDeviceProperties(vk_physical_device_, &properties);
  vk_physical_device_limits_ = properties.limits;
}

void VKContext::activate()
{
  if (ghost_window_) {
    VkImage image; /* TODO will be used for reading later... */
    VkFramebuffer vk_framebuffer;
    VkFramebuffer vk_framebuffer;
    VkRenderPass render_pass;
    VkExtent2D extent;
    uint32_t fb_id;

    GHOST_GetVulkanBackbuffer(
        (GHOST_WindowHandle)ghost_window_, &image, &framebuffer, &render_pass, &extent, &fb_id);

    /* Recreate the gpu::VKFrameBuffer wrapper after every swap. */
    delete back_left;

    back_left = new VKFrameBuffer("back_left", framebuffer, render_pass, extent);
    active_fb = back_left;
  }
  immActivate();
}

void VKContext::deactivate()
{
}

void VKContext::begin_frame()
{

  if (vk_in_frame_) {
    return;
  }

  vk_in_frame_ = true;
  BLI_assert(validate_frame());

  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  GHOST_GetVulkanCommandBuffer(static_cast<GHOST_ContextHandle>(ghost_context_), &command_buffer);

  BLI_assert(command_buffer_.init(vk_device_, vk_queue_, command_buffer));
  BLI_assert(validate_image());

  // command_buffer_.begin_recording();

  descriptor_pools_.reset();
}

void VKContext::end_frame()
{
  if (has_active_framebuffer()) {
    deactivate_framebuffer();
  }
  command_buffer_.end_recording(true);

  vk_in_frame_ = false;
}

void VKContext::flush()
{
  /*
  VKFrameBuffer *previous_framebuffer = active_framebuffer_get();
  if (has_active_framebuffer()) {
    deactivate_framebuffer();
  }
  */

  command_buffer_.submit();
  /*
  if (previous_framebuffer != nullptr) {
    activate_framebuffer(*previous_framebuffer);
  }*/
}

void VKContext::finish()
{
  command_buffer_.submit();
}

bool VKContext::validate_frame()
{

  VkSemaphore wait = VK_NULL_HANDLE, fin = VK_NULL_HANDLE;

  uint8_t fb_id = semaphore_get(wait, fin);
  uint8_t current_frame = (fb_id >> 1) & 1;
  bool active_cache = (active_fb == back_left);

  vk_fb_id_ = fb_id;
  VkSemaphore sema_fin = command_buffer_.get_fin_semaphore();
  uint8_t sema_frame = command_buffer_.get_sema_frame();

  if (sema_fin == VK_NULL_HANDLE) {
    if (sema_frame == current_frame) {
      GHOST_SwapWindowBuffers((GHOST_WindowHandle)ghost_window_);
      fb_id = semaphore_get(wait, fin);
      current_frame = (fb_id >> 1) & 1;
      BLI_assert(sema_frame != current_frame);
    }
    command_buffer_.set_remote_semaphore(wait);
    command_buffer_.set_fin_semaphore(fin);
    command_buffer_.set_sema_frame(current_frame);
  }
  else if (sema_fin == fin) {
    BLI_assert(sema_frame == current_frame);
  }
  else {
    BLI_assert_unreachable();
  }

  uint8_t backleft_id  = ((VKFrameBuffer *)back_left)->get_image_id();
  uint8_t current_im  = (fb_id) & 1;

  if (current_im != backleft_id) {
    std::swap(back_left, front_left);
    if (active_cache) {
      active_fb = back_left;
    }
  }

  if (!active_fb) {
    active_fb = back_left;
  }
 printf("FRAMEBUFER Validate  >>>>>>>>>>>>>>>>>>>> back %llx (%s) front %llx (%s) \n",(uint64_t)back_left,back_left->name_get(),(uint64_t)front_left,front_left->name_get());

  BLI_assert(((VKFrameBuffer *)back_left)->get_image_id() == current_im);

  return true;
};

void VKContext::swapchains()
{
  GHOST_SwapWindowBuffers((GHOST_WindowHandle)ghost_window_);
};

bool VKContext::validate_frame()
{

  VkSemaphore wait = VK_NULL_HANDLE, fin = VK_NULL_HANDLE;

  uint8_t fb_id = semaphore_get(wait, fin);
  uint8_t current_frame = (fb_id >> 1) & 1;
  bool active_cache = (active_fb == back_left);

  vk_fb_id_ = fb_id;
  VkSemaphore sema_fin = command_buffer_.get_fin_semaphore();
  uint8_t sema_frame = command_buffer_.get_sema_frame();

  if (sema_fin == VK_NULL_HANDLE) {
    if (sema_frame == current_frame) {
      GHOST_SwapWindowBuffers((GHOST_WindowHandle)ghost_window_);
      fb_id = semaphore_get(wait, fin);
      current_frame = (fb_id >> 1) & 1;
      BLI_assert(sema_frame != current_frame);
    }
    command_buffer_.set_remote_semaphore(wait);
    command_buffer_.set_fin_semaphore(fin);
    command_buffer_.set_sema_frame(current_frame);
  }
  else if (sema_fin == fin) {
    BLI_assert(sema_frame == current_frame);
  }
  else {
    BLI_assert_unreachable();
  }

  uint8_t backleft_id  = ((VKFrameBuffer *)back_left)->get_image_id();
  uint8_t current_im  = (fb_id) & 1;

  if (current_im != backleft_id) {
    std::swap(back_left, front_left);
    if (active_cache) {
      active_fb = back_left;
    }
  }

  if (!active_fb) {
    active_fb = back_left;
  }
 printf("FRAMEBUFER Validate  >>>>>>>>>>>>>>>>>>>> back %llx (%s) front %llx (%s) \n",(uint64_t)back_left,back_left->name_get(),(uint64_t)front_left,front_left->name_get());

  BLI_assert(((VKFrameBuffer *)back_left)->get_image_id() == current_im);

  return true;
};

void VKContext::swapchains()
{
  GHOST_SwapWindowBuffers((GHOST_WindowHandle)ghost_window_);
};

void VKContext::memory_statistics_get(int * /*total_mem*/, int * /*free_mem*/) {}

void VKContext::activate_framebuffer(VKFrameBuffer &framebuffer)
{
  if (has_active_framebuffer()) {
    deactivate_framebuffer();
  }

  BLI_assert(active_fb == nullptr);
  active_fb = &framebuffer;
  command_buffer_.begin_render_pass(framebuffer);
}

bool VKContext::has_active_framebuffer() const
{
  return active_fb != nullptr;
}

void VKContext::deactivate_framebuffer()
{
  BLI_assert(active_fb != nullptr);
  VKFrameBuffer *framebuffer = unwrap(active_fb);
  command_buffer_.end_render_pass(*framebuffer);
  active_fb = nullptr;
}

uint8_t VKContext::semaphore_get(VkSemaphore &wait, VkSemaphore &finish)
{

  uint32_t id = 0;
  ((GHOST_ContextVK *)ghost_context_)->getVulkanSemaphore(&wait, &finish, &id);

  int current_im = id & 1;
  VkImage image; /* TODO will be used for reading later... */
  VkFramebuffer vk_framebuffer;
  VkRenderPass render_pass;
  VkExtent2D extent;
  uint32_t fb_id;

  GHOST_GetVulkanBackbuffer((GHOST_WindowHandle)ghost_window_,
                            &image,
                            &vk_framebuffer,
                            &render_pass,
                            &extent,
                            &fb_id,
                            current_im);
  auto &im = sc_image_get(current_im);
  auto cimage = im.vk_image_handle();
  if (cimage) {
    BLI_assert(cimage == image);
  }

  debug::raise_vk_info(
      "GHOST::internal::connection `VkRederPass[%llx]`\n `VkSemaphore`[wait] %llx  "
      "`VkSemaphore`[finish] %llx    CurrentImage[%llx]   index[%u]  CurrentFrame %u SwapChainID "
      "%u\n",
      (uint64_t)render_pass,
      (uint64_t)wait,
      (uint64_t)finish,
      (uint64_t)image,
      current_im,
      (id >> 1) & 1,
      id >> 2);
  return static_cast<uint8_t>(id);
}

/* -------------------------------------------------------------------- */
/** \name Framebuffer
 * \{ */

void VKContext::activate_framebuffer(VKFrameBuffer &framebuffer)
{
  if (has_active_framebuffer()) {
    deactivate_framebuffer();
  }

  BLI_assert(active_fb == nullptr);
  active_fb = &framebuffer;

  if (!vk_in_frame_) {
    begin_frame();
  }


  if(command_buffer_.ensure_render_pass()){
      active_fb = &framebuffer;
      return;
  };

  VKFrameBuffer* new_fb = reinterpret_cast<VKFrameBuffer*>(active_fb);

  if (new_fb->is_immutable()) {
    BLI_assert(
        command_buffer_.begin_render_pass(*new_fb, vk_swap_chain_images_[vk_fb_id_ & 1]));
  }
  else {
    VKTexture &tex = (*(VKTexture *)new_fb->color_tex(0));
    BLI_assert(command_buffer_.begin_render_pass(*new_fb, tex));
  }

  active_fb = new_fb;

}

VKFrameBuffer *VKContext::active_framebuffer_get() const
{
  return unwrap(active_fb);
}

bool VKContext::has_active_framebuffer() const
{
  return active_framebuffer_get() != nullptr;
}

void VKContext::deactivate_framebuffer()
{
  if (command_buffer_.is_begin_rp()) {
    VKFrameBuffer *framebuffer = active_framebuffer_get();
    BLI_assert(framebuffer != nullptr);
    command_buffer_.end_render_pass(*framebuffer);
  }
  active_fb = nullptr;
}

void VKContext::swapbuffers()
{
  GHOST_SwapWindowBuffers((GHOST_WindowHandle)ghost_window_);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Graphics pipeline
 * \{ */
void VKContext::bind_graphics_pipeline()
{
  VKShader *shader = unwrap(this->shader);
  BLI_assert(shader);
  shader->update_graphics_pipeline(*this);
  command_buffer_get().bind(shader->pipeline_get(), VK_PIPELINE_BIND_POINT_GRAPHICS);
}

void VKContext::bind_graphics_pipeline(const VKBatch &batch,
                                       const VKVertexAttributeObject &vertex_attribute_object)
{
  VKShader *shader = unwrap(this->shader);
  BLI_assert(shader);
  shader->update_graphics_pipeline(*this, batch, vertex_attribute_object);
  command_buffer_get().bind(shader->pipeline_get(), VK_PIPELINE_BIND_POINT_GRAPHICS);
  shader->pipeline_get().push_constants_get().update(*this);

  VKDescriptorSetTracker &descriptor_set = shader->pipeline_get().descriptor_set_get();
  descriptor_set.update(*this);
  descriptor_set.bindcmd( command_buffer_,shader->vk_pipeline_layout_get());

}
/** \} */

}  // namespace blender::gpu
