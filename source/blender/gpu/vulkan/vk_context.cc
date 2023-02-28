/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_context.hh"
#include "BKE_global.h"
#include "BLI_assert.h"
#include "BLI_utildefines.h"
#include "vk_backend.hh"
#include "vk_debug.hh"
#include "vk_framebuffer.hh"
#include "vk_immediate.hh"
#include "vk_state.hh"
#include "vk_vertex_buffer.hh"

#include "GHOST_C-api.h"
#include "GPU_capabilities.h"
#include "GPU_common_types.h"
#include "GPU_context.h"
#include "gpu_context_private.hh"


#include "intern/GHOST_Context.h"
#include "intern/GHOST_ContextVK.h"
#include "intern/GHOST_Window.h"


void GHOST_VulkanCreateSwapchainRenderPass(){
  blender::gpu::VKContext::get()->create_swapchain_fb();
  return;
};
void GHOST_VulkanSwapchainTransition()
{
  auto ctx = GPU_context_active_get();
  BLI_assert(ctx);
  GPU_context_end_frame(ctx);
  GPU_context_begin_frame(ctx);
};

namespace blender::gpu {

void VKContext::destroyMemAllocator()
{

  if (mem_allocator_ != VK_NULL_HANDLE) {
    vmaDestroyAllocator(mem_allocator_);
  }
  mem_allocator_ = VK_NULL_HANDLE;
  mem_allocator_info = {};
};

VmaAllocator VKContext::mem_allocator_get()
{
  return mem_allocator_;
}

/* Global memory manager. */
uint32_t VKContext::max_cubemap_size = 0;
uint32_t VKContext::max_ubo_size = 0;
uint32_t VKContext::max_ubo_binds = 0;
uint32_t VKContext::max_ssbo_size = 0;
uint32_t VKContext::max_ssbo_binds = 0;
uint32_t VKContext::max_push_constants_size = 0;
uint32_t VKContext::max_inline_ubo_size = 0;
bool VKContext::multi_draw_indirect_support = 0;
bool VKContext::vertex_attrib_binding_support = false;

VKContext::VKContext(void *ghost_window,
                     void *ghost_context,
                     VKSharedOrphanLists &shared_orphan_list)
    :vk_submitter_(this) ,shared_orphan_list_(shared_orphan_list)
{

  for (int i = 0; i < GPU_SAMPLER_MAX; i++)
    sampler_state_cache_[i] = VK_NULL_HANDLE;
  init(ghost_window, ghost_context);
  buffer_manager_ = new VKStagingBufferManager(*this);

  auto ctx_ = GPU_context_active_get();

  GPU_context_active_set((GPUContext *)this);

  /*NOTE: Frame Check */
  for (int i = 0; i < 4; i++) {
    GPU_context_begin_frame(GPU_context_active_get());

    GPU_context_end_frame(GPU_context_active_get());
  }

  float data[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  VKResourceOptions options;
  options.setDeviceLocal(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "default_attr", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  auto default_attr_vbo = GPU_vertbuf_create_with_format_ex(&format, GPU_USAGE_STATIC);
  default_attr_vbo_ = (VKVertBuf *)default_attr_vbo;
  GPU_vertbuf_data_alloc(default_attr_vbo, 1);
  void *dst = GPU_vertbuf_get_data(default_attr_vbo);
  memcpy(dst, data, 16);
  default_attr_vbo_->bind();

  GPU_context_active_set(ctx_);
};

VKContext::~VKContext()
{

  GPUVertBuf *vbo = ((GPUVertBuf *)default_attr_vbo_);
  GPU_VERTBUF_DISCARD_SAFE(vbo);

#define DELE(a) \
  if (a) { \
    delete a; \
    a = nullptr; \
  }

  for (VKFrameBuffer *fb_ : frame_buffers_) {
    DELE(fb_);
  }

  DELE(state_manager);
  DELE(imm);
  DELE(this->back_left);
  DELE(this->front_left);
  DELE(buffer_manager_);
#undef DELE

  for (auto command_buffer : vk_cmd_primaries_) {
    vkFreeCommandBuffers(device_, vk_command_pool_, 1, &command_buffer);
  }
  vk_cmd_primaries_.clear();

  if (vk_command_pool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device_, vk_command_pool_, NULL);
  }

  for (int i = 0; i < GPU_SAMPLER_MAX; i++) {
    if (sampler_state_cache_[i] != VK_NULL_HANDLE)
      vkDestroySampler(device_, sampler_state_cache_[i], nullptr);
  };
  vmaDestroyAllocator(mem_allocator_);
  mem_allocator_ = VK_NULL_HANDLE;
};

void VKContext::create_swapchain_fb()
{
  if (this->front_left) {
    auto fb = static_cast<VKFrameBuffer *>(this->front_left);
    fb->create_swapchain_frame_buffer(0);
  }
  if (this->back_left) {
    auto fb = static_cast<VKFrameBuffer *>(this->back_left);
    fb->create_swapchain_frame_buffer(1);
  }
};

void VKContext::init(void *ghost_window, void *ghost_context)
{
  if (G.debug) {  //& G_DEBUG_GPU
                  /// debug::init_vk_callbacks();
  }
  ghost_window_ = ghost_window;
  if (ghost_window_ && ghost_context == NULL) {
    GHOST_SetDrawingContextType((GHOST_WindowHandle)ghost_window_,
                                GHOST_kDrawingContextTypeVulkan);
    GHOST_Window *ghostWin = reinterpret_cast<GHOST_Window *>(ghost_window_);
    ghost_context = (ghostWin ? ghostWin->getContext() : NULL);
  }
  BLI_assert(ghost_context);
  this->ghost_context_ = ghost_context;
  // gcontext->initializeDrawingContext();
  GHOST_GetVulkanHandles(GHOST_ContextHandle(ghost_context),
                         &instance_,
                         &physical_device_,
                         &device_,
                         &queue_,
                         &graphic_queue_familly_);
  
  for (int i = 0; i < 2; i++) {
    vk_sw_layouts[current_img_index_] = VK_IMAGE_LAYOUT_UNDEFINED;
  }
  /* Initialize the memory allocator. */
  // VmaAllocatorCreateInfo info = {};
  /* Should use same vulkan version as GHOST. */
  if (mem_allocator_ == VK_NULL_HANDLE) {
    mem_allocator_info.vulkanApiVersion = VK_API_VERSION_1_2;
    mem_allocator_info.physicalDevice = physical_device_;
    mem_allocator_info.device = device_;
    mem_allocator_info.instance = instance_;
    vmaCreateAllocator(&mem_allocator_info, &mem_allocator_);
  }

  VKBackend::capabilities_init(this);
  VKBackend::platform_init(this);

  state_manager = new VKStateManager(this);
  imm = new VKImmediate(this);

  is_swapchain_ = false;
  if (ghost_window) {
    is_swapchain_ = true;
    GHOST_RectangleHandle bounds = GHOST_GetClientBounds((GHOST_WindowHandle)ghost_window);
    // int w = GHOST_GetWidthRectangle(bounds);
    // int h = GHOST_GetHeightRectangle(bounds);
    GHOST_DisposeRectangle(bounds);

    /* Create FrameBuffer handles. */
    int i = 0;
    for (auto name : {"front_left", "back_left"}) {
      VKFrameBuffer *vk_fb = new VKFrameBuffer(name, this);
      vk_fb->create_swapchain_frame_buffer(i++);
      if (i == 1) {
        this->front_left = vk_fb;
      }
      else {
        this->back_left = vk_fb;
      }
    }
/*IF0 extern API */
#if 0
       GLuint default_fbo = GHOST_GetDefaultOpenGLFramebuffer((GHOST_WindowHandle)ghost_window);
       if (default_fbo != 0) {
          * Bind default framebuffer, otherwise state might be undefined because of
          * detect_mip_render_workaround().
         glBindFramebuffer(GL_FRAMEBUFFER, default_fbo);
         front_left = new VKFrameBuffer("front_left", this, GL_COLOR_ATTACHMENT0, default_fbo, w,
     h); back_left = new VKFrameBuffer("back_left", this, GL_COLOR_ATTACHMENT0, default_fbo, w, h);
       }
       else {
         front_left = new VKFrameBuffer("front_left", this, GL_FRONT_LEFT, 0, w, h);
         back_left = new VKFrameBuffer("back_left", this, GL_BACK_LEFT, 0, w, h);
       }

       GLboolean supports_stereo_quad_buffer = GL_FALSE;
       glGetBooleanv(GL_STEREO, &supports_stereo_quad_buffer);
       if (supports_stereo_quad_buffer) {
         front_right = new GLFrameBuffer("front_right", this, GL_FRONT_RIGHT, 0, w, h);
         back_right = new GLFrameBuffer("back_right", this, GL_BACK_RIGHT, 0, w, h);
       }

#endif
  }
  else {
    /* For off-screen contexts. Default frame-buffer is null. */
    /* back_left = new VKFrameBuffer("back_left", this, GL_NONE, 0, 0, 0); */
    this->back_left = new VKFrameBuffer("back_left", this);
  }

  /* Initialize Render-pass and Frame-buffer State. */

  /* Initialize command buffer state. */
  // this->main_command_buffer.prepare();
  /* Initialize IMM and pipeline state */
  // this->pipeline_state.initialised = false;

  /*
   this->queue = (id<MTLCommandQueue>)this->ghost_context_->metalCommandQueue();
   this->device = (id<MTLDevice>)this->ghost_context_->metalDevice();
   BLI_assert(this->queue);
   BLI_assert(this->device);
   */

  /* Initialize samplers. */
  for (uint i = 0; i < GPU_SAMPLER_MAX; i++) {
    VKSamplerState state;
    state.state = static_cast<eGPUSamplerState>(i);
    sampler_state_cache_[i] = this->generate_sampler_from_state(state);
  }

  this->active_fb = this->back_left;
  static_cast<VKStateManager *>(state_manager)->active_fb = static_cast<VKFrameBuffer *>(
      active_fb);

/*IF0: ClearSwapchain initial check*/
#if 0
  if (is_swapchain_) {

#  if 0
    auto gcontext = (GHOST_ContextVK *)ghost_context;

    auto func_sb = [&]() {
      GHOST_ContextVK *gcontext = (GHOST_ContextVK *)this->ghost_context_;
      if (gcontext->is_inside_frame()) {
        static_cast<VKFrameBuffer *>(this->active_fb)->render_end();

        gcontext->finalize_image_layout();
        end_frame();
      }
    };
    auto f_sb = std::function<void(void)>(func_sb);
    gcontext->set_fb_sb_cb(f_sb);

    auto func_sb2 = [&]() {

      GHOST_ContextVK *gcontext = (GHOST_ContextVK *)this->ghost_context_;
      float r = (float)gcontext->getCurrentImage();
      auto fb = static_cast<VKFrameBuffer *>(this->active_fb);
      float color[4] = {r, 1., 0., 1.};

      VkClearValue clearValues[2];
      for (int i = 0; i < 4; i++) {
        clearValues[0].color.float32[i] = color[i];
      }

      clearValues[1].depthStencil.depth = 1.f;
      clearValues[1].depthStencil.stencil = 0;
      fb->render_begin(
          VK_NULL_HANDLE, VK_COMMAND_BUFFER_LEVEL_PRIMARY, (VkClearValue *)clearValues);

      //
      // gcontext->clear_color(cmd, (VkClearColorValue*)& clearValues[0].color.float32[0]);

      fb->render_end();
    };

    auto f_sb2 = std::function<void(void)>(func_sb2);
    gcontext->set_fb_sb2_cb(f_sb2);


    auto ctx_ = GPU_context_active_get();

    GPU_context_active_set((GPUContext *)this);

    gcontext->init_image_layout();

    GPU_context_active_set(ctx_);
#  endif

  }
#endif

};
void VKContext::clear_color(VkCommandBuffer cmd, const VkClearColorValue *clearValues)
{
  auto gcontext_ = static_cast<GHOST_ContextVK *>(ghost_context_);
  gcontext_->clear_color(cmd, clearValues);
};

void VKSharedOrphanLists::orphans_clear()
{
  /* Check if any context is active on this thread! */
  BLI_assert(VKContext::get());

  lists_mutex.lock();
  if (!buffers.is_empty()) {
    /// <summary>
    /// Delete Buffers
    /// </summary>
    /// glDeleteBuffers(uint(buffers.size()), buffers.data());
    buffers.clear();
  }
  if (!textures.is_empty()) {
    /// glDeleteTextures(uint(textures.size()), textures.data());
    textures.clear();
  }
  lists_mutex.unlock();
};

void VKContext::orphans_clear()
{
  /* Check if context has been activated by another thread! */
  BLI_assert(this->is_active_on_thread());

  lists_mutex_.lock();
  if (!orphaned_vertarrays_.is_empty()) {
    /// glDeleteVertexArrays(uint(orphaned_vertarrays_.size()), orphaned_vertarrays_.data());
    orphaned_vertarrays_.clear();
  }
  if (!orphaned_framebuffers_.is_empty()) {
    /// glDeleteFramebuffers(uint(orphaned_framebuffers_.size()), orphaned_framebuffers_.data());
    orphaned_framebuffers_.clear();
  }
  lists_mutex_.unlock();

  shared_orphan_list_.orphans_clear();
};

void VKContext::fbo_free(VkFramebuffer fbo_id)
{
  if (this == VKContext::get()) {
    BLI_assert(false);
    /// glDeleteFramebuffers(1, &fbo_id);
  }
  else {
    orphans_add(orphaned_framebuffers_, lists_mutex_, fbo_id);
  }
}
void VKContext::buf_free(VKBuffer *buf)
{

  if (!buf) {
    return;
  };
  /* Any context can free. */
  if (VKContext::get()) {
    if (buf) {
      delete buf;
    }
  }
  else {
    VKSharedOrphanLists &orphan_list =
        static_cast<VKBackend *>(VKBackend::get())->shared_orphan_list_get();
    orphans_add(orphan_list.buffers, orphan_list.lists_mutex, buf);
  }
};

void VKContext::vao_free(VKVAOty buf)
{

  if (!buf) {
    return;
  }

  if (this == VKContext::get()) {
    delete buf;
  }
  else {
    orphans_add(orphaned_vertarrays_, lists_mutex_, buf);
  }
};

void VKContext::activate()

{
  /* Make sure no other context is already bound to this thread. */
  BLI_assert(is_active_ == false);

  is_active_ = true;
  thread_ = pthread_self();

  /* Clear accumulated orphans. */
  orphans_clear();

  if (ghost_window_) {
    /* Get the correct framebuffer size for the internal framebuffers. */
    GHOST_RectangleHandle bounds = GHOST_GetClientBounds((GHOST_WindowHandle)ghost_window_);
    int w = GHOST_GetWidthRectangle(bounds);
    int h = GHOST_GetHeightRectangle(bounds);
    GHOST_DisposeRectangle(bounds);

    if (front_left) {
      front_left->size_set(w, h);
    }
    if (back_left) {
      back_left->size_set(w, h);
    }
    if (front_right) {
      front_right->size_set(w, h);
    }
    if (back_right) {
      back_right->size_set(w, h);
    }
  }

  /* Not really following the state but we should consider
   * no ubo bound when activating a context.
  bound_ubo_slots = 0;
  */

  immActivate();
}

void VKContext::deactivate()
{
  immDeactivate();
  is_active_ = false;
}

void VKContext::getImageView(VkImageView &view, int i)
{

  auto context = ((GHOST_ContextVK *)ghost_context_);
  context->getImageView(view, i);
};

int VKContext::getImageViewNums()
{
  auto context = ((GHOST_ContextVK *)ghost_context_);
  return context->getImageCount();
};

void VKContext::getRenderExtent(VkExtent2D &_render_extent)
{
  auto context = ((GHOST_ContextVK *)ghost_context_);
  context->getRenderExtent(_render_extent);
};



void VKContext::begin_frame()
{
  if (is_inside_frame_) {
    return;
  }

  if (is_swapchain_) {
    VkSemaphore curr_sema_[2];
    if( GHOST_kSuccess != GHOST_VulkanAcquireFrame((GHOST_ContextHandle)ghost_context_,
                             &current_frame_index_,
                             &current_img_index_,
                             (void *)&current_swapchain_img_,
                             &current_swapchain_img_layout_,
                             &current_swapchain_img_format_,
                             &curr_sema_[0]
      )){
      clear_sw();
    };
    
    vk_submitter_.wait_sema_sw_ = curr_sema_[0];
    vk_submitter_.fin_sema_sw_   = curr_sema_[1];
    vk_submitter_.is_final_sw_ = false;
    vk_submitter_.is_init_sw_   = false;
    vk_submitter_.signal_sema_sw_  = VK_NULL_HANDLE;
    
    vk_submitter_.init_image_layout();
    debug::pushMarker(queue_get(), "AcquireSwapImage");
    
    is_inside_frame_ = true;
    
  }
};

void VKContext::clear_sw()
{
  vk_submitter_.clear();
  for (int i = 0; i < 2; i++) {
    vk_sw_layouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;
  }
}




void VKContext::end_frame()
{


  if (is_inside_frame_ ) {
    if (is_swapchain_) {
      VKFrameBuffer *fb = static_cast<VKFrameBuffer *>(this->active_fb);
      
      fb->render_end();
      if (!vk_submitter_.is_final_sw_) {
        vk_submitter_.is_init_sw_ = true;
      }
      vk_submitter_.finalize_sw_submit();
      vk_submitter_.finalize_image_layout();
      debug::popMarker(queue_get());
      if (GHOST_VulkanPresentFrame((GHOST_ContextHandle)ghost_context_) == GHOST_kFailure) {
        clear_sw();
       
      };
      is_inside_frame_ = false;
      vk_submitter_.clear();
    }
  }
}



void VKContext::begin_submit_simple(VkCommandBuffer &cmd, bool ofscreen)
{

  vk_submitter_.begin_submit_simple(cmd, ofscreen);
  debug::pushMarker(cmd, "SimpleSubmit");
  current_cmd_ = cmd;
};
void VKContext::end_submit_simple()
{
  debug::popMarker(current_cmd_);
  vk_submitter_.end_submit_simple();
};

int VKContext::begin_onetime_submit(VkCommandBuffer cmd)
{
  return vk_submitter_.begin_onetime_submit(cmd);
};
void VKContext::end_onetime_submit(int i)
{
  vk_submitter_.end_onetime_submit(i);
  is_onetime_commit_ = false;
}
bool VKContext::begin_blit_submit(VkCommandBuffer &cmd)
{
  return (bool)vk_submitter_.begin_blit_submit(cmd);
};
bool VKContext::end_blit_submit(VkCommandBuffer &cmd, std::vector<VkSemaphore> batch_signal)
{
  return (bool)vk_submitter_.end_blit_submit(cmd, batch_signal);
};

int VKContext::begin_offscreen_submit(VkCommandBuffer cmd)
{
  vk_submitter_.begin_offscreen_submit(cmd);
  return -1;
};
void VKContext::end_offscreen_submit(VkCommandBuffer &cmd, VkSemaphore wait, VkSemaphore signal)
{
  vk_submitter_.end_offscreen_submit(cmd, wait, signal);
};

#if 0
void VKContext::submit()
{
  auto context = ((GHOST_ContextVK *)ghost_context_);
  context->waitCustom();
};
void VKContext::submit_nonblocking()
{
  auto context = ((GHOST_ContextVK *)ghost_context_);
  context->submit_nonblocking();
};
#endif
#if 0
void VKContext::begin_render_pass(VkCommandBuffer &cmd, int i)
{

  auto context = ((GHOST_ContextVK *)ghost_context_);
  i = (i == -1) ? context->getFrame() : i;
  context->begin_frame(cmd, i);
  cmd_valid_[i] = false;
};
void VKContext::end_render_pass(VkCommandBuffer &cmd, int i)
{
  auto context = ((GHOST_ContextVK *)ghost_context_);
  i = (i == -1) ? context->getFrame() : i;
  context->end_frame();
  cmd_valid_[i] = true;
};

void VKContext::begin_submit(int N)
{
  auto context = ((GHOST_ContextVK *)ghost_context_);
  context->begin_submit(N);
};
void VKContext::end_submit()
{
  auto context = ((GHOST_ContextVK *)ghost_context_);
  context->end_submit();

  context->finalize_image_layout();
};
#endif

VkPhysicalDevice VKContext::get_physical_device()
{
  auto ctx = (GHOST_ContextVK *)(ghost_context_);
  return ctx->getPhysicalDevice();
};

VkCommandBuffer VKContext::request_command_buffer()
{

  /*TODO:Designing secondary commands.*/
  #if 0
      if (secondary) {
        int stored_nums = static_cast<int>(m_seco_command_buffers.size());
        if (secondary_index_ < stored_nums) {
          cmd = m_seco_command_buffers[secondary_index_];
        }
        else {

          BLI_assert(m_command_pool != VK_NULL_HANDLE);
          VkCommandBufferAllocateInfo alloc_info = {};
          alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
          alloc_info.commandPool = m_command_pool;
          alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
          alloc_info.commandBufferCount = 1;

          VK_CHECK(vkAllocateCommandBuffers(m_device, &alloc_info, &cmd));

          m_seco_command_buffers.push_back(cmd);
        }
        secondary_index_++;
        return GHOST_kSuccess;
      }
  #endif
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  {
    if (vk_command_pool_ == VK_NULL_HANDLE) {
      VkCommandPoolCreateInfo poolInfo = {};
      poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      poolInfo.queueFamilyIndex = graphic_queue_familly_;

      VK_CHECK(vkCreateCommandPool(device_, &poolInfo, NULL, &vk_command_pool_));
      cmd_prim_id_ = 0;
    }
  }

  int stored_nums = static_cast<int>(vk_cmd_primaries_.size());
  if (cmd_prim_id_ >=0 && cmd_prim_id_ < stored_nums) {

    cmd = vk_cmd_primaries_[cmd_prim_id_];

  }
  else {

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = vk_command_pool_;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(device_, &alloc_info, &cmd));

    vk_cmd_primaries_.append(cmd);
    cmd_prim_id_++;
  }


  return cmd;
};



bool VKContext::is_support_format(VkFormat format, VkFormatFeatureFlagBits flag, bool linear)
{
  /* Check if the device supports blitting to linear images */
  /* reference =>
   * https://github.com/SaschaWillems/Vulkan/blob/79d0c5e436623436b6297a8c81fb3ee8ff78d804/examples/screenshot/screenshot.cpp#L194
   */
  VkFormatProperties formatProps = {};
  auto context = ((GHOST_ContextVK *)ghost_context_);

  vkGetPhysicalDeviceFormatProperties(context->getPhysicalDevice(), format, &formatProps);
  if (linear) {
    return (formatProps.linearTilingFeatures & flag);
  }

  return (formatProps.optimalTilingFeatures & flag);
}
void VKContext::flush()
{
}

void VKContext::finish()
{
}

void VKContext::memory_statistics_get(int * /*total_mem*/, int * /*free_mem*/)
{
}

void VKContext::debug_group_begin(const char *, int)
{
}

void VKContext::debug_group_end()
{
}
blender::gpu::VKFrameBuffer *VKContext::get_default_framebuffer()
{
  return static_cast<VKFrameBuffer *>(this->back_left);
}
blender::gpu::VKFrameBuffer *VKContext::get_current_framebuffer()
{
  VKFrameBuffer *last_bound = static_cast<VKFrameBuffer *>(this->active_fb);
  return last_bound ? last_bound : this->get_default_framebuffer();
}
void VKContext::set_viewport(int origin_x, int origin_y, int width, int height)
{

  BLI_assert(this);
  BLI_assert(width > 0);
  BLI_assert(height > 0);
  BLI_assert(origin_x >= 0);
  BLI_assert(origin_y >= 0);

  auto &pipeline_state = ((VKStateManager *)this->state_manager)->getPipelineStateCI();
  auto &vp = pipeline_state.viewport_cache;
  bool changed = ((int)vp.x != origin_x) || ((int)vp.y != origin_y) || ((int)vp.width != width) ||
                 ((int)vp.height != height);

  vp = {(float)origin_x, (float)origin_y, (float)width, (float)height, 0., 1.};

  if (changed) {
    pipeline_state.dirty_flags = (VKPipelineStateDirtyFlag)((pipeline_state.dirty_flags |
                                                             VK_PIPELINE_STATE_VIEWPORT_FLAG));
  }
}

void VKContext::set_scissor(int scissor_x, int scissor_y, int scissor_width, int scissor_height)
{
  BLI_assert(this);
  auto &pipeline_state = ((VKStateManager *)this->state_manager)->getPipelineStateCI();
  auto &sc = pipeline_state.scissor_cache;

  bool changed = (sc.offset.x != scissor_x) || (sc.offset.y != scissor_y) ||
                 (sc.extent.width != scissor_width) || (sc.extent.height != scissor_height);

  sc = {{scissor_x, scissor_y}, {(uint)scissor_width, (uint)scissor_height}};

  /* TODO: dynamic state scissor test */
  ///(pipeline_state.scissor_enabled != true);

  if (changed) {
    pipeline_state.dirty_flags = (VKPipelineStateDirtyFlag)(pipeline_state.dirty_flags |
                                                            VK_PIPELINE_STATE_SCISSOR_FLAG);
  }
}

void VKContext::set_scissor_enabled(bool scissor_enabled)
{
  auto pipeline_state = ((VKStateManager *)this->state_manager)->getPipelineStateCI();

  /* Only turn on Scissor if requested scissor region is valid */
  scissor_enabled = scissor_enabled && (pipeline_state.scissor_cache.extent.width > 0 &&
                                        pipeline_state.scissor_cache.extent.height > 0);

  bool changed = (pipeline_state.scissor_enabled != scissor_enabled);
  pipeline_state.scissor_enabled = scissor_enabled;
  if (changed) {
    pipeline_state.dirty_flags = (VKPipelineStateDirtyFlag)(pipeline_state.dirty_flags |
                                                            VK_PIPELINE_STATE_SCISSOR_FLAG);
  }
}

void VKContext::framebuffer_bind(VKFrameBuffer *framebuffer)
{
  /* We do not yet begin the pass -- We defer beginning the pass until a draw is requested. */
  BLI_assert(framebuffer);
  this->active_fb = framebuffer;
}

void VKContext::framebuffer_restore()
{
  /* Bind default framebuffer from context --
   * We defer beginning the pass until a draw is requested. */
  this->active_fb = this->back_left;
}

VkPipelineCache VKContext::get_pipeline_cache()
{

  return ((GHOST_ContextVK *)ghost_context_)->getPipelineCache();
};

VkQueue VKContext::queue_get()
{
  return queue_;
}
VkRenderPass VKContext::get_renderpass()
{
  return ((GHOST_ContextVK *)ghost_context_)->getRenderPass();
};
VkSampler VKContext::get_default_sampler_state()
{
  if (default_sampler_state_ == VK_NULL_HANDLE) {
    default_sampler_state_ = this->get_sampler_from_state(DEFAULT_SAMPLER_STATE);
  }
  return default_sampler_state_;
}
VkSampler VKContext::get_sampler_from_state(VKSamplerState sampler_state)
{
  BLI_assert((uint)sampler_state >= 0 && ((uint)sampler_state) < GPU_SAMPLER_MAX);
  return sampler_state_cache_[(uint)sampler_state];
}
VKTexture *VKContext::get_dummy_texture(eGPUTextureType type)
{
  /* Decrement 1 from texture type as they start from 1 and go to 32 (inclusive). Remap to 0..31
   */
  gpu::VKTexture *dummy_tex = dummy_textures_[type - 1];
  if (dummy_tex != nullptr) {
    return dummy_tex;
  }
  else {
    GPUTexture *tex = nullptr;
    switch (type) {
      case GPU_TEXTURE_1D:
        tex = GPU_texture_create_1d("Dummy 1D", 128, 1, GPU_RGBA8, nullptr);
        break;
      case GPU_TEXTURE_1D_ARRAY:
        tex = GPU_texture_create_1d_array("Dummy 1DArray", 128, 1, 1, GPU_RGBA8, nullptr);
        break;
      case GPU_TEXTURE_2D:
        tex = GPU_texture_create_2d("Dummy 2D", 128, 128, 1, GPU_RGBA8, nullptr);
        break;
      case GPU_TEXTURE_2D_ARRAY:
        tex = GPU_texture_create_2d_array("Dummy 2DArray", 128, 128, 1, 1, GPU_RGBA8, nullptr);
        break;
      case GPU_TEXTURE_3D:
        tex = GPU_texture_create_3d(
            "Dummy 3D", 128, 128, 1, 1, GPU_RGBA8, GPU_DATA_UBYTE, nullptr);
        break;
      case GPU_TEXTURE_CUBE:
        tex = GPU_texture_create_cube("Dummy Cube", 128, 1, GPU_RGBA8, nullptr);
        break;
      case GPU_TEXTURE_CUBE_ARRAY:
        tex = GPU_texture_create_cube_array("Dummy CubeArray", 128, 1, 1, GPU_RGBA8, nullptr);
        break;
      case GPU_TEXTURE_BUFFER:
        if (!dummy_verts_) {
          GPU_vertformat_clear(&dummy_vertformat_);
          GPU_vertformat_attr_add(&dummy_vertformat_, "dummy", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
          dummy_verts_ = GPU_vertbuf_create_with_format_ex(&dummy_vertformat_, GPU_USAGE_STATIC);
          GPU_vertbuf_data_alloc(dummy_verts_, 64);
        }
        tex = GPU_texture_create_from_vertbuf("Dummy TextureBuffer", dummy_verts_);
        break;
      default:
        BLI_assert_msg(false, "Unrecognised texture type");
        return nullptr;
    }
    gpu::VKTexture *vk_tex = static_cast<gpu::VKTexture *>(reinterpret_cast<Texture *>(tex));
    dummy_textures_[type - 1] = vk_tex;
    return vk_tex;
  }
  return nullptr;
}
VkSampler VKContext::generate_sampler_from_state(VKSamplerState sampler_state)
{

  /* Check if sampler already exists for given state. */
  VkSamplerCreateInfo samplerCI = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

  VkSamplerAddressMode clamp_type = (sampler_state.state & GPU_SAMPLER_CLAMP_BORDER) ?
                                        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER :
                                        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  VkSamplerAddressMode repeat_type = (sampler_state.state & GPU_SAMPLER_MIRROR_REPEAT) ?
                                         VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT :
                                         VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerCI.addressModeU = (sampler_state.state & GPU_SAMPLER_REPEAT_R) ? repeat_type : clamp_type;
  samplerCI.addressModeV = (sampler_state.state & GPU_SAMPLER_REPEAT_S) ? repeat_type : clamp_type;
  samplerCI.addressModeW = (sampler_state.state & GPU_SAMPLER_REPEAT_T) ? repeat_type : clamp_type;
  samplerCI.borderColor =
      VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;  // VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;  //

  samplerCI.unnormalizedCoordinates = VK_FALSE;  /// descriptor.normalizedCoordinates = true;

  samplerCI.minFilter = (sampler_state.state & GPU_SAMPLER_FILTER) ? VK_FILTER_LINEAR :
                                                                     VK_FILTER_NEAREST;

  samplerCI.magFilter = (sampler_state.state & GPU_SAMPLER_FILTER) ? VK_FILTER_LINEAR :
                                                                     VK_FILTER_NEAREST;

  samplerCI.mipmapMode = (sampler_state.state & GPU_SAMPLER_MIPMAP) ?
                             VK_SAMPLER_MIPMAP_MODE_LINEAR :
                             VK_SAMPLER_MIPMAP_MODE_NEAREST;

  samplerCI.mipLodBias = 0.0f;

  samplerCI.minLod = -1000;
  samplerCI.maxLod = 1000;

  float aniso_filter = max_ff(16, U.anisotropic_filter);
  samplerCI.maxAnisotropy = (sampler_state.state & GPU_SAMPLER_MIPMAP) ? aniso_filter : 1;
  samplerCI.compareEnable = (sampler_state.state & GPU_SAMPLER_COMPARE) ? VK_TRUE : VK_FALSE;
  samplerCI.compareOp = (sampler_state.state & GPU_SAMPLER_COMPARE) ? VK_COMPARE_OP_EQUAL :
                                                                      VK_COMPARE_OP_ALWAYS;

  /// VKSamplerState  state = [this->device newSamplerStateWithDescriptor:descriptor];
  /// sampler_state_cache_[(uint)sampler_state] = state;
  ///
  ///

  /* Custom sampler for icons. */
  if (sampler_state.state == GPU_SAMPLER_ICON) {
    samplerCI.mipLodBias = -0.5f;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  }

  VK_CHECK2(
      vkCreateSampler(device_, &samplerCI, nullptr, &sampler_state_cache_[(uint)sampler_state]));
  return sampler_state_cache_[(uint)sampler_state];
}

VkFormat VKContext::getImageFormat()
{
  GHOST_ContextVK *gcontext = (GHOST_ContextVK *)ghost_context_;
  return gcontext->getImageFormat();
}

VkFormat VKContext::getDepthFormat()
{
  GHOST_ContextVK *gcontext = (GHOST_ContextVK *)ghost_context_;
  return gcontext->getDepthFormat();
};

}  // namespace blender::gpu

/* -------------------------------------------------------------------- */
/** \name Get & Set
 * \{ */

namespace blender::gpu{

