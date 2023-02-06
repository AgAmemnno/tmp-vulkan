/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */
#include "BLI_assert.h"

#include "GPU_batch.h"

#include "gpu_context_private.hh"
#include "gpu_drawlist_private.hh"
#include "gpu_vertex_buffer_private.hh"

#include "vk_state.hh"
#include "vk_drawlist.hh"
#include "vk_index_buffer.hh"
#include "vk_framebuffer.hh"


namespace blender::gpu {

#define VK_MDI_ENABLED (buffer_size_ != 0)
#define VK_MDI_DISABLED (buffer_size_ == 0)
#define VK_MDI_INDEXED (base_index_ != UINT_MAX)

VKDrawList::~VKDrawList(){};
VKDrawList::VKDrawList(int length)
{
  BLI_assert(length > 0);
  batch_ = nullptr;
  buffer_id_ = 0;
  command_len_ = 0;
  base_index_ = 0;
  command_offset_ = 0;
  data_size_ = 0;
  data_ = nullptr;

  if (VKContext::multi_draw_indirect_support) {
    /* Alloc the biggest possible command list, which is indexed. */
    buffer_size_ = sizeof(VkDrawIndexedIndirectCommand) * length;
  }
  else {
    /* Indicates MDI is not supported. */
    buffer_size_ = 0;
  }
  /* Force buffer specification on first init. */
  data_offset_ = buffer_size_;
}

void VKDrawList::init()
{
  BLI_assert(VKContext::get());
  BLI_assert(VK_MDI_ENABLED);
  BLI_assert(data_ == nullptr);
  batch_ = nullptr;
  command_len_ = 0;

  if (buffer_id_ == nullptr) {
    /* Allocate on first use. */
      VKResourceOptions options;
      options.setHostVisible(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
      buffer_id_ = new VKBuffer(buffer_size_, VKStagingBufferManager::vk_staging_buffer_min_alignment, options);
      context_ = VKContext::get();
  }

  /* If buffer is full, orphan buffer data and start fresh. */
  size_t command_size = VK_MDI_INDEXED ? sizeof(VkDrawIndexedIndirectCommand) :sizeof(VkDrawIndirectCommand);
  if (data_offset_ + command_size > buffer_size_) {
    /* glBufferData(GL_DRAW_INDIRECT_BUFFER, buffer_size_, nullptr, GL_DYNAMIC_DRAW); */
    data_offset_ = 0;
  }

  /* TODO :: Map the remaining range. 
  #GLbitfield flag = GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT;
  #data_ = (GLbyte *)glMapBufferRange(GL_DRAW_INDIRECT_BUFFER, data_offset_, data_size_, flag);
  */

  data_size_ = buffer_size_ - data_offset_;
  data_ = (GLbyte *)(buffer_id_->get_host_ptr()) + data_offset_; 
  command_offset_ = 0;

}

void VKDrawList::append(GPUBatch *gpu_batch, int i_first, int i_count)
{
  /* Fallback when MultiDrawIndirect is not supported/enabled. */
  if (VK_MDI_DISABLED) {
    GPU_batch_draw_advanced(gpu_batch, 0, 0, i_first, i_count);
    return;
  }

  if (data_ == nullptr) {
    this->init();
  }

  VKBatch *batch = static_cast<VKBatch *>(gpu_batch);
  if (batch != batch_) {
    // BLI_assert(batch->flag | GPU_BATCH_INIT);
    this->submit();
    batch_ = batch;
    /* Cached for faster access. */
    VKIndexBuf *el   = static_cast<VKIndexBuf *>(batch_->elem_());
    base_index_       = el ? el->index_base_ : UINT_MAX;
    v_first_ = el ? el->index_start_ : 0;
    v_count_ = el ? el->index_len_ : batch->verts_(0)->vertex_len;
  }

  if (v_count_ == 0) {
    /* Nothing to draw. */
    return;
  }

  if (VK_MDI_INDEXED) {
    VkDrawIndexedIndirectCommand *cmd = reinterpret_cast<VkDrawIndexedIndirectCommand *>(data_ + command_offset_);
    cmd->vertexOffset = v_first_;
    cmd->indexCount = v_count_;
    cmd->instanceCount = i_count;
    cmd->firstIndex = base_index_;
    cmd->firstInstance = i_first;
  }
  else {
    VkDrawIndirectCommand *cmd = reinterpret_cast<VkDrawIndirectCommand *>(data_ +
                                                                           command_offset_);
    cmd->firstVertex = v_first_;
    cmd->vertexCount = v_count_;
    cmd->instanceCount = i_count;
    cmd->firstInstance = i_first;
  }

  size_t command_size = VK_MDI_INDEXED ? sizeof(VkDrawIndexedIndirectCommand) :
                                         sizeof(VkDrawIndirectCommand);

  command_offset_ += command_size;
  command_len_++;

  /* Check if we can fit at least one other command. */
  if (command_offset_ + command_size > data_size_) {
    this->submit();
  }
}

void VKDrawList::submit()
{
  if (command_len_ == 0) {
    return;
  }
  /* Something's wrong if we get here without MDI support. */
  BLI_assert(VK_MDI_ENABLED);
  BLI_assert(data_);
  BLI_assert(VKContext::get()->shader != nullptr);

  size_t command_size = VK_MDI_INDEXED ? sizeof(VkDrawIndexedIndirectCommand) : sizeof(VkDrawIndirectCommand);

  auto context_ = VKContext::get();
  static int cnt = 0;
  auto fb_ = static_cast<VKFrameBuffer *>(context_->active_fb);
  VkCommandBuffer cmd = fb_->render_begin(VK_NULL_HANDLE, VK_COMMAND_BUFFER_LEVEL_PRIMARY, (VkClearValue *)nullptr, false);


  /* Only do multi-draw indirect if doing more than 2 drawcall. This avoids the overhead of
   * buffer mapping if scene is not very instance friendly. BUT we also need to take into
   * account the case where only a few instances are needed to finish filling a call buffer. */
  const bool is_finishing_a_buffer = (command_offset_ + command_size > data_size_);
  if (command_len_ > 2 || is_finishing_a_buffer) {

    /*A fallback level bind that isn't very well timing.*/
    VKStateManager::set_prim_type(batch_->prim_type);
    VKShader *vkshader = reinterpret_cast<VKShader *>(VKContext::get()->shader);
    vkshader->CreatePipeline(fb_);

    auto current_pipe_ = vkshader->get_pipeline();
    BLI_assert(current_pipe_ != VK_NULL_HANDLE);

    vkshader->update_descriptor_set();
    auto image_index = context_->get_current_image_index();
    auto vkinterface = (VKShaderInterface *)vkshader->interface;
    Vector<VkDescriptorSet> Sets;
    auto descN = 0;
    for (auto &set : vkinterface->sets_vec_) {
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

    batch_->bind(0);

    if (VK_MDI_INDEXED) {
      vkCmdDrawIndexedIndirect(cmd,
                               buffer_id_->get_vk_buffer(),
                               data_offset_,
                               command_len_,
                               sizeof(VkDrawIndexedIndirectCommand));
   
    }
    else {
      vkCmdDrawIndirect(cmd,
                        buffer_id_->get_vk_buffer(),
                        data_offset_,
                        command_len_,
                        sizeof(VkDrawIndexedIndirectCommand));
    }


    buffer_id_->unmap();
    data_ = nullptr;
    data_offset_ += command_offset_;

  }
  else {
    /* Fallback do simple drawcalls, and don't unmap the buffer. */
    if (VK_MDI_INDEXED) {
      VkDrawIndexedIndirectCommand *icmd = (VkDrawIndexedIndirectCommand *)data_;
      for (int i = 0; i < command_len_; i++, icmd++) {
        /* Index start was already added. Avoid counting it twice. */
        icmd->vertexOffset = v_first_;
        batch_->draw(
            icmd->vertexOffset, icmd->indexCount, icmd->firstInstance, icmd->instanceCount);
      }
      /* Reuse the same data. */
      command_offset_ -= command_len_ * sizeof(VkDrawIndexedIndirectCommand);
    }
    else {
      VkDrawIndirectCommand *icmd = (VkDrawIndirectCommand *)data_;
      for (int i = 0; i < command_len_; i++, icmd++) {
        batch_->draw(
            icmd->firstVertex, icmd->vertexCount, icmd->firstInstance, icmd->instanceCount);
      }
      /* Reuse the same data. */
      command_offset_ -= command_len_ * sizeof(VkDrawIndirectCommand);
    }
  }
  /* Do not submit this buffer again. */
  command_len_ = 0;
  /* Avoid keeping reference to the batch. */
  batch_ = nullptr;
}

}  // namespace blender::gpu
