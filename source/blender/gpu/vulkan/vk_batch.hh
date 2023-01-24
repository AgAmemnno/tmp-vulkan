/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_batch_private.hh"
#include "vk_context.hh"

namespace blender::gpu {
  const  uint32_t  VK_GPU_VAO_STATIC_LEN = 3;


  /**
   * VAO management: remembers all geometry state (vertex attribute bindings & element buffer)
   * for each shader interface. Start with a static number of VAO's and fallback to dynamic count
   * if necessary. Once a batch goes dynamic it does not go back.
   */
class VKVaoCache {

  private:
    /** Context for which the vao_cache_ was generated. */
    VKContext* context_ = nullptr;
    /** Last interface this batch was drawn with. */
    VKShaderInterface* interface_ = nullptr;
    /** Cached VAO for the last interface. */
    VKVAOty vao_id_ = nullptr;
    /** Used when arb_base_instance is not supported. */
    VKVAOty vao_base_instance_ = nullptr;
    
    int base_instance_ = 0;

    bool is_dynamic_vao_count = false;
    union {
      /** Static handle count */
      struct {
        const VKShaderInterface* interfaces[VK_GPU_VAO_STATIC_LEN];
        VKVAOty vao_ids[VK_GPU_VAO_STATIC_LEN];
      } static_vaos;
      /** Dynamic handle count */
      struct {
        uint32_t count;
        const VKShaderInterface** interfaces;
        VecVKVAOty  vao_ids;
      } dynamic_vaos;
    };

  public:
    VKVaoCache();
    ~VKVaoCache();

    VKVAOty vao_get(GPUBatch* batch);
    VKVAOty base_instance_vao_get(GPUBatch* batch, int i_first);

    /**
     * Return nullptr on cache miss (invalid VAO).
     */
    VKVAOty lookup(const VKShaderInterface* interface);
    /**
     * Create a new VAO object and store it in the cache.
     */
    void insert(const VKShaderInterface* interface, VKVAOty  vao_id);
    void remove(const VKShaderInterface* interface);
    void clear();

  private:
    void init();
    /**
     * The #GLVaoCache object is only valid for one #GLContext.
     * Reset the cache if trying to draw in another context;.
     */
    void context_check();
  };


class VKBatch : public Batch {
private:
  VKVaoCache vao_cache_;
 public:

   void bind(int i_first);
  void draw(int v_first, int v_count, int i_first, int i_count) override;
  void draw_indirect(GPUStorageBuf *indirect_buf, intptr_t offset) override;
  void multi_draw_indirect(GPUStorageBuf *indirect_buf,
                           int count,
                           intptr_t offset,
                           intptr_t stride) override;
};


}  // namespace blender::gpu
