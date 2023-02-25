/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "BLI_assert.h"
#include "BLI_utildefines.h"
#include "BKE_global.h"
#include "vk_debug.hh"
#include "vk_context.hh"
#include "vk_framebuffer.hh"
#include "vk_backend.hh"
#include "vk_state.hh"
#include "vk_immediate.hh"
#include "vk_vertex_buffer.hh"

#include "GHOST_C-api.h"
#include "GPU_common_types.h"
#include "GPU_capabilities.h"
#include "GPU_context.h"
#include "gpu_context_private.hh"


#include "intern/GHOST_Context.h"
#include "intern/GHOST_ContextVK.h"
#include "intern/GHOST_Window.h"


namespace blender::gpu {



  void VKContext::destroyMemAllocator(){

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
  uint32_t    VKContext::max_cubemap_size = 0;
  uint32_t    VKContext::max_ubo_size = 0;
  uint32_t   VKContext::max_ubo_binds = 0;
  uint32_t   VKContext::max_ssbo_size = 0;
  uint32_t   VKContext::max_ssbo_binds = 0;
  uint32_t   VKContext::max_push_constants_size = 0;
  uint32_t   VKContext::max_inline_ubo_size = 0;
  bool   VKContext::multi_draw_indirect_support  = 0;
  bool          VKContext::vertex_attrib_binding_support = false;

VKContext::VKContext(void *ghost_window,
                     void *ghost_context,
                     VKSharedOrphanLists &shared_orphan_list)
    : shared_orphan_list_(shared_orphan_list)
{




  for (int i = 0; i < GPU_SAMPLER_MAX; i++) 
    sampler_state_cache_[i] = VK_NULL_HANDLE;
  init(ghost_window, ghost_context);
  buffer_manager_ = new VKStagingBufferManager(*this);

  auto ctx_ = GPU_context_active_get();

  GPU_context_active_set((GPUContext*)this);


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
  GPU_VERTBUF_DISCARD_SAFE( vbo);

#define DELE(a) \
  if (a) { \
    delete a; \
    a = nullptr; \
  }



  for (VKFrameBuffer* fb_ : frame_buffers_) {
    DELE(fb_);
  }


  DELE(state_manager);
  DELE(imm);
  DELE(this->back_left);
  DELE(this->front_left);
  DELE(buffer_manager_);
#undef DELE

  for (int i = 0; i < GPU_SAMPLER_MAX; i++) {
    if (sampler_state_cache_[i] != VK_NULL_HANDLE)
      vkDestroySampler(device_, sampler_state_cache_[i], nullptr);
  };
  vmaDestroyAllocator(mem_allocator_);
  mem_allocator_ = VK_NULL_HANDLE;
};

void VKContext::create_swapchain_fb() {
  if (this->front_left) {
    auto fb = static_cast<VKFrameBuffer*> (this->front_left);
      fb->create_swapchain_frame_buffer(0);


}
  if (this->back_left) {
    auto fb = static_cast<VKFrameBuffer*> (this->back_left);
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
    GHOST_Window *ghostWin = reinterpret_cast<GHOST_Window *>(ghost_window_);
    ghost_context = (ghostWin ? ghostWin->getContext() : NULL);
  }
  BLI_assert(ghost_context);
  this->ghost_context_ = static_cast<GHOST_ContextVK*>(ghost_context);
  GHOST_ContextVK *gcontext = (GHOST_ContextVK *)ghost_context;




  gcontext->initializeDrawingContext();

  gcontext->getVulkanHandles(
      //  GHOST_GetVulkanHandles((GHOST_ContextHandle)ghost_context,
      &instance_,
      &physical_device_,
      &device_,
      &graphic_queue_familly_);

  /* Initialize the memory allocator. */
  //VmaAllocatorCreateInfo info = {};
  /* Should use same vulkan version as GHOST. */
  if (mem_allocator_ == VK_NULL_HANDLE) {

    mem_allocator_info.vulkanApiVersion = VK_API_VERSION_1_2;
    mem_allocator_info.physicalDevice = physical_device_;
    mem_allocator_info.device = device_;
    mem_allocator_info.instance = instance_;
    vmaCreateAllocator(&mem_allocator_info, &mem_allocator_);
    gcontext->destroyer = [&]() {

    };
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
    for (auto name : { "front_left","back_left" }) {
      VKFrameBuffer* vk_fb = new VKFrameBuffer(name, this);
      vk_fb->create_swapchain_frame_buffer(i++);
      if (i == 1) {
        this->front_left = vk_fb;
      }
      else {
        this->back_left = vk_fb;
      }
    }

    auto func = [&]() {
      this->create_swapchain_fb();
    };
    auto f = std::function<void(void)>(func);


    gcontext->set_fb_cb(f);

    /*
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
     
          */
  }
  else {
    /* For off-screen contexts. Default frame-buffer is null. */
    /* back_left = new VKFrameBuffer("back_left", this, GL_NONE, 0, 0, 0); */
    this->back_left = new VKFrameBuffer( "back_left",this);
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
  static_cast<VKStateManager*>(state_manager)->active_fb = static_cast<VKFrameBuffer*>( active_fb);


  if (is_swapchain_) {
    auto func_sb= [&]() {
      GHOST_ContextVK *gcontext = (GHOST_ContextVK *)this->ghost_context_;
      if (gcontext ->is_inside_frame()) {
      static_cast<VKFrameBuffer*>(this->active_fb)->render_end();
      
      gcontext->finalize_image_layout();
      end_frame();
    }


    };
    auto f_sb = std::function<void(void)>(func_sb);
    gcontext->set_fb_sb_cb(f_sb);

    auto func_sb2 = [&]() {
   
      GHOST_ContextVK* gcontext = (GHOST_ContextVK*)this->ghost_context_;
      float r = (float)gcontext->getCurrentImage();
      auto fb = static_cast<VKFrameBuffer*>(this->active_fb);
      float color[4] = { r,1.,0.,1. };
    
      VkClearValue clearValues[2];
      for (int i = 0; i < 4; i++)clearValues[0].color.float32[i] = color[i];
      clearValues[1].depthStencil.depth = 1.f;
      clearValues[1].depthStencil.stencil = 0;
      fb->render_begin(VK_NULL_HANDLE, VK_COMMAND_BUFFER_LEVEL_PRIMARY, (VkClearValue*)clearValues);
    
      //
      //gcontext->clear_color(cmd, (VkClearColorValue*)& clearValues[0].color.float32[0]);

  
      fb->render_end();



    };

    auto f_sb2 = std::function<void(void)>(func_sb2);
    gcontext->set_fb_sb2_cb(f_sb2);


    auto ctx_ = GPU_context_active_get();

    GPU_context_active_set((GPUContext *)this);

    clear_draw_test(gcontext);

    GPU_context_active_set(ctx_);
  }

  


};
void  VKContext::clear_color(VkCommandBuffer cmd, const VkClearColorValue* clearValues) {
  auto gcontext_ = static_cast<GHOST_ContextVK*>(ghost_context_);
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
    ///glDeleteBuffers(uint(buffers.size()), buffers.data());
    buffers.clear();
  }
  if (!textures.is_empty()) {
    ///glDeleteTextures(uint(textures.size()), textures.data());
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
    ///glDeleteVertexArrays(uint(orphaned_vertarrays_.size()), orphaned_vertarrays_.data());
    orphaned_vertarrays_.clear();
  }
  if (!orphaned_framebuffers_.is_empty()) {
    ///glDeleteFramebuffers(uint(orphaned_framebuffers_.size()), orphaned_framebuffers_.data());
    orphaned_framebuffers_.clear();
  }
  lists_mutex_.unlock();

  shared_orphan_list_.orphans_clear();
};





 void VKContext::fbo_free(VkFramebuffer fbo_id)
{
  if (this == VKContext::get()) {
    BLI_assert(false);
    ///glDeleteFramebuffers(1, &fbo_id);
  }
  else {
    orphans_add(orphaned_framebuffers_, lists_mutex_, fbo_id);
  }
}
 void VKContext::buf_free(VKBuffer* buf) {

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
     VKSharedOrphanLists& orphan_list = static_cast<VKBackend*>(VKBackend::get())->shared_orphan_list_get();
     orphans_add(orphan_list.buffers, orphan_list.lists_mutex, buf);
   }

 };

