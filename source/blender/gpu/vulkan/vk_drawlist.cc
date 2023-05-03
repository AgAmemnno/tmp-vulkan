/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */
#include "BLI_assert.h"

#include "GPU_batch.h"
#include "GPU_debug.h"

#include "gpu_context_private.hh"
#include "gpu_drawlist_private.hh"
#include "gpu_vertex_buffer_private.hh"

#include "vk_buffer.hh"
#include "vk_debug.hh"
#include "vk_drawlist.hh"
#include "vk_index_buffer.hh"
#include "vk_framebuffer.hh"
#include "vk_batch.hh"

namespace blender::gpu {

#define VK_MDI_ENABLED (buffer_size_ != 0)
#define VK_MDI_DISABLED (buffer_size_ == 0)
#define VK_MDI_INDEXED (base_index_ != UINT_MAX)

VKDrawList::~VKDrawList(){
  buffer_.free(*VKContext::get());
};

VKDrawList::VKDrawList(int length)
{
  BLI_assert(length > 0);
  batch_ = nullptr;
  command_len_ = 0;
  base_index_ = 0;
  command_offset_ = 0;
  data_size_ = 0;
  data_ = nullptr;
  buffer_size_ = sizeof(VkDrawIndexedIndirectCommand) * length;
  /*
  if (VKContext::multi_draw_indirect_support) {
    buffer_size_ = sizeof(VkDrawIndexedIndirectCommand) * length;
  }
  else {
    buffer_size_ = 0;
  }
  */
  /* Force buffer specification on first init. */
  data_offset_ = buffer_size_;
}

void VKDrawList::init()
{
  VKContext* context = VKContext::get();
  BLI_assert(VKContext::get());
  BLI_assert(VK_MDI_ENABLED);
  BLI_assert(data_ == nullptr);
  batch_ = nullptr;
  command_len_ = 0;

  if(!buffer_.is_allocated()) {
      buffer_.create(*context,
                        buffer_size_,
                        GPU_USAGE_DYNAMIC,
                        (VkBufferUsageFlagBits)(VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                VK_BUFFER_USAGE_TRANSFER_DST_BIT));
   }
   //convert_host_to_device(staging_buffer.mapped_memory_get(), data, sample_len, format, format_);
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
  data_ = (int8_t *)(buffer_.mapped_memory_get()) + data_offset_; 
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
    VKIndexBuffer*el   = static_cast<VKIndexBuffer *>(batch_->elem_());
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
static void activate(VKContext& context,int CNT){
VKFrameBuffer *fb = context.active_framebuffer_get();

  int viewport[4];
  fb->viewport_get(viewport);
  printf(
      ">>>>>>>>>>>>>>>>>>>>>>VKDrawList <<<<<<<<<<<<< CNT[%d]  FrameBuffer %llx Shader %s Viewport (%d %d "
      "%d %d) \n",
      CNT,
      (uintptr_t)fb->vk_framebuffer_get(),
      context.shader->name_get(),
      viewport[0],
      viewport[1],
      viewport[2],
      viewport[3]);
  context.activate_framebuffer(*fb);
  if(std::string(context.shader->name_get()) == "workbench_composite_studio")
  {
    printf("");
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
  static int CNT = 0;
  auto fb_ = static_cast<VKFrameBuffer *>(context_->active_fb);
  bool rebuild = false;
  if (CNT > 0) {
    rebuild = true;
  }


  GPU_debug_capture_begin();
  VKContext::get()->debug_capture_title( (std::string("BTC") + std::to_string(CNT)).c_str());
  VKContext &context = *VKContext::get();
  activate(context,CNT);
  const bool is_finishing_a_buffer = (command_offset_ + command_size > data_size_);
  context.state_manager->apply_state();
  if (command_len_ > 2 || is_finishing_a_buffer) {
    BLI_assert_unreachable();
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
  CNT++;
}

}  // namespace blender::gpu
