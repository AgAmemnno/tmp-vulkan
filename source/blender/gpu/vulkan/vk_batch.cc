/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_batch.hh"
#include "vk_context.hh"
#include "vk_shader.hh"
#include "vk_state.hh"
#include "vk_framebuffer.hh"
#include "vk_vertex_array.hh"
#include "vk_index_buffer.hh"

namespace blender::gpu {
  /* -------------------------------------------------------------------- */
/** \name VAO Cache
 *
 * Each #VKBatch has a small cache of VAO objects that are used to avoid VAO reconfiguration.
 * TODO(fclem): Could be revisited to avoid so much cross references.
 * \{ */

  VKVaoCache::VKVaoCache()
  {
    init();
  }

  VKVaoCache::~VKVaoCache()
  {
    this->clear();
  }

  void VKVaoCache::init()
  {
    context_ = nullptr;
    interface_ = nullptr;
    is_dynamic_vao_count = false;
    for (int i = 0; i < VK_GPU_VAO_STATIC_LEN; i++) {
      static_vaos.interfaces[i] = nullptr;
      static_vaos.vao_ids[i] = nullptr;
    }
    vao_base_instance_ = nullptr;
    base_instance_ = 0;
    vao_id_ = 0;
  }

  void VKVaoCache::insert(const VKShaderInterface* interface, VKVAOty vao)
  {
    /* Now insert the cache. */
    if (!is_dynamic_vao_count) {
      int i; /* find first unused slot */
      for (i = 0; i < VK_GPU_VAO_STATIC_LEN; i++) {
        if (static_vaos.vao_ids[i] == nullptr) {
          break;
        }
      }

      if (i < VK_GPU_VAO_STATIC_LEN) {
        static_vaos.interfaces[i] = interface;
        static_vaos.vao_ids[i] = vao;
      }
      else {
        /* Erase previous entries, they will be added back if drawn again. */
        for (int i = 0; i < VK_GPU_VAO_STATIC_LEN; i++) {
          if (static_vaos.interfaces[i] != nullptr) {
            const_cast<VKShaderInterface*>(static_vaos.interfaces[i])->ref_remove(this);
            context_->vao_free(static_vaos.vao_ids[i]);
          }
        }
        /* Not enough place switch to dynamic. */
        is_dynamic_vao_count = true;
        /* Init dynamic arrays and let the branch below set the values. */
        dynamic_vaos.count = GPU_BATCH_VAO_DYN_ALLOC_COUNT;
        dynamic_vaos.interfaces = (const VKShaderInterface**)MEM_callocN(
        dynamic_vaos.count * sizeof(VKShaderInterface*), "dyn vaos interfaces");
        dynamic_vaos.vao_ids = (VKVAOty*)MEM_callocN(dynamic_vaos.count * sizeof(VKVAOty), "dyn vaos ids");
      }
    }

    if (is_dynamic_vao_count) {
      int i; /* find first unused slot */
      for (i = 0; i < dynamic_vaos.count; i++) {
        if (dynamic_vaos.vao_ids[i] == nullptr) {
          break;
        }
      }

      if (i == dynamic_vaos.count) {
        /* Not enough place, realloc the array. */
        i = dynamic_vaos.count;
        dynamic_vaos.count += GPU_BATCH_VAO_DYN_ALLOC_COUNT;
        dynamic_vaos.interfaces = (const VKShaderInterface**)MEM_recallocN(
          (void*)dynamic_vaos.interfaces, sizeof(VKShaderInterface*) * dynamic_vaos.count);
        dynamic_vaos.vao_ids = (VKVAOty*)MEM_recallocN(dynamic_vaos.vao_ids, sizeof(VKVAOty) * dynamic_vaos.count);
       }

      dynamic_vaos.interfaces[i] = interface;
      dynamic_vaos.vao_ids[i] = vao;
    }

    const_cast<VKShaderInterface*>(interface)->ref_add(this);
  }

  void VKVaoCache::remove(const VKShaderInterface* interface)
  {
    const int count = (is_dynamic_vao_count) ? dynamic_vaos.count : VK_GPU_VAO_STATIC_LEN;
    VecVKVAOty vaos = (is_dynamic_vao_count) ? dynamic_vaos.vao_ids : static_vaos.vao_ids;
    
    const VKShaderInterface** interfaces = (is_dynamic_vao_count) ? dynamic_vaos.interfaces :
      static_vaos.interfaces;
    for (int i = 0; i < count; i++) {
      if (interfaces[i] == interface) {
        context_->vao_free(vaos[i]);
        vaos[i] = nullptr;
        interfaces[i] = nullptr;
        break; /* cannot have duplicates */
      }
    }

  }

