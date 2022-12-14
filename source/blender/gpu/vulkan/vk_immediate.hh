/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Mimics old style opengl immediate mode drawing.
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "vk_mem_alloc.h"

#include "gpu_immediate_private.hh"
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;

typedef int GLint;
typedef unsigned int GLuint;

typedef int GLsizei;

typedef double GLdouble;
typedef double GLclampd;
typedef void *GLeglClientBufferEXT;
typedef void *GLeglImageOES;
typedef char GLchar;
typedef char GLcharARB;
namespace blender::gpu {
/* size of internal buffer */
#define DEFAULT_INTERNAL_BUFFER_SIZE (4 * 1024 * 1024)


  struct  VBO_INFO{
  /** Vulkan Handle for this buffer. */
  VkBuffer vbo_id = VK_NULL_HANDLE;
  /** Offset of the mapped data in data. */
  VkDeviceSize buffer_offset = 0;
  /** Size of the whole buffer in bytes. */
  VkDeviceSize buffer_size = 0;

  VmaAllocation allocation = nullptr;
};

  class VKImmediate : public Immediate {
 public:
    void record();

 private:

   VkPipeline current_pipe_ =VK_NULL_HANDLE;

  /** Size in bytes of the mapped region. */
  size_t bytes_mapped_ = 0;
  /** Vertex array for this immediate mode instance. */
  /// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_vertex_attribute_divisor.html

  struct vaoInfo{
    VkPipelineVertexInputStateCreateInfo info = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL};

    VkPipelineVertexInputDivisorStateCreateInfoEXT divisorInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT, NULL};

      Vector<VkVertexInputBindingDivisorDescriptionEXT> divisors;
      Vector<VkVertexInputBindingDescription> vertexInputBindings;
      Vector<VkVertexInputAttributeDescription> vertexInputAttributes;

      vaoInfo()
      {
         info.pNext = &divisorInfo;
         divisorInfo.vertexBindingDivisorCount = 0;
         divisorInfo.pVertexBindingDivisors = NULL;
      }
  } vao;

 uint16_t descript_vao(const ShaderInterface *interface_,
                    const GPUVertFormat *format,
                    uint v_first,
                    uint v_len,
                    const bool use_instancing);
  void update_bindings(
                                    const uint v_first,
                                    const GPUVertFormat *format,
                                    const ShaderInterface *interface_);
  uchar data_[4 * 1024 * 1024];
  VKContext *context_;
  VKBuffer* vkbuffer_=nullptr,*vkstaging_ =nullptr;
 public:
  VKImmediate(VKContext *context_);
  ~VKImmediate();

  uchar *begin(void) override;
  void end(void) override;


  
/// private:





};



}  // namespace blender::gpu
