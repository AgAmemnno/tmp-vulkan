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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "gpu_uniform_buffer_private.hh"

#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_memory.hh"
#include "vk_shader.hh"
#include "vk_texture.hh"

#include "vk_uniform_buffer.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

VKUniformBuf::VKUniformBuf(size_t size, const char *name) : UniformBuf(size, name)
{
  /* Do not create ubo VK buffer here to allow allocation from any thread. */
  BLI_assert(size <= VKContext::max_ubo_size);
}

VKUniformBuf::~VKUniformBuf()
{

  VKContext::get()->buf_free(ubo);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data upload / update
 * \{ */

void VKUniformBuf::init()
{
  BLI_assert(VKContext::get());

  VKResourceOptions options;
  options.setHostVisible(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  ubo = new VKBuffer(size_in_bytes_,
                     VKStagingBufferManager::vk_staging_buffer_min_alignment,
                     "VKUniformBuf",
                     options);

  ubo_id_ = (uint64_t)(ubo);
  /* TODO::  #debug::object_label(VK_UNIFORM_BUFFER, ubo_id_, name_);*/
}

void VKUniformBuf::update(const void *data)
{
  if (ubo == nullptr) {
    this->init();
  }

  ubo->Copy((void *)data, size_in_bytes_);
}

void VKUniformBuf::clear_to_zero()
{

  /*When random access occurs, do memory barriers solve it? need to verify.*/
  if (ubo == nullptr) {
    this->init();
  }
  ubo->Fill(0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Usage
 * \{ */

void VKUniformBuf::bind(int slot)
{

  /*It is better to check at the time of shader compilation.*/
  if (slot >= VKContext::max_ubo_binds) {
    fprintf(
        stderr,
        "Error: Trying to bind \"%s\" ubo to slot %d which is above the reported limit of %d.\n",
        name_,
        slot,
        VKContext::max_ubo_binds);
    return;
  }

  if (ubo == nullptr) {
    this->init();
  }

  if (data_ != nullptr) {
    this->update(data_);
    MEM_SAFE_FREE(data_);
  }

  slot_ = slot;
  info.buffer = ubo->get_vk_buffer();
  info.offset = 0;
  info.range = VK_WHOLE_SIZE;
  VKShader *shader = VKContext::get()->pipeline_state.active_shader;
#if 0
  char *d = (char*)ubo->get_host_ptr();
  printf("BIND ================== set  %d  slot %d  size  %zu  host Pointer     %llx  \n",
         setID,
         slot,
         size_in_bytes_,
    (uintptr_t) d);
#endif
  shader->append_write_descriptor(setID, slot_, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, info);

#if 0
  BLI_assert(slot < 16);
  VKContext::get()->bound_ubo_slots |= 1 << slot;
#endif
}

void VKUniformBuf::bind_as_ssbo(int slot)
{

  /*NIL*/
  BLI_assert(false);
#if 0
  if (ubo_id_ == 0) {
    this->init();
  }
  if (data_ != nullptr) {
    this->update(data_);
    MEM_SAFE_FREE(data_);
  }

  glBindBufferBase(VK_SHADER_STORAGE_BUFFER, slot, ubo_id_);
#endif
}

void VKUniformBuf::unbind()
{
#if 0
  /* NOTE: This only unbinds the last bound slot. */
  glBindBufferBase(VK_UNIFORM_BUFFER, slot_, 0);
  /* Hope that the context did not change. */
  VKContext::get()->bound_ubo_slots &= ~(1 << slot_);
#endif
  slot_ = 0;
}

/** \} */

}  // namespace blender::gpu
