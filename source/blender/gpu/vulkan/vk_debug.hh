/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */
#pragma once

#include "BKE_global.h"
#include "gl_debug.hh"
#include "vk_common.hh"

namespace blender {
namespace gpu {

const char *to_string(VkObjectType type);

namespace debug {
struct VKDebuggingTools {
  /* Function pointer definitions .*/

  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT_r = nullptr;
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT_r = nullptr;
  PFN_vkSubmitDebugUtilsMessageEXT vkSubmitDebugUtilsMessageEXT_r = nullptr;
  PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT_r = nullptr;
  PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT_r = nullptr;
  PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT_r = nullptr;
  PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT_r = nullptr;
  PFN_vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXT_r = nullptr;
  PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT_r = nullptr;
  PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT_r = nullptr;
  PFN_vkSetDebugUtilsObjectTagEXT vkSetDebugUtilsObjectTagEXT_r = nullptr;

  bool enabled = false;
  /*One -on -one for instance.*/
  VkInstance instance = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT dbgMessenger = nullptr;
  Set<int32_t> dbgIgnoreMessages;
  std::mutex lists_mutex_;

  VKDebuggingTools();
  void clear();
  void add_ignore(int32_t id);
  void remove_ignore(int32_t id);
};
extern VKDebuggingTools tools;

void raise_vk_error(const char *info);
void check_vk_resources(const char *info);

template<typename... Args> void raise_vk_info(const std::string &fmt, Args... args)
{
  if (tools.enabled) {
    size_t len = std::snprintf(nullptr, 0, fmt.c_str(), args...);
    std::vector<char> info(len + 1);
    std::snprintf(&info[0], len + 1, fmt.c_str(), args...);

    static VkDebugUtilsMessengerCallbackDataEXT cbdata;
    cbdata.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    cbdata.pNext = 0;
    cbdata.messageIdNumber = 101;
    cbdata.pMessageIdName = "Blender Vulkan Debug Information::";
    cbdata.objectCount = 0;
    cbdata.flags = 0;
    cbdata.pObjects = nullptr;
    cbdata.pMessage = info.data();
    tools.vkSubmitDebugUtilsMessageEXT_r(tools.instance,
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
                                         &cbdata);
  }
}

/**
 * This function needs to be called once per context.
 */
bool init_vk_callbacks(void *instance, PFN_vkGetInstanceProcAddr instload);
void destroy_vk_callbacks();

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

/* clang-format off */
#  define __STR_VK_CHECK(s) "" #s
/* clang-format on */
namespace blender {
namespace tests {
void test_create();
}
}  // namespace  blender
