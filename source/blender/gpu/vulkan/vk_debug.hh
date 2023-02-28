/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Debug features of Vulkan.
 */

#pragma once


#include "vk_context.hh"

namespace blender {
namespace gpu {
namespace debug {

void raise_vk_error(const char *info);
void check_vk_resources(const char *info);
/**
 * This function needs to be called once per context.
 */
GHOST_TSuccess init_vk_callbacks(void *instance);
void destroy_vk_callbacks();
/**
 * Initialize a fallback layer (to KHR_debug) that covers only some functions.
 * We override the functions pointers by our own implementation that just checks #glGetError.
 * Some additional functions (not overridable) are covered inside the header using wrappers.
 */
void init_vk_debug_layer();

/* render doc demo => https://
 * github.com/baldurk/renderdoc/blob/52e9404961277d1bb1ed03a376b722c2b91e3762/util/test/demos/vk/vk_test.h#L231
 */
template<typename T> void object_vk_label(VkDevice device, T obj, const std::string &name);
void object_vk_label(VkDevice device, VkObjectType objType, uint64_t obj, const std::string &name);

void pushMarker(VkCommandBuffer cmd, const std::string &name);
void setMarker(VkCommandBuffer cmd, const std::string &name);
void popMarker(VkCommandBuffer cmd);
void pushMarker(VkQueue q, const std::string &name);
void setMarker(VkQueue q, const std::string &name);
void popMarker(VkQueue q);

}  // namespace debug
}  // namespace gpu
}  // namespace blender