  void VKVaoCache::clear()
  {
  
    const int count = (is_dynamic_vao_count) ? dynamic_vaos.count : VK_GPU_VAO_STATIC_LEN;
    VecVKVAOty vaos = (is_dynamic_vao_count) ? dynamic_vaos.vao_ids :  static_vaos.vao_ids;
    const VKShaderInterface** interfaces = (is_dynamic_vao_count) ? dynamic_vaos.interfaces : static_vaos.interfaces;

    /* Early out, nothing to free. */
    if (context_ == nullptr) {
      return;
    }


    /* TODO(fclem): Slow way. Could avoid multiple mutex lock here */
    for (int i = 0; i < count; i++) {
      context_->vao_free(vaos[i]);
    }
    context_->vao_free(vao_base_instance_);


    for (int i = 0; i < count; i++) {
      if (interfaces[i] != nullptr) {
        const_cast<VKShaderInterface*>(interfaces[i])->ref_remove(this);
      }
    }

    if (is_dynamic_vao_count) {
      MEM_freeN((void*)dynamic_vaos.interfaces);
      MEM_freeN((void*)dynamic_vaos.vao_ids);
    }

    if (context_) {
      context_->vao_cache_unregister(this);
    }
    /* Reinit. */
    this->init();
  }

  VKVAOty VKVaoCache::lookup(const VKShaderInterface* interface)
  {
    const int count = (is_dynamic_vao_count) ? dynamic_vaos.count : VK_GPU_VAO_STATIC_LEN;
    const VKShaderInterface** interfaces = (is_dynamic_vao_count) ? dynamic_vaos.interfaces :
      static_vaos.interfaces;
    for (int i = 0; i < count; i++) {
      if (interfaces[i] == interface) {
        return (is_dynamic_vao_count) ? dynamic_vaos.vao_ids[i] : static_vaos.vao_ids[i];
      }
    }
    return nullptr;
  }

  void VKVaoCache::context_check()
  {
    VKContext* ctx = VKContext::get();
    BLI_assert(ctx);

    if (context_ != ctx) {
      if (context_ != nullptr) {
        /* IMPORTANT: Trying to draw a batch in multiple different context will trash the VAO cache.
         * This has major performance impact and should be avoided in most cases. */
        context_->vao_cache_unregister(this);
      }
      this->clear();
      context_ = ctx;
      context_->vao_cache_register(this);
    }
  }

  VKVAOty VKVaoCache::base_instance_vao_get(GPUBatch* batch, int i_first)
  {
    this->context_check();
    /* Make sure the interface is up to date. */
    Shader* shader = VKContext::get()->shader;
    VKShaderInterface* interface = static_cast<VKShaderInterface*>(shader->interface);
    if (interface_ != interface) {
      vao_get(batch);
      /* Trigger update. */
      base_instance_ = 0;
    }
    /**
     * There seems to be a nasty bug when drawing using the same VAO reconfiguring (T71147).
     * We just use a throwaway VAO for that. Note that this is likely to degrade performance.
     */
#ifdef __APPLE__
    glDeleteVertexArrays(1, &vao_base_instance_);
    vao_base_instance_ = 0;
    base_instance_ = 0;
#endif
    BLI_assert(false);
    /*
    if (vao_base_instance_ == nullptr) {
      glGenVertexArrays(1, &vao_base_instance_);
    }

    if (base_instance_ != i_first) {
      base_instance_ = i_first;
      VKVertArray::update_bindings(vao_base_instance_, batch, interface_, i_first);
    }
    */
    return vao_base_instance_;
  }

