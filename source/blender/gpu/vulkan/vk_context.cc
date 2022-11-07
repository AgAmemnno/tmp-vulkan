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

namespace blender::gpu {

template<class T, class Geom>
bool $createBuffer$(T &cmder, Geom *&geometry, bool _createInfo)
{

  MBvk input;
  MBvk index;
  VkDeviceSize size = 0;
  {

    VkMemoryRequirements memReqs;
    VkMemoryAllocateInfo memAlloc = {};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    VkBufferCreateInfo BufferInfo = {};
    BufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BufferInfo.size = geometry->array.memorySize;
    BufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    size = geometry->array.memorySize;

    VK_CHECK_RESULT(vkCreateBuffer($device, &BufferInfo, nullptr, &input.buffer));
    vkGetBufferMemoryRequirements($device, input.buffer, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = vka::shelve::getMemoryTypeIndex(
        memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory($device, &memAlloc, nullptr, &input.memory));
    VK_CHECK_RESULT(vkBindBufferMemory($device, input.buffer, input.memory, 0));

    BufferInfo.size = geometry->Size.index;

    size += geometry->Size.index;
    BufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VK_CHECK_RESULT(vkCreateBuffer($device, &BufferInfo, nullptr, &index.buffer));
    vkGetBufferMemoryRequirements($device, index.buffer, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = vka::shelve::getMemoryTypeIndex(
        memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory($device, &memAlloc, nullptr, &index.memory));
    VK_CHECK_RESULT(vkBindBufferMemory($device, index.buffer, index.memory, 0));
  }

  cmder.allocStaging(size);

  cmder.Map(geometry->map(1), 0, geometry->array.memorySize);
  cmder.Map(geometry->map(0), geometry->array.memorySize, geometry->Size.index);

  cmder.begin();
  cmder.Copy(input, geometry->array.memorySize, 0, 0);
  cmder.Copy(index, geometry->Size.index, geometry->array.memorySize, 0);
  cmder.end();
  cmder.submit();

  if (_createInfo)
    $createVertexInfo$(geometry);
  index.count = geometry->updateRange.count;

  cmder.wait();

  Mem(MBvk, SIZE_MB).$set$(std::move(input), geometry);
  geometry->ID.vert = geometry->id;
  geometry->info.vert = {.buffer = input.buffer, .offset = 0, .range = geometry->array.memorySize};

  Mem(MBvk, SIZE_MB).$set$(std::move(index), geometry);
  geometry->ID.index = geometry->id;
  geometry->info.index = {.buffer = index.buffer, .offset = 0, .range = geometry->Size.index};

  int c = (int)inth.size();
  inth["vertex" + std::to_string(c)] = geometry->ID.vert;
  c = (int)inth.size();
  inth["index" + std::to_string(c)] = geometry->ID.index;

  return true;
};

template<class T, class Geom>
bool $createBufferInstanced$(T &cmder,
                             Geom *&geometry,
                             bool _createInfo,
                             VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                        VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                             VkMemoryPropertyFlags mem = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
{



  if (geometry->attributes != nullptr)
    $createBuffer$(cmder, geometry->attributes->buffer, false);

  long orig = InterlockedCompareExchange(&(geometry->instance->buffer->id), INT32_MAX, -1);
  if (orig != -1)
    return false;

  MBvk insta;

  auto buf = geometry->instance->buffer;
  VkDeviceSize size = buf->array.memorySize;
  {

    VkMemoryRequirements memReqs;
    VkMemoryAllocateInfo memAlloc = {};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    VkBufferCreateInfo BufferInfo = {};
    BufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BufferInfo.size = size;
    BufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | usage;

    VK_CHECK_RESULT(vkCreateBuffer($device, &BufferInfo, nullptr, &insta.buffer));
    vkGetBufferMemoryRequirements($device, insta.buffer, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = vka::shelve::getMemoryTypeIndex(memReqs.memoryTypeBits, mem);
    VK_CHECK_RESULT(vkAllocateMemory($device, &memAlloc, nullptr, &insta.memory));
    VK_CHECK_RESULT(vkBindBufferMemory($device, insta.buffer, insta.memory, 0));
  }

  cmder.allocStaging(size);
  cmder.Map(buf->map(1), 0, size);
  cmder.begin();
  cmder.Copy(insta, size, 0, 0);
  cmder.end();
  cmder.submit();

  $VInfo.$setInstanced$(geometry);

  cmder.wait();

  geometry->instance->buffer->info.attr.buffer = insta.buffer;
  geometry->instance->buffer->info.attr.range = size;
  geometry->instance->buffer->info.attr.offset = 0;

  Mem(MBvk, SIZE_MB).$set$(std::move(insta), buf);
  int c = (int)inth.size();
  inth["instance" + std::to_string(c)] = buf->id;

  return true;
};



void VKContext::activate()
{
}

void VKContext::deactivate()
{
}

void VKContext::begin_frame()
{
}

void VKContext::end_frame()
{
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



VKContext::VKContext(void *ghost_window, void *ghost_context)// : main_command_buffer(*this)
    //   : memory_manager(*this),
{

    if (G.debug & G_DEBUG_GPU) {
      debug::init_vk_callbacks();
    }
    ghost_window_ = ghost_window;
    if (ghost_window_ && ghost_context == NULL) {
      GHOST_Window *ghostWin = reinterpret_cast<GHOST_Window *>(ghost_window_);
      ghost_context = (ghostWin ? ghostWin->getContext() : NULL);
    }
    BLI_assert(ghost_context);

#if 0
  if (ghost_window) {
    GLuint default_fbo = GHOST_GetDefaultOpenGLFramebuffer((GHOST_WindowHandle)ghost_window);
    GHOST_RectangleHandle bounds = GHOST_GetClientBounds((GHOST_WindowHandle)ghost_window);
    int w = GHOST_GetWidthRectangle(bounds);
    int h = GHOST_GetHeightRectangle(bounds);
    GHOST_DisposeRectangle(bounds);

    if (default_fbo != 0) {
      /* Bind default framebuffer, otherwise state might be undefined because of
       * detect_mip_render_workaround(). */
      glBindFramebuffer(GL_FRAMEBUFFER, default_fbo);
      front_left = new GLFrameBuffer("front_left", this, GL_COLOR_ATTACHMENT0, default_fbo, w, h);
      back_left = new GLFrameBuffer("back_left", this, GL_COLOR_ATTACHMENT0, default_fbo, w, h);
    }
    else {
      front_left = new GLFrameBuffer("front_left", this, GL_FRONT_LEFT, 0, w, h);
      back_left = new GLFrameBuffer("back_left", this, GL_BACK_LEFT, 0, w, h);
    }

    GLboolean supports_stereo_quad_buffer = GL_FALSE;
    glGetBooleanv(GL_STEREO, &supports_stereo_quad_buffer);
    if (supports_stereo_quad_buffer) {
      front_right = new GLFrameBuffer("front_right", this, GL_FRONT_RIGHT, 0, w, h);
      back_right = new GLFrameBuffer("back_right", this, GL_BACK_RIGHT, 0, w, h);
    }
  }
  else {
    /* For off-screen contexts. Default frame-buffer is null. */
    back_left = new GLFrameBuffer("back_left", this, GL_NONE, 0, 0, 0);
  }

  #endif
    this->ghost_context_ = static_cast<GHOST_ContextVK *>(ghost_context);


  /* Initialize Render-pass and Frame-buffer State. */
  this->back_left = nullptr;
  /* Initialize command buffer state. */
 // this->main_command_buffer.prepare();
  /* Initialize IMM and pipeline state */
  //this->pipeline_state.initialised = false;




  //state_manager = new VKStateManager();
  //imm = new VKImmediate();


 /*
  this->queue = (id<MTLCommandQueue>)this->ghost_context_->metalCommandQueue();
  this->device = (id<MTLDevice>)this->ghost_context_->metalDevice();
  BLI_assert(this->queue);
  BLI_assert(this->device);
  */

  
  /* Create FrameBuffer handles. */

 // this->front_left = new VKFrameBuffer(this, "front_left");
 // this->back_left =  new VKFrameBuffer(this, "back_left");

  this->active_fb = this->back_left;

  VKBackend::platform_init(this);
 // VKBackend::capabilities_init(this);


}

}  // namespace blender::gpu
