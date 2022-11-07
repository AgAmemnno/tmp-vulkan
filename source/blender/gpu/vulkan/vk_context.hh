/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"
#include "gpu_context_private.hh"
#include "GPU_common_types.h"
#include "GPU_context.h"

#include "intern/GHOST_Context.h"
#include "intern/GHOST_ContextVK.h"
#include "intern/GHOST_Window.h"


#define GPU_VK_DEBUG

#ifdef GPU_VK_DEBUG
#  define GPU_VK_DEBUG_PRINTF(...) printf(__VA_ARGS__);
#else
#  define GPU_VK_DEBUG_PRINTF(...)
#endif



namespace blender::gpu {




class VKContext : public Context {

 public:
  VKContext(void *ghost_window, void *ghost_context);
   /*
  VKScratchBufferManager memory_manager;
  static VKBufferPool global_memory_manager;
  
  /* CommandBuffer managers.   */
  //VKCommandBufferManager main_command_buffer;


  VkDevice device;
  void activate() override;
  void deactivate() override;
  void begin_frame() override;
  void end_frame() override;

  void flush() override;
  void finish() override;

  void memory_statistics_get(int *total_mem, int *free_mem) override;

  void debug_group_begin(const char *, int) override;
  void debug_group_end() override;
  GHOST_ContextVK *ghost_context_; 
  
  private:
  /* Parent Context. */
  
};

}  // namespace blender::gpu
