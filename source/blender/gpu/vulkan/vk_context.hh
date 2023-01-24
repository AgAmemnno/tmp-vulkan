/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once
///Capabilities
///
#define VK_MAX_TEXTURE_SLOTS 128
#define VK_MAX_SAMPLER_SLOTS VK_MAX_TEXTURE_SLOTS
/* Max limit without using bind-less for samplers. */
#define VK_MAX_DEFAULT_SAMPLERS 16
#define VK_MAX_UNIFORM_BUFFER_BINDINGS 31
#define VK_MAX_VERTEX_INPUT_ATTRIBUTES 31
#define VK_MAX_UNIFORMS_PER_BLOCK 64

#define VK_FRAME_AVERAGE_COUNT 5
#define VK_MAX_DRAWABLES 3
#define VK_MAX_SET_BYTES_SIZE 4096
#define VK_FORCE_WAIT_IDLE 0

#define VK_MAX_COMMAND_BUFFERS 64
#define VK_NUM_SAFE_FRAMES (VK_MAX_DRAWABLES + 1)

/* Display debug information about missing attributes and incorrect vertex formats. */
#define VK_DEBUG_SHADER_ATTRIBUTES 0

///Capabilities



#include "DNA_userdef_types.h"
#include "vk_mem_alloc.h"
#include "MEM_guardedalloc.h"
#include "gpu_context_private.hh"
#include "GPU_common_types.h"
#include "GPU_context.h"
#include "intern/GHOST_Context.h"
#include "vk_layout.hh"
#include "vk_memory.hh"
#ifdef __APPLE__
#  include <MoltenVK/vk_mvk_moltenvk.h>
#else
#  include <vulkan/vulkan.h>
#endif

#include "BLI_set.hh"


#include <mutex>

namespace blender::gpu {

static VkPrimitiveTopology to_vk(const GPUPrimType prim_type)
  {

    switch (prim_type) {
      case GPU_PRIM_POINTS:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
      case GPU_PRIM_LINES:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
      case GPU_PRIM_LINES_ADJ:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
      case GPU_PRIM_LINE_LOOP:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
      case GPU_PRIM_LINE_STRIP:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
      case GPU_PRIM_LINE_STRIP_ADJ:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
      case GPU_PRIM_TRIS:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      case GPU_PRIM_TRIS_ADJ:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
      case GPU_PRIM_TRI_FAN:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
      case GPU_PRIM_TRI_STRIP:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
      case GPU_PRIM_NONE:
        return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    };
    return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
  }

class VKSharedOrphanLists {
 public:
  /** Mutex for the below structures. */
  std::mutex lists_mutex;
  /** Buffers and textures are shared across context. Any context can free them. */
  Vector<GLuint> textures;
  Vector<VKBuffer*> buffers;

 public:
  void orphans_clear();
};




class VKFrameBuffer;
class VKCommandBufferManager;
class VKContext;
class VKShader;
class VKImmediate;
class VKShaderInterface;
class VKTexture;
class VKVaoCache;
/* Metal Context Render Pass State -- Used to track active RenderCommandEncoder state based on
 * bound MTLFrameBuffer's.Owned by MTLContext. */
class VKRenderPassState {
  friend class VKContext;

 public:
  VKRenderPassState(VKContext &context, VKCommandBufferManager &command_buffer_manager)
      : ctx(context), cmd(command_buffer_manager){

    for (int i = 0; i < VK_MAX_TEXTURE_SLOTS; i++) {
      cached_vertex_sampler_state_bindings[i].sampler_state = VK_NULL_HANDLE;
    }

  };
  ~VKRenderPassState()
  {


  };
  /* Given a RenderPassState is associated with a live RenderCommandEncoder,
   * this state sits within the MTLCommandBufferManager. */
  VKContext &ctx;
  VKCommandBufferManager &cmd;

  /* Caching of resource bindings for active MTLRenderCommandEncoder.
   * In Metal, resource bindings are local to the MTLCommandEncoder,
   * not globally to the whole pipeline/cmd buffer. */
  struct VKBoundShaderState {
    VKShader *shader_ = nullptr;
    uint pso_index_;
    void set(VKShader *shader, uint pso_index)
    {
      shader_ = shader;
      pso_index_ = pso_index;
    }
  };

  VKBoundShaderState last_bound_shader_state;
//  VkRenderPipelineStateCreateInfo  bound_pso = nil;
//  id<MTLDepthStencilState> bound_ds_state = nil;
  uint last_used_stencil_ref_value = 0;
  VkRect2D last_scissor_rect;