  void VKContext::get_frame_buffer(VKFrameBuffer *&fb_)
  {
    fb_ = frame_buffers_[current_frame_index_];
  };



  /*NOTE: Functions for getting members of #GHOST_ContextVK. */

  uint32_t VKContext::get_graphicQueueIndex()
  {
    BLI_assert(device_ != VK_NULL_HANDLE);
    return graphic_queue_familly_;
  };

  uint32_t VKContext::get_current_image_index()
  {
    BLI_assert(current_img_index_ >= 0);
    return (uint32_t)current_img_index_;
  };

  VkImage VKContext::get_current_image()
  {
    BLI_assert(current_swapchain_img_ != VK_NULL_HANDLE);
    return current_swapchain_img_;
  }

  VkImageLayout VKContext::get_current_image_layout()
  {
    return vk_sw_layouts[current_img_index_];
  };

  void VKContext::set_current_image_layout(VkImageLayout layout)
  {
    vk_sw_layouts[current_img_index_] = layout;
  };

}  // namespace blender::gpu

namespace blender::gpu {


  VKSubmitter::VKSubmitter(VKContext *ctx) : ctx_(ctx)
  {

    for (int i = 0; i < sema_stock_nums; i++) {
        sema_stock_[i] = VK_NULL_HANDLE;
    };
  }

