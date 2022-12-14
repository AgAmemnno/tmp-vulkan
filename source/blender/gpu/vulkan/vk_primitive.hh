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
 *
 * Encapsulation of Framebuffer states (attached textures, viewport, scissors).
 */

#pragma once

#include "BLI_assert.h"

#include "GPU_primitive.h"

namespace blender::gpu {

static inline GLenum to_gl(GPUPrimType prim_type)
{
  BLI_assert(prim_type != GPU_PRIM_NONE);
  switch (prim_type) {
    default:
    case GPU_PRIM_POINTS:
    case GPU_PRIM_LINES:
    case GPU_PRIM_LINE_STRIP:
    case GPU_PRIM_LINE_LOOP:
    case GPU_PRIM_TRIS:
    case GPU_PRIM_TRI_STRIP:
    case GPU_PRIM_TRI_FAN:
    case GPU_PRIM_LINES_ADJ:
    case GPU_PRIM_LINE_STRIP_ADJ:
    case GPU_PRIM_TRIS_ADJ:
      return 0;
  };
}

}  // namespace blender::gpu
