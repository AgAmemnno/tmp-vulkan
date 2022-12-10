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

#include "GHOST_C-api.h"
#include "GPU_common_types.h"
#include "GPU_capabilities.h"
#include "GPU_context.h"
#include "gpu_context_private.hh"


#include "intern/GHOST_Context.h"
#include "intern/GHOST_ContextVK.h"
#include "intern/GHOST_Window.h"


namespace blender::gpu {
/* Global memory manager. */


    VKContext::VKContext(void *ghost_window,
                     void *ghost_context,
                     VKSharedOrphanLists &shared_orphan_list)
    : shared_orphan_list_(shared_orphan_list)
{

  for (int i = 0; i < GPU_SAMPLER_MAX; i++) 
    sampler_state_cache_[i] = VK_NULL_HANDLE;
  init(ghost_window, ghost_context);

   buffer_manager_ = new VKStagingBufferManager(*this);
};

VKContext::~VKContext()
{
#define DELE(a) \
  if (a) { \
    delete a; \
    a = nullptr; \
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

  GHOST_ContextVK *gcontext = (GHOST_ContextVK *)ghost_context;

  gcontext->getVulkanHandles(
      //  GHOST_GetVulkanHandles((GHOST_ContextHandle)ghost_context,
      &instance_,
      &physical_device_,
      &device_,
      &graphic_queue_familly_);

  /* Initialize the memory allocator. */
  VmaAllocatorCreateInfo info = {};
  /* Should use same vulkan version as GHOST. */
  info.vulkanApiVersion = VK_API_VERSION_1_2;
  info.physicalDevice = physical_device_;
  info.device = device_;
  info.instance = instance_;
  vmaCreateAllocator(&info, &mem_allocator_);

  if (ghost_window) {

    GHOST_RectangleHandle bounds = GHOST_GetClientBounds((GHOST_WindowHandle)ghost_window);
   // int w = GHOST_GetWidthRectangle(bounds);
   // int h = GHOST_GetHeightRectangle(bounds);
    GHOST_DisposeRectangle(bounds);

    /* Create FrameBuffer handles. */
    VKFrameBuffer *vk_front_left = new VKFrameBuffer(this, "front_left");
    VKFrameBuffer *vk_back_left = new VKFrameBuffer(this, "back_left");
    this->front_left = vk_front_left;
    this->back_left = vk_back_left;

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
    ///back_left = new VKFrameBuffer("back_left", this, GL_NONE, 0, 0, 0);
    this->back_left = new VKFrameBuffer(this, "back_left");
  }


  this->ghost_context_ = static_cast<GHOST_ContextVK *>(ghost_context);



  /* Initialize Render-pass and Frame-buffer State. */

  /* Initialize command buffer state. */
  // this->main_command_buffer.prepare();
  /* Initialize IMM and pipeline state */
  // this->pipeline_state.initialised = false;

  VKBackend::platform_init(this);
  VKBackend::capabilities_init(this);

  state_manager = new VKStateManager(this);
  imm = new VKImmediate(this);
  
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



void VKContext::orphans_add(Vector<GLuint> &orphan_list, std::mutex &list_mutex, GLuint id)
{
  list_mutex.lock();
  orphan_list.append(id);
  list_mutex.unlock();
}

 void VKContext::fbo_free(GLuint fbo_id)
{
  if (this == VKContext::get()) {
    ///TODO Delete frambuffer
    ///glDeleteFramebuffers(1, &fbo_id);
  }
  else {
    orphans_add(orphaned_framebuffers_, lists_mutex_, fbo_id);
  }
}


void VKContext::activate()
{
}

void VKContext::deactivate()
{
}

void VKContext::begin_frame()
{
  auto context = ((GHOST_ContextVK *)ghost_context_);
  context->acquireCustom();

 ///context->begin_frame(current_cmd_);
}
void VKContext::end_frame()
{
  auto context = ((GHOST_ContextVK *)ghost_context_);

  if (context->presentCustom() == GHOST_kFailure) {
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
  context->end_frame(cmd);
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


  VkPhysicalDevice VKContext::get_physical_device(){
      auto ctx = (GHOST_ContextVK *)(ghost_context_);
    return ctx->getPhysicalDevice();
  };





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
    samplerCI.addressModeU = (sampler_state.state & GPU_SAMPLER_REPEAT_R) ?
                                 VK_SAMPLER_ADDRESS_MODE_REPEAT :
                                 clamp_type;
    samplerCI.addressModeV = (sampler_state.state & GPU_SAMPLER_REPEAT_S) ?
                                 VK_SAMPLER_ADDRESS_MODE_REPEAT :
                                 clamp_type;
    samplerCI.addressModeW = (sampler_state.state & GPU_SAMPLER_REPEAT_T) ?
                                 VK_SAMPLER_ADDRESS_MODE_REPEAT :
                                 clamp_type;
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

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
    samplerCI.compareOp = (sampler_state.state & GPU_SAMPLER_COMPARE) ?
                              VK_COMPARE_OP_LESS_OR_EQUAL :
                              VK_COMPARE_OP_ALWAYS;

    /// VKSamplerState  state = [this->device newSamplerStateWithDescriptor:descriptor];
    /// sampler_state_cache_[(uint)sampler_state] = state;
    ///
    ///
    
    VK_CHECK2( vkCreateSampler(device_, &samplerCI, nullptr, &sampler_state_cache_[(uint)sampler_state]));

   
    return sampler_state_cache_[(uint)sampler_state];
  }


}  // namespace blender::gpu
