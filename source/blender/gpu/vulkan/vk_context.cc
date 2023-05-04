/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "DNA_userdef_types.h"

#include "vk_context.hh"
#include "vk_debug.hh"

#include "vk_backend.hh"
#include "vk_vertex_buffer.hh"
#include "vk_framebuffer.hh"
#include "vk_immediate.hh"
#include "vk_memory.hh"
#include "vk_shader.hh"
#include "vk_state_manager.hh"

#include "GHOST_C-api.h"

#include "intern/GHOST_ContextVk.hh"

namespace blender::gpu {

bool VKContext::base_instance_support = false;

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

  debug::init_callbacks(this, vkGetInstanceProcAddr);
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

  state_manager = new VKStateManager();
  imm = new VKImmediate();

  /* For off-screen contexts. Default frame-buffer is empty. */
  back_left = new VKFrameBuffer("back_left");
  active_fb = back_left;
  vk_swap_chain_images_.resize(vk_im_prop.nums);
  command_buffer_.create_resource(this);

  vk_in_frame_ = false;
  default_vbo_dummy=nullptr;
}

VKContext::~VKContext()
{
  auto vbo = ((GPUVertBuf*)default_vbo_dummy);
  GPU_VERTBUF_DISCARD_SAFE( vbo);
  if(imm)
  {
    delete imm;
    imm = nullptr;
  }
  if(back_left)
  {
    delete back_left;
    back_left = nullptr;
  }
  VKBackend::desable_gpuctx(this, descriptor_pools_);
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
    VkRenderPass render_pass;
    VkExtent2D extent;
    uint32_t fb_id;

    GHOST_GetVulkanBackbuffer((GHOST_WindowHandle)ghost_window_,
                              &image,
                              &vk_framebuffer,
                              &render_pass,
                              &extent,
                              &fb_id,
                              0);
    active_fb = nullptr;
    delete back_left;
    back_left = new VKFrameBuffer("swapchain-0", vk_framebuffer, render_pass, extent);
    ((VKFrameBuffer *)back_left)->set_image_id(0);

    GHOST_GetVulkanBackbuffer((GHOST_WindowHandle)ghost_window_,
                              &image,
                              &vk_framebuffer,
                              &render_pass,
                              &extent,
                              &fb_id,
                              1);
    delete front_left;
    front_left = new VKFrameBuffer("swapchain-1", vk_framebuffer, render_pass, extent);
    ((VKFrameBuffer *)front_left)->set_image_id(1);

    uint32_t current_im = fb_id & 1;

    if (current_im == 1) {
      std::swap(back_left, front_left);
    }

    printf("FRAMEBUFER Reallocate  >>>>>>>>>>>>>>>>>>>> back %llx (%s) front %llx (%s) \n",
           (uint64_t)back_left,
           back_left->name_get(),
           (uint64_t)front_left,
           front_left->name_get());

    active_fb = back_left;
    back_left->bind(false);
  }
  immActivate();
}

void VKContext::deactivate()
{
  immDeactivate();
}

