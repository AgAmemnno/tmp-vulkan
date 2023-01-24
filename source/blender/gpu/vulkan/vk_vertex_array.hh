/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_batch.h"
#include "vk_shader_interface.hh"

namespace blender {
namespace gpu {

namespace VKVertArray {

/**
 * Update the Attribute Binding of the currently bound VAO.
 */
void update_bindings(const VKVAOty vao,
                     const GPUBatch *batch,
                     const ShaderInterface *interface,
                     int base_instance);

/**
 * Another version of update_bindings for Immediate mode.
 */
void update_bindings(const VKVAOty vao,
                     uint v_first,
                     const GPUVertFormat *format,
                     const ShaderInterface *interface);

}  // namespace GLVertArray

}  // namespace gpu
}  // namespace blender