 void VKContext::vao_free(VKVAOty buf) {

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


void  VKContext::getImageView(VkImageView& view, int i) {
  
  auto context = ((GHOST_ContextVK*)ghost_context_);
  context->getImageView(view, i);

};

int  VKContext::getImageViewNums() {
  auto context = ((GHOST_ContextVK*)ghost_context_);
  return context->getImageCount();

};

void  VKContext::getRenderExtent(VkExtent2D& _render_extent) {
  auto context = ((GHOST_ContextVK*)ghost_context_);
  context->getRenderExtent(_render_extent);
};

void VKContext::bottom_transition() {
  auto context = ((GHOST_ContextVK*)ghost_context_);
  context->finalize_image_layout();
};

void VKContext::fail_transition() {
  auto context = ((GHOST_ContextVK*)ghost_context_);
  context->fail_image_layout();
};

void VKContext::begin_frame()
{

  auto context = ((GHOST_ContextVK*)ghost_context_);
  if (context->is_inside_frame()) {
    return;
  }
   
  if (is_swapchain_ ) {
    context->acquireCustom();
    debug::pushMarker( queue_get(graphic_queue_familly_) ,"AcquireSwapImage");
    nums_submit_ = 0;
  }

};

uint32_t VKContext::get_current_frame_index() {
  auto context = ((GHOST_ContextVK*)ghost_context_);
  return (uint32_t)context->getFrame();
};
void VKContext::get_frame_buffer(VKFrameBuffer*& fb_) {

  auto context = ((GHOST_ContextVK*)ghost_context_);
  int i = context->getFrame();
  fb_ = frame_buffers_[i];

};

void VKContext::get_command_buffer(VkCommandBuffer& cmd) {

  auto context = ((GHOST_ContextVK*)ghost_context_);
  context->getCrrentCommandBuffer(cmd);

};

void VKContext::end_frame()
{
  auto context = ((GHOST_ContextVK *)ghost_context_);

  if (context->is_inside_frame()) {
    if (is_swapchain_) {
      static_cast<VKFrameBuffer *>(this->active_fb)->render_end();
      context->finalize_sw_submit();

      debug::popMarker( queue_get(graphic_queue_familly_));
      if (context->presentCustom() == GHOST_kFailure) {
        /*  recreate swapbuffer  */
      };
    }
  }

}
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
void VKContext::begin_submit_simple(VkCommandBuffer &cmd, bool ofscreen)
{
  auto context = ((GHOST_ContextVK *)ghost_context_);
  context->begin_submit_simple(cmd, ofscreen);
  debug::pushMarker(cmd, "SimpleSubmit");
  current_cmd_ = cmd;
};
void VKContext::end_submit_simple()
{
  auto context = ((GHOST_ContextVK *)ghost_context_);
  debug::popMarker(current_cmd_);
  context->end_submit_simple();
  
};


int VKContext::begin_onetime_submit(VkCommandBuffer cmd) {
  auto context = ((GHOST_ContextVK*)ghost_context_);


  return context->begin_onetime_submit(cmd);

};
void  VKContext::end_onetime_submit(int i ) {
  auto context = ((GHOST_ContextVK*)ghost_context_);
  context->end_onetime_submit(i);
  is_onetime_commit_ = false;
 
}
bool VKContext::begin_blit_submit(VkCommandBuffer &cmd)
{
  auto context = ((GHOST_ContextVK *)ghost_context_);
  return (bool)context->begin_blit_submit(cmd);
};
bool VKContext::end_blit_submit(VkCommandBuffer &cmd, std::vector<VkSemaphore> batch_signal)
{
  auto context = ((GHOST_ContextVK *)ghost_context_);
  return (bool)context->end_blit_submit(cmd, batch_signal);
};



int    VKContext::begin_offscreen_submit(VkCommandBuffer cmd) {
  auto context = ((GHOST_ContextVK*)ghost_context_);
  context->begin_offscreen_submit(cmd);
  return -1;
};
void  VKContext::end_offscreen_submit(VkCommandBuffer& cmd, VkSemaphore wait, VkSemaphore signal){
  auto context = ((GHOST_ContextVK*)ghost_context_);
  context->end_offscreen_submit(cmd,wait,signal);
};


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

uint32_t VKContext::get_current_image_index() {
  auto context = ((GHOST_ContextVK*)ghost_context_);
  return context->getCurrentImage();
};

  VkPhysicalDevice VKContext::get_physical_device(){
      auto ctx = (GHOST_ContextVK *)(ghost_context_);
    return ctx->getPhysicalDevice();
  };

  VkCommandBuffer  VKContext::request_command_buffer(bool second) {
    
    auto ghost = static_cast<GHOST_ContextVK*>(ghost_context_);
    VkCommandBuffer cmd = VK_NULL_HANDLE;;
    ghost->requestCommandBuffer(cmd,second);
    return cmd;

  };

  VkImage VKContext::get_current_image() {

    auto context = ((GHOST_ContextVK*)ghost_context_);
    VkImage image = VK_NULL_HANDLE;
    context->getImage(image);
    return image;
  };

  VkImageLayout VKContext::get_current_image_layout() {

    auto context = ((GHOST_ContextVK*)ghost_context_);

    return context->get_layout();

  };

  void VKContext::set_current_image_layout(VkImageLayout layout) {

    auto context = ((GHOST_ContextVK*)ghost_context_);
    context->set_layout(layout);
  };


  bool VKContext::is_support_format(VkFormat format, VkFormatFeatureFlagBits flag, bool linear) {
    /* Check if the device supports blitting to linear images */
    /* reference => https://github.com/SaschaWillems/Vulkan/blob/79d0c5e436623436b6297a8c81fb3ee8ff78d804/examples/screenshot/screenshot.cpp#L194 */
    VkFormatProperties formatProps = {};
    auto context = ((GHOST_ContextVK*)ghost_context_);
   
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

  auto& pipeline_state = ((VKStateManager*)this->state_manager)->getPipelineStateCI();
  auto &vp = pipeline_state.viewport_cache;
  bool changed = ((int)vp.x != origin_x) || ((int)vp.y != origin_y) || ((int)vp.width != width) ||
                 ((int)vp.height != height);

  vp = {(float)origin_x, (float)origin_y, (float)width, (float)height, 0.,1.};


  if (changed) {
    pipeline_state.dirty_flags = (VKPipelineStateDirtyFlag) ( (pipeline_state.dirty_flags |
                                        VK_PIPELINE_STATE_VIEWPORT_FLAG));

  }
}

void VKContext::set_scissor(int scissor_x, int scissor_y, int scissor_width, int scissor_height)
{
  BLI_assert(this);
  auto& pipeline_state = ((VKStateManager *)this->state_manager)->getPipelineStateCI();
  auto &sc = pipeline_state.scissor_cache;



 
  bool changed = (sc.offset.x != scissor_x) || (sc.offset.y != scissor_y) ||
                 (sc.extent.width != scissor_width) || (sc.extent.height != scissor_height);

  sc = {{scissor_x, scissor_y}, {(uint)scissor_width, (uint)scissor_height}};

   ///TODO dynamic state scissor test
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

  uint32_t VKContext::get_graphicQueueIndex()
{
  return ((GHOST_ContextVK *)ghost_context_)->getQueueIndex(0);
}

uint32_t VKContext::get_transferQueueIndex()
{
  return ((GHOST_ContextVK *)ghost_context_)->getQueueIndex(2);
}


VkPipelineCache VKContext::get_pipeline_cache(){

     return ((GHOST_ContextVK *)ghost_context_)->getPipelineCache();
};

  VkQueue VKContext::queue_get(uint32_t type_)
{
  uint32_t r_graphic_queue_familly = 0;
  VkQueue queue;
   ((GHOST_ContextVK*)ghost_context_)->getQueue(0, &queue, &r_graphic_queue_familly);
  return queue;
}
VkRenderPass VKContext::get_renderpass()
{
  return ((GHOST_ContextVK *)ghost_context_)->getRenderPass();
};
  VkSampler VKContext::get_default_sampler_state()
{
  if (default_sampler_state_== VK_NULL_HANDLE){
    default_sampler_state_ = this->get_sampler_from_state(DEFAULT_SAMPLER_STATE);
  }
  return default_sampler_state_;
}

VkSampler VKContext::get_sampler_from_state(VKSamplerState sampler_state)
{
  BLI_assert((uint)sampler_state >= 0 && ((uint)sampler_state) < GPU_SAMPLER_MAX);
  return sampler_state_cache_[(uint)sampler_state];
}


VKTexture* VKContext::get_dummy_texture(eGPUTextureType type)
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
    samplerCI.addressModeU = (sampler_state.state & GPU_SAMPLER_REPEAT_R) ? repeat_type :
                                 clamp_type;
    samplerCI.addressModeV = (sampler_state.state & GPU_SAMPLER_REPEAT_S) ? repeat_type :
                                 clamp_type;
    samplerCI.addressModeW = (sampler_state.state & GPU_SAMPLER_REPEAT_T) ? repeat_type :
                                 clamp_type;
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;//VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;  //

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



VkFormat  VKContext::getImageFormat() 
{
  GHOST_ContextVK* gcontext = (GHOST_ContextVK*)ghost_context_;
  return  gcontext->getImageFormat();
}

VkFormat VKContext::getDepthFormat()
{
  GHOST_ContextVK* gcontext = (GHOST_ContextVK*)ghost_context_;
  return  gcontext->getDepthFormat();
};

}  // namespace blender::gpu
