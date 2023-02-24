/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. 
 */

/** \file
 * \ingroup gpu
 *
 * Debug features of Vulkan.
 */

#pragma once


#include "GHOST_C-api.h"

namespace blender {
namespace gpu {
namespace debug {

        void raise_vk_error(const char *info);
void check_vk_resources(const char *info);
        /**
         * This function needs to be called once per context.
         */
GHOST_TSuccess init_vk_callbacks(void* instance);
        void destroy_vk_callbacks();
        /**
         * Initialize a fallback layer (to KHR_debug) that covers only some functions.
         * We override the functions pointers by our own implementation that just checks #glGetError.
         * Some additional functions (not overridable) are covered inside the header using wrappers.
         */
        void init_vk_debug_layer();
        //void object_vk_label(GLenum type, GLuint object, const char *name){};

      }  // namespace debug
    }  // namespace gpu
}  // namespace blender