  VKVAOty VKVaoCache::vao_get(GPUBatch* batch)
  {
    this->context_check();

    Shader* shader = VKContext::get()->shader;
    VKShaderInterface* interface = static_cast<VKShaderInterface*>(shader->interface);
    if (interface_ != interface) {
      interface_ = interface;
    };

    vao_id_ = this->lookup(interface_);

    if (vao_id_ == nullptr) {
      /* Cache miss, create a new VAO. */
      VKResourceOptions options;
      options.setDeviceLocal(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
       
      vao_id_ = new VKVAOty_impl(0, 256, options);
        
      /*#glGenVertexArrays(1, &vao_id_);*/
      this->insert(interface_, vao_id_);

       
    }

    BLI_assert(vao_id_);
    VKVertArray::update_bindings(vao_id_, batch, interface_, 0);

    return vao_id_;
  }






  void VKBatch::bind(int i_first)
  {
    VKContext::get()->state_manager->apply_state();


    if (flag & GPU_BATCH_DIRTY) {
      flag &= ~GPU_BATCH_DIRTY;
      vao_cache_.clear();
    }

/* TODO :: #if GPU_TRACK_INDEX_RANGE */
#if  0
    /* Can be removed if GL 4.3 is required. */
    if (!VKContext::fixed_restart_index_support) {
      glPrimitiveRestartIndex((elem != nullptr) ? this->elem_()->restart_index() : 0xFFFFFFFFu);
    }
#endif


    /*Too much processing intensive. #glBindVertexArray();*/
    /*Since the vertexbuffer is handled independently, there should be variations in how to upload.
     It is possible to have a direct pointer to the gpu if it is host-visible.*/
    vao_cache_.vao_get(this);

  }

  void VKBatch::draw(int v_first, int  v_count, int  i_first, int  i_count)
  {
    /* #GL_CHECK_RESOURCES("Batch"); */
    if (v_count <= 0 || i_count <= 0)
      return;

    auto context_ = VKContext::get();
    
    static int cnt = 0;

    auto fb_ = static_cast<VKFrameBuffer*>(context_->active_fb);
    VkCommandBuffer cmd = fb_->render_begin(VK_NULL_HANDLE, VK_COMMAND_BUFFER_LEVEL_PRIMARY, (VkClearValue*)nullptr, false);
    if (cnt == 110) {
      printf("BP");
    }
    this->bind(i_first);





    int w = fb_->get_width();
    int h = fb_->get_height();
    printf("Offscreen render     ( %d  ,%d )     FB    %llu \n", w, h, (uint64_t)fb_);




    auto image_index = context_->get_current_image_index();


    /*Here, setting prim_type without fail means that there is a premise that the topology type will be handled dynamically.*/
    VKStateManager::set_prim_type(prim_type);
    VKShader* vkshader = reinterpret_cast<VKShader*>(shader);
    vkshader->CreatePipeline(fb_->get_render_pass());

    auto current_pipe_ = vkshader->get_pipeline();
    BLI_assert(current_pipe_ != VK_NULL_HANDLE);

    vkshader->update_descriptor_set();


    auto vkinterface = (VKShaderInterface*)vkshader->interface;
    Vector<VkDescriptorSet > Sets;
    auto descN = 0;
    for (auto& set : vkinterface->sets_vec_) {
      if (set[image_index] != VK_NULL_HANDLE) {
        descN++;
        Sets.append(set[image_index]);
      }
    }




    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, current_pipe_);

    VKStateManager::cmd_dynamic_state(cmd);

    if (vkinterface->push_range_.size > 0) {
      vkCmdPushConstants(cmd,
        vkshader->current_layout_,
        vkinterface->push_range_.stageFlags,
        vkinterface->push_range_.offset,
        vkinterface->push_range_.size,
        vkinterface->push_cache_);
    }

    if (descN > 0) {
      vkCmdBindDescriptorSets(cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        vkshader->current_layout_,
        0,
        descN,
        Sets.data(),
        0,
        NULL);
    }



    if (elem) {

      const VKIndexBuf* el =static_cast<VKIndexBuf*>(elem_());
      auto idx_count = el->index_len_get();
      auto idx_first = el->index_base_;
      vkCmdDrawIndexed(cmd, idx_count, i_count, idx_first , v_first, i_first);
      
    }
    else {
      vkCmdDraw(cmd, v_count, i_count, v_first, i_first);
    }
    fb_->is_dirty_render_ = true;


    fb_->render_end();
   

    /*Test by presenting immediately.*/
#if 1
   
    std::string filename = "vk_frame_" + std::to_string(fb_->get_height()) + std::to_string(fb_->get_width()) + "No" + std::to_string(cnt) +".ppm";
    fb_->save_current_frame(filename.c_str());

    cnt++;
    if (cnt == 20) {
      printf("BP");
    }
    if(cnt == -14)
    {
      VKFrameBuffer* swfb = static_cast<VKFrameBuffer*> (context_->back_left);
     
      swfb->render_begin(VK_NULL_HANDLE, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, true);
      swfb->append_wait_semaphore(fb_->get_signal());
      fb_->blit_to(GPU_COLOR_BIT, 0, swfb, 0, 0, 0);
      swfb->render_end();
    
    };

#endif

    VKContext::get()->active_fb = fb_;

};

  void VKBatch::draw_indirect(GPUStorageBuf* /*indirect_buf*/, intptr_t /*offset*/)
  {
    BLI_assert("NIL   draw_indirect \n");
  };

  void VKBatch::multi_draw_indirect(GPUStorageBuf* /*indirect_buf*/,
    int /*count*/,
    intptr_t /*offset*/,
    intptr_t /*stride*/)
  {
    BLI_assert("NIL   multi_draw_indirect \n");
  };






}  // namespace blender::gpu