  /* Caching of CommandEncoder Vertex/Fragment buffer bindings. */
  struct BufferBindingCached {
    /* Whether the given binding slot uses byte data (Push Constant equivalent)
     * or an MTLBuffer. */
    bool is_bytes;
    VkBuffer metal_buffer;
    int offset;
  };

  VkDescriptorBufferInfo cached_vertex_buffer_bindings[VK_MAX_UNIFORM_BUFFER_BINDINGS];
  VkDescriptorBufferInfo cached_fragment_buffer_bindings[VK_MAX_UNIFORM_BUFFER_BINDINGS];




  struct SamplerStateBindingCached {
    //MTLSamplerState binding_state;
    //id<MTLSamplerState> sampler_state;
    VKSamplerState  binding_state;
    VkSampler  sampler_state;
    bool is_arg_buffer_binding;
  };

  SamplerStateBindingCached cached_vertex_sampler_state_bindings[VK_MAX_TEXTURE_SLOTS];
  SamplerStateBindingCached cached_fragment_sampler_state_bindings[VK_MAX_TEXTURE_SLOTS];

  Vector<VkWriteDescriptorSet> write_outs;


  void append_write_texture(VkWriteDescriptorSet &write);


    /* Sampler Binding (RenderCommandEncoder). */
  bool append_sampler(VKSamplerBinding &sampler_binding,
                      bool use_argument_buffer_for_samplers,
                      uint slot);

  /* Reset RenderCommandEncoder binding state. */
  void reset_state();

  /* Texture Binding (RenderCommandEncoder). */

  void bind_vertex_texture(VkImageView iinfo, uint32_t slot);
  void bind_fragment_texture(VkDescriptorImageInfo iinfo, uint32_t slot);




  void bind_fragment_sampler(SamplerStateBindingCached &sampler_binding,
                             bool use_argument_buffer_for_samplers,
                             uint slot);

  /* Buffer binding (RenderCommandEncoder). */

  void bind_vertex_buffer(VkDescriptorBufferInfo buffer,
                                              uint buffer_offset,
                                              uint index);

  void bind_fragment_buffer(VkBuffer buffer, uint buffer_offset, uint index);
  void bind_vertex_bytes(void *bytes, uint length, uint index);
  void bind_fragment_bytes(void *bytes, uint length, uint index);



};


class VKStateManager;
typedef VKBuffer     VKVAOty_impl;
typedef VKVAOty_impl*   VKVAOty;
typedef VKVAOty*   VecVKVAOty;

class VKContext : public Context {
 private:
  /** Copies of the handles owned by the GHOST context. */
  VkInstance instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  uint32_t graphic_queue_familly_ = 0;
  /** Allocator used for texture and buffers and other resources. */
  VmaAllocator mem_allocator_ = VK_NULL_HANDLE;


  /** Mutex for the below structures. */
  std::mutex lists_mutex_;
  /** VertexArrays and framebuffers are not shared across context. */
  Vector<VKBuffer*> orphaned_vertarrays_;
  Vector<VkFramebuffer> orphaned_framebuffers_;
  /** #GLBackend owns this data. */
  VKSharedOrphanLists &shared_orphan_list_;
  uint32_t current_frame_index_;

  bool is_inside_frame_ = false;
  bool is_initialized_      = false;

  VkSampler sampler_state_cache_[GPU_SAMPLER_MAX];
  VkSampler default_sampler_state_;



 public:

   /*  Capabilities. */
   static uint32_t    max_cubemap_size;
   static uint32_t    max_ubo_size;
   static  uint32_t   max_ubo_binds;
   static  uint32_t   max_ssbo_size;
   static  uint32_t   max_ssbo_binds;
   static uint32_t    max_push_constants_size;
   static uint32_t    max_inline_ubo_size;
   static bool vertex_attrib_binding_support;

  VkSampler get_default_sampler_state();
  VkSampler get_sampler_from_state(VKSamplerState sampler_state);
  VkSampler generate_sampler_from_state(VKSamplerState sampler_state);
  VkFormat  getImageFormat();
  VkFormat getDepthFormat();
  void getImageView(VkImageView& view, int i);
  int getImageViewNums();
  void getRenderExtent(VkExtent2D& _render_extent);
 VkRenderPass get_renderpass();
  VkPhysicalDevice get_physical_device();
  void *ghost_context_;