void VKContext::begin_frame()
{
  if(default_vbo_dummy==nullptr)
  {
    GPUVertFormat format = {0};
    GPU_vertformat_attr_add(&format, "dummy_vbo", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    default_vbo_dummy =  (VKVertexBuffer*)GPU_vertbuf_create_with_format_ex(&format,GPU_USAGE_DYNAMIC);
    GPU_vertbuf_data_alloc((GPUVertBuf*)default_vbo_dummy, 1);
    float dummy[4] = {1.f,2.f,3.f,4.f};
    default_vbo_dummy->update_sub(0,4*sizeof(float),dummy);
  }
  if (vk_in_frame_) {
    return;
  }

  vk_in_frame_ = true;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  GHOST_GetVulkanCommandBuffer(static_cast<GHOST_ContextHandle>(ghost_context_), &command_buffer);

  BLI_assert(command_buffer_.init(vk_device_, vk_queue_, command_buffer));
  VKContext* gpu_ctx = VKBackend::gpu_ctx_get();
  if(gpu_ctx)
  {
    BLI_assert(gpu_ctx->validate_frame());
    BLI_assert(gpu_ctx->validate_image());
  }
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

  if (has_active_framebuffer()) {
    deactivate_framebuffer();
  }

  command_buffer_.submit(false, true);
  vk_in_frame_ = false;
}

void VKContext::flush(bool toggle, bool fin, bool activate)
{

  VKFrameBuffer *previous_framebuffer = nullptr;

  if (activate) {
    previous_framebuffer = active_framebuffer_get();
    if (has_active_framebuffer()) {
      deactivate_framebuffer();
    }
  }

  command_buffer_.submit(toggle, fin);

  if (activate && (previous_framebuffer != nullptr)) {
    activate_framebuffer(*previous_framebuffer);
  }

  if (fin) {
    vk_in_frame_ = false;
  }
}

bool VKContext::validate_image()
{

  VkImage image;
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
                            vk_fb_id_ & 1);
  uint8_t current_im = fb_id & 1;
  BLI_assert((vk_fb_id_ & 1) == current_im);
  auto &im = sc_image_get(current_im);

  if (!im.is_valid(image)) {

    im.init(image, vk_im_prop.format);

    command_buffer_.begin_recording();

    im.current_layout_set(VK_IMAGE_LAYOUT_UNDEFINED);

    command_buffer_.image_transition(&im, VkTransitionState::VK_BEFORE_PRESENT, false);

    flush(true, false, true);
  }

  return true;
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

  uint8_t backleft_id = ((VKFrameBuffer *)back_left)->get_image_id();
  uint8_t current_im = (fb_id)&1;

  if (current_im != backleft_id) {
    std::swap(back_left, front_left);
    if (active_cache) {
      active_fb = back_left;
    }
  }

  if (!active_fb) {
    active_fb = back_left;
  }
  printf("FRAMEBUFER Validate  >>>>>>>>>>>>>>>>>>>> back %llx (%s) front %llx (%s) \n",
         (uint64_t)back_left,
         back_left->name_get(),
         (uint64_t)front_left,
         front_left->name_get());

  BLI_assert(((VKFrameBuffer *)back_left)->get_image_id() == current_im);

  return true;
};

void VKContext::swapchains()
{
  GHOST_SwapWindowBuffers((GHOST_WindowHandle)ghost_window_);
};

void VKContext::memory_statistics_get(int * /*total_mem*/, int * /*free_mem*/) {}

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

  if (command_buffer_.ensure_render_pass()) {
    active_fb = &framebuffer;
    return;
  };

  VKFrameBuffer *new_fb = reinterpret_cast<VKFrameBuffer *>(active_fb);

  if (new_fb->is_immutable()) {
    BLI_assert(command_buffer_.begin_render_pass(*new_fb, &vk_swap_chain_images_[vk_fb_id_ & 1]));
  }
  else {
    BLI_assert(command_buffer_.begin_render_pass(*new_fb, (VKTexture*)nullptr));
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
void VKContext::bind_graphics_pipeline(GPUPrimType prim_type,
                                       const VKVertexAttributeObject &vertex_attribute_object)
{
  VKShader *shader = unwrap(this->shader);
  BLI_assert(shader);
  shader->update_graphics_pipeline(*this, prim_type, vertex_attribute_object);
  command_buffer_get().bind(shader->pipeline_get(), VK_PIPELINE_BIND_POINT_GRAPHICS);
  shader->pipeline_get().push_constants_get().update(*this);
  VKDescriptorSetTracker &descriptor_set = shader->pipeline_get().descriptor_set_get();
  if(descriptor_set.update(*this))
  {
    descriptor_set.bindcmd(command_buffer_, shader->vk_pipeline_layout_get());
  }
}

void VKContext::bind_graphics_pipeline(const VKBatch &batch,
                                       const VKVertexAttributeObject &vertex_attribute_object)
{
  VKShader *shader = unwrap(this->shader);
  BLI_assert(shader);
  shader->update_graphics_pipeline(*this, batch, vertex_attribute_object);
  command_buffer_get().bind(shader->pipeline_get(), VK_PIPELINE_BIND_POINT_GRAPHICS);
  shader->pipeline_get().push_constants_get().update(*this);
  printf("BIND PIPELINE %llx  Shader %llx  %s\n",(uintptr_t)&shader->pipeline_get(),(uintptr_t)shader,shader->name_get());
  VKDescriptorSetTracker &descriptor_set = shader->pipeline_get().descriptor_set_get();
  if(descriptor_set.update(*this))
  {
    descriptor_set.bindcmd(command_buffer_, shader->vk_pipeline_layout_get());
  }
}
/** \} */

}  // namespace blender::gpu
