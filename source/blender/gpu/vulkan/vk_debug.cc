/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 *
 * Debug features of Vulkan.
 */


#include "BKE_global.h"

#include "GPU_debug.h"
#include "GPU_platform.h"

#include "CLG_log.h"


#include "vk_context.hh"
#include "vk_debug.hh"

static CLG_LogRef LOG = {"gpu.debug.vulkan"};
namespace blender {
namespace gpu {
namespace debug {
void init_vk_callbacks()
{
  CLOG_ENSURE(&LOG);
}
}  // namespace debug
}  // namespace gpu
}  // namespace blender