  VKStagingBufferManager*  buffer_manager_;
  PipelineStateCreateInfoVk       pipeline_state;
  VkCommandBuffer                     current_cmd_;
  Vector<VKFrameBuffer*>             frame_buffers_;
  bool is_swapchain_ = false;
  VKContext(void *ghost_window, void *ghost_context,VKSharedOrphanLists &shared_orphan_list);
  ~VKContext();
  void init(void *ghost_window, void *ghost_context);

  void clear_color(VkCommandBuffer cmd, const VkClearColorValue* clearValues);
  
  /* CommandBuffer managers.   */
  //
  void activate() override;
  void deactivate() override;
  void begin_frame() override;
  void end_frame() override;
  void begin_submit_simple(VkCommandBuffer& cmd);
  void end_submit_simple();

  void begin_submit(int N);
  void end_submit();
  void submit();
  void submit_nonblocking();

  void flush() override;
  void finish() override;

  void memory_statistics_get(int *total_mem, int *free_mem) override;

  void debug_group_begin(const char *, int) override;
  void debug_group_end() override;

  bool cmd_valid_[2] = {false, false};

  void begin_render_pass(VkCommandBuffer &cmd, int i = -1);
  void end_render_pass(VkCommandBuffer &cmd, int i = -1);
  bool is_onetime_commit_= false;
  int begin_onetime_submit(VkCommandBuffer cmd);
  void end_onetime_submit(int i);
  
  int   begin_offscreen_submit(VkCommandBuffer cmd);
  void end_offscreen_submit(VkCommandBuffer& cmd, VkSemaphore wait, VkSemaphore signal);

  void get_frame_buffer(VKFrameBuffer*& fb_);

  void get_command_buffer(VkCommandBuffer& cmd);

  VmaAllocator mem_allocator_get() const
  {
    return mem_allocator_;
  }
  VkDevice device_get() {
    return device_;
  };
  VkQueue queue_get(uint32_t type_);
  void fbo_free(VkFramebuffer fbo_id);
  void vao_free(VKVAOty buf);
  static   void buf_free(VKBuffer* buf);

  static VKContext *get()
  {
    return static_cast<VKContext *>(Context::get());
  }

  Set<VKVaoCache*> vao_caches_;
  void  vao_cache_register(VKVaoCache* cache) {
    lists_mutex_.lock();
    vao_caches_.add(cache);
    lists_mutex_.unlock();
  }
  void vao_cache_unregister(VKVaoCache* cache)
  {
    lists_mutex_.lock();
    vao_caches_.remove(cache);
    lists_mutex_.unlock();
  }
  void         create_swapchain_fb();
  uint32_t get_current_frame_index();
  uint32_t get_current_image_index();
  uint32_t get_transferQueueIndex();
  uint32_t get_graphicQueueIndex();
  VkImage get_current_image();
  VkImageLayout get_current_image_layout();
  

  void set_current_image_layout(VkImageLayout layout);

  bool begin_blit_submit(VkCommandBuffer& cmd);

  bool end_blit_submit(VkCommandBuffer& cmd, std::vector<VkSemaphore> batch_signal);

  bool is_support_format(VkFormat format,  VkFormatFeatureFlagBits flag,bool linear);


  VKStagingBufferManager *get_buffer_manager()
  {
    return buffer_manager_;
  }
  VkCommandBuffer request_command_buffer(bool second= false);
  void framebuffer_bind(VKFrameBuffer *framebuffer);
  void framebuffer_restore();

  blender::gpu::VKFrameBuffer *get_default_framebuffer();
  blender::gpu::VKFrameBuffer *get_current_framebuffer();
  void set_viewport(int origin_x, int origin_y, int width, int height);

  void set_scissor(int scissor_x, int scissor_y, int scissor_width, int scissor_height);

  void set_scissor_enabled(bool scissor_enabled);

  bool ensure_render_pipeline_state(GPUPrimType prim_type);



  bool get_inside_frame()
  {
    return is_inside_frame_;
  }
  /** Dummy Resources */
  /* Maximum of 32 texture types. Though most combinations invalid. */
  VKTexture *dummy_textures_[GPU_TEXTURE_BUFFER] = {nullptr};
  GPUVertFormat dummy_vertformat_;
  GPUVertBuf *dummy_verts_ = nullptr;
  VKTexture *get_dummy_texture(eGPUTextureType type);
  void bottom_transition();
  void fail_transition();

  private:
  /* Parent Context. */
   void orphans_clear();

   template<typename T>
   static  void orphans_add(Vector<T>& orphan_list, std::mutex& list_mutex, T id)
   {
     BLI_assert(id);
     list_mutex.lock();
     orphan_list.append(id);
     list_mutex.unlock();
   }
};

}  // namespace blender::gpu