  VKSubmitter::~VKSubmitter()
  {
    destroySemaphore();
  }

  bool VKSubmitter::is_inside_frame()
  {
    return is_inside_;
  }
  GHOST_TSuccess VKSubmitter::SubmitVolatileFence(VkDevice device,
                                     VkQueue queue,
                                     VkSubmitInfo &submit_info,
                                     bool nofence)
  {

    VkFence fence = VK_NULL_HANDLE;
    if (!nofence) {
      VkFenceCreateInfo fence_info = {};
      fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
      VK_CHECK(vkCreateFence(device, &fence_info, NULL, &fence));
      // object_vk_label(device, fence, "Volatile_Fence");
      vkResetFences(device, 1, &fence);
    }

    VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, fence));
    VK_CHECK(vkQueueWaitIdle(queue));

    if (!nofence) {
      VkResult result;
      do {
        result = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
      } while (result == VK_TIMEOUT);
      BLI_assert(result == VK_SUCCESS);
      vkDestroyFence(device, fence, NULL);
    }

    return GHOST_kSuccess;
  }

  void VKSubmitter::clear(){
    onetime_commands_.clear();
    wait_sema_sw_ = signal_sema_sw_ = fin_sema_sw_ = VK_NULL_HANDLE;
    num_submit_ = num_submit_bl_ = 0;
  };
  VkImageLayout VKSubmitter::init_image_layout()
  {

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkImageLayout layout = ctx_->get_current_image_layout();
    if (layout != VK_IMAGE_LAYOUT_UNDEFINED) {
      return layout;
    }

    begin_submit_simple(cmd, true);
    GHOST_ImageTransition(
        cmd,
        ctx_->current_swapchain_img_,
        ctx_->current_swapchain_img_format_,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED);
    ctx_->set_current_image_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    end_submit_simple();

    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  };
  VkImageLayout VKSubmitter::fail_image_layout()
  {

    VkImageLayout layout = ctx_->get_current_image_layout();
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
      BLI_assert(layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    begin_submit_simple(cmd, true);

    GHOST_ImageTransition(
        cmd,
        ctx_->current_swapchain_img_,
        ctx_->current_swapchain_img_format_,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    end_submit_simple();
    ctx_->set_current_image_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  };
  VkImageLayout VKSubmitter::finalize_image_layout()
  {

    VkImageLayout layout = ctx_->get_current_image_layout();
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
      return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
    if (layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
      BLI_assert_unreachable();
      return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }

    begin_submit_simple(cmd, true);

    GHOST_ImageTransition(
        cmd,
        ctx_->current_swapchain_img_,
        ctx_->current_swapchain_img_format_,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    end_submit_simple();
    ctx_->set_current_image_layout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  };

  GHOST_TSuccess VKSubmitter::initialize_sw_submit()
  {
    if (wait_sema_sw_ == VK_NULL_HANDLE) {
      ctx_->begin_frame();
      /*  return GHOST_kSuccess; */
    }
    onetime_commands_.clear();
    if (sema_stock_[0] == VK_NULL_HANDLE) {
      createSemaphore();
    }
    signal_sema_sw_ = sema_stock_[0];

    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &wait_sema_sw_;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 0;
    submit_info.pCommandBuffers = nullptr;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &signal_sema_sw_;
    SubmitVolatileFence(ctx_->device_get(), ctx_->queue_get(), submit_info);

    wait_sema_sw_ = signal_sema_sw_;
    signal_sema_sw_ = sema_stock_[1];
    is_init_sw_ = true;

    return GHOST_kSuccess;
  }
  GHOST_TSuccess VKSubmitter::finalize_sw_submit()
  {

    if (!is_init_sw_) {
      return GHOST_kSuccess;
    }

    VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &wait_sema_sw_;
    submit_info.pWaitDstStageMask = &wait_stages;
    submit_info.commandBufferCount = 0;
    submit_info.pCommandBuffers = nullptr;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &fin_sema_sw_;
    SubmitVolatileFence(ctx_->device_get(), ctx_->queue_get(), submit_info);

    onetime_commands_.clear();
    pipeline_sema_idx_ = 0;
    wait_sema_sw_ = signal_sema_sw_ = fin_sema_sw_= VK_NULL_HANDLE;

    is_init_sw_ = false;
    is_final_sw_ = true;

    return GHOST_kSuccess;
  }
  void VKSubmitter::begin_submit_simple(VkCommandBuffer &cmd, bool ofscreen)
  {

    /*todo::thread safe on cpu.*/
    if (( !ofscreen )&& (num_submit_ == 0) && (ctx_->is_swapchain_ )) {
      initialize_sw_submit();
    }

    cmd_ = cmd = ctx_->request_command_buffer();
    VkCommandBufferBeginInfo cmdBufInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkResetCommandBuffer(cmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBufInfo));

    return;
  };
  void VKSubmitter::end_submit_simple()
  {

    VK_CHECK(vkEndCommandBuffer(cmd_));

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = nullptr;
    submit_info.pWaitDstStageMask = nullptr;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_;
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores = nullptr;
    SubmitVolatileFence(ctx_->device_get(), ctx_->queue_get(), submit_info);
    return;
  };

  GHOST_TSuccess VKSubmitter::begin_blit_submit(VkCommandBuffer &cmd)
  {
    /*todo::thread safe on cpu.*/
    if (num_submit_ == 0) {
      initialize_sw_submit();
    }
    VkCommandBufferResetFlags flag = VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT;
    vkResetCommandBuffer(cmd, flag);
    return GHOST_kSuccess;
  };

  GHOST_TSuccess VKSubmitter::end_blit_submit(VkCommandBuffer &cmd,
                                              std::vector<VkSemaphore> batch_signal)
  {

    std::vector<VkPipelineStageFlags> wait_stages;

    for (int i = 0; i < batch_signal.size(); i++) {
      wait_stages.push_back(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    };

    wait_stages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    batch_signal.push_back(wait_sema_sw_);

    VkSubmitInfo submit_info = {};

    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = batch_signal.size();
    submit_info.pWaitSemaphores = batch_signal.data();
    submit_info.pWaitDstStageMask = wait_stages.data();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &signal_sema_sw_;
    SubmitVolatileFence(ctx_->device_get(), ctx_->queue_get(), submit_info);

    toggle_sema(wait_sema_sw_, signal_sema_sw_);

    num_submit_bl_++;
    num_submit_++;

    return GHOST_kSuccess;
  };

  int VKSubmitter::begin_onetime_submit(VkCommandBuffer cmd)
  {
    /*todo::thread safe on cpu.*/
    if (num_submit_ == 0) {
      initialize_sw_submit();
      num_submit_++;
    }

    pipeline_sema_idx_++;
    onetime_commands_.push_back(cmd);
    BLI_assert(onetime_commands_.size() == pipeline_sema_idx_);
    VkCommandBufferResetFlags flag = VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT;
    vkResetCommandBuffer(cmd, flag);

    return pipeline_sema_idx_ - 1;
  }
  void VKSubmitter::toggle_sema(VkSemaphore &s1, VkSemaphore &s2)
  {
    VkSemaphore &tmp = s1;
    s1 = s2;
    s2 = tmp;
  };
  GHOST_TSuccess VKSubmitter::end_onetime_submit(int registerID)
  {

    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &wait_sema_sw_;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &onetime_commands_[registerID];

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.flags = 0;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &signal_sema_sw_;

    SubmitVolatileFence(ctx_->device_get(), ctx_->queue_get(), submit_info);

    ctx_->set_current_image_layout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    toggle_sema(wait_sema_sw_, signal_sema_sw_);

    return GHOST_kSuccess;
  }
  GHOST_TSuccess VKSubmitter::begin_offscreen_submit(VkCommandBuffer &cmd)
  {

    VkCommandBufferResetFlags flag = VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT;
    vkResetCommandBuffer(cmd, flag);
    return GHOST_kSuccess;
  };

  GHOST_TSuccess VKSubmitter::end_offscreen_submit(VkCommandBuffer &cmd,
                                                   VkSemaphore wait,
                                                   VkSemaphore signal)
  {

    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    if (wait == VK_NULL_HANDLE) {
      submit_info.waitSemaphoreCount = 0;
      submit_info.pWaitSemaphores = nullptr;
      submit_info.pWaitDstStageMask = nullptr;
    }
    else {
      submit_info.waitSemaphoreCount = 1;
      submit_info.pWaitSemaphores = &wait;
      submit_info.pWaitDstStageMask = wait_stages;
    }

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.flags = 0;

    BLI_assert(signal != VK_NULL_HANDLE);
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &signal;

    SubmitVolatileFence(ctx_->device_get(), ctx_->queue_get(), submit_info);

    return GHOST_kSuccess;
  }
  void VKSubmitter::destroySemaphore()
  {
    for (int i = 0; i < sema_stock_nums; i++) {
      if (sema_stock_[i] != VK_NULL_HANDLE) {
        vkDestroySemaphore(ctx_->device_get(), sema_stock_[i], NULL);
        sema_stock_[i] = VK_NULL_HANDLE;
      }
    };
  }
  void VKSubmitter::createSemaphore()
  {
    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.flags = 0;

    auto device = ctx_->device_get();
    destroySemaphore();
    for (int i = 0; i < sema_stock_nums; i++) {
      VK_CHECK(vkCreateSemaphore(device, &semaphore_info, NULL, &sema_stock_[i]));
      debug::object_vk_label(
          device, sema_stock_[i], std::string("Semaphore_SW_") + std::to_string(i));
    }
  }
  }
/** \} */

