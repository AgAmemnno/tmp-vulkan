/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.h"
#include "BLI_set.hh"
#include "BLI_system.h"
#include "CLG_log.h"

#include "vk_backend.hh"
#include "vk_common.hh"
#include "vk_context.hh"

#define VK_DEBUG_ENABLED 1

#if defined(__unix__) || defined(__APPLE__)
#  include <sys/time.h>
#  include <unistd.h>
#  define GET_FUNC_ADDRESS dlsym
#endif
#if defined(_MSC_VER)
#  define NOMINMAX
#  define WINDOWS_LEAN_AND_MEAN
#  include <Windows.h>

#  include <io.h>

#  if !defined(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
#    define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#  endif

#  define GET_FUNC_ADDRESS (void *)GetProcAddress
#endif



#include <mutex>

namespace blender::gpu {
const char *to_vk_error_string(VkResult result)
{
#define FORMAT_ERROR(X) \
  case X: { \
    return "" #X; \
  }

  switch (result) {
    FORMAT_ERROR(VK_NOT_READY);
    FORMAT_ERROR(VK_TIMEOUT);
    FORMAT_ERROR(VK_EVENT_SET);
    FORMAT_ERROR(VK_EVENT_RESET);
    FORMAT_ERROR(VK_INCOMPLETE);
    FORMAT_ERROR(VK_ERROR_OUT_OF_HOST_MEMORY);
    FORMAT_ERROR(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    FORMAT_ERROR(VK_ERROR_INITIALIZATION_FAILED);
    FORMAT_ERROR(VK_ERROR_DEVICE_LOST);
    FORMAT_ERROR(VK_ERROR_MEMORY_MAP_FAILED);
    FORMAT_ERROR(VK_ERROR_LAYER_NOT_PRESENT);
    FORMAT_ERROR(VK_ERROR_EXTENSION_NOT_PRESENT);
    FORMAT_ERROR(VK_ERROR_FEATURE_NOT_PRESENT);
    FORMAT_ERROR(VK_ERROR_INCOMPATIBLE_DRIVER);
    FORMAT_ERROR(VK_ERROR_TOO_MANY_OBJECTS);
    FORMAT_ERROR(VK_ERROR_FORMAT_NOT_SUPPORTED);
    FORMAT_ERROR(VK_ERROR_FRAGMENTED_POOL);
    FORMAT_ERROR(VK_ERROR_UNKNOWN);
    FORMAT_ERROR(VK_ERROR_OUT_OF_POOL_MEMORY);
    FORMAT_ERROR(VK_ERROR_INVALID_EXTERNAL_HANDLE);
    FORMAT_ERROR(VK_ERROR_FRAGMENTATION);
    FORMAT_ERROR(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
    FORMAT_ERROR(VK_ERROR_SURFACE_LOST_KHR);
    FORMAT_ERROR(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
    FORMAT_ERROR(VK_SUBOPTIMAL_KHR);
    FORMAT_ERROR(VK_ERROR_OUT_OF_DATE_KHR);
    FORMAT_ERROR(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
    FORMAT_ERROR(VK_ERROR_VALIDATION_FAILED_EXT);
    FORMAT_ERROR(VK_ERROR_INVALID_SHADER_NV);
    FORMAT_ERROR(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
    FORMAT_ERROR(VK_ERROR_NOT_PERMITTED_EXT);
    FORMAT_ERROR(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
    FORMAT_ERROR(VK_THREAD_IDLE_KHR);
    FORMAT_ERROR(VK_THREAD_DONE_KHR);
    FORMAT_ERROR(VK_OPERATION_DEFERRED_KHR);
    FORMAT_ERROR(VK_OPERATION_NOT_DEFERRED_KHR);
    FORMAT_ERROR(VK_PIPELINE_COMPILE_REQUIRED_EXT);
    default:
      return "Unknown Error";
  }
}

namespace debug {
#if defined(VK_DEBUG_ENABLED)
static VkInstance vk_instance_s = VK_NULL_HANDLE;

VKDebuggingTools::VKDebuggingTools()
{
  clear();
}
void VKDebuggingTools::clear()
{
  instance = VK_NULL_HANDLE;
  device = VK_NULL_HANDLE;
  dbgIgnoreMessages.clear();
  dbgMessenger = nullptr;
  enabled = false;
}
void VKDebuggingTools::add_ignore(int32_t id)
{
  lists_mutex_.lock();
  dbgIgnoreMessages.add(id);
  lists_mutex_.unlock();
}
void VKDebuggingTools::remove_ignore(int32_t id)
{
  lists_mutex_.lock();
  dbgIgnoreMessages.remove(id);
  lists_mutex_.unlock();
}

/*for creating breakpoints.*/
extern inline void VK_ERROR_CHECK(VkResult r, const char *name)
{
  if (r != VK_SUCCESS) {
    fprintf(
        stderr, "Vulkan Error :: %s failled with %s\n", name, blender::gpu::to_vk_error_string(r));
  }
};

static CLG_LogRef LOG = {"gpu.debug.vulkan"};

#  define CONSOLE_COLOR_YELLOW "\x1b[33m"
#  define CONSOLE_COLOR_RED "\x1b[31m"
#  define CONSOLE_COLOR_RESET "\x1b[0m"
#  define CONSOLE_COLOR_FINE "\x1b[2m"

const char *to_string(VkObjectType type)
{
  switch (type) {

    case VK_OBJECT_TYPE_UNKNOWN:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_UNKNOWN);
    case VK_OBJECT_TYPE_INSTANCE:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_INSTANCE);
    case VK_OBJECT_TYPE_PHYSICAL_DEVICE:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_PHYSICAL_DEVICE);
    case VK_OBJECT_TYPE_DEVICE:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_DEVICE);
    case VK_OBJECT_TYPE_QUEUE:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_QUEUE);
    case VK_OBJECT_TYPE_SEMAPHORE:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_SEMAPHORE);
    case VK_OBJECT_TYPE_COMMAND_BUFFER:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_COMMAND_BUFFER);
    case VK_OBJECT_TYPE_FENCE:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_FENCE);
    case VK_OBJECT_TYPE_DEVICE_MEMORY:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_DEVICE_MEMORY);
    case VK_OBJECT_TYPE_BUFFER:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_BUFFER);
    case VK_OBJECT_TYPE_IMAGE:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_IMAGE);
    case VK_OBJECT_TYPE_EVENT:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_EVENT);
    case VK_OBJECT_TYPE_QUERY_POOL:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_QUERY_POOL);
    case VK_OBJECT_TYPE_BUFFER_VIEW:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_BUFFER_VIEW);
    case VK_OBJECT_TYPE_IMAGE_VIEW:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_IMAGE_VIEW);
    case VK_OBJECT_TYPE_SHADER_MODULE:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_SHADER_MODULE);
    case VK_OBJECT_TYPE_PIPELINE_CACHE:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_PIPELINE_CACHE);
    case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_PIPELINE_LAYOUT);
    case VK_OBJECT_TYPE_RENDER_PASS:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_RENDER_PASS);
    case VK_OBJECT_TYPE_PIPELINE:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_PIPELINE);
    case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
    case VK_OBJECT_TYPE_SAMPLER:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_SAMPLER);
    case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_DESCRIPTOR_POOL);
    case VK_OBJECT_TYPE_DESCRIPTOR_SET:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_DESCRIPTOR_SET);
    case VK_OBJECT_TYPE_FRAMEBUFFER:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_FRAMEBUFFER);
    case VK_OBJECT_TYPE_COMMAND_POOL:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_COMMAND_POOL);
    case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION);
    case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE);
    case VK_OBJECT_TYPE_SURFACE_KHR:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_SURFACE_KHR);
    case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_SWAPCHAIN_KHR);
    case VK_OBJECT_TYPE_DISPLAY_KHR:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_DISPLAY_KHR);
    case VK_OBJECT_TYPE_DISPLAY_MODE_KHR:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_DISPLAY_MODE_KHR);
    case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT);
#  ifdef VK_ENABLE_BETA_EXTENSIONS
    case VK_OBJECT_TYPE_VIDEO_SESSION_KHR:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_VIDEO_SESSION_KHR);
#  endif
#  ifdef VK_ENABLE_BETA_EXTENSIONS
    case VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR);
#  endif
    case VK_OBJECT_TYPE_CU_MODULE_NVX:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_CU_MODULE_NVX);
    case VK_OBJECT_TYPE_CU_FUNCTION_NVX:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_CU_FUNCTION_NVX);
    case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT);
    case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR);
    case VK_OBJECT_TYPE_VALIDATION_CACHE_EXT:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_VALIDATION_CACHE_EXT);
    case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV);
    case VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL);
    case VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR);
    case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV);
    case VK_OBJECT_TYPE_PRIVATE_DATA_SLOT_EXT:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_PRIVATE_DATA_SLOT_EXT);
    case VK_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA:
      return __STR_VK_CHECK(VK_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA);
    default:
      BLI_assert_unreachable();
  }
  return "NotFound";
};
static VkResult vulkan_dynamic_load(PFN_vkGetInstanceProcAddr &func)
{

#  if defined(_WIN32)
  HMODULE vulkanDll = LoadLibraryA("vulkan-1.dll");
  if (!vulkanDll) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
#  elif defined(__APPLE__)
  void *vulkanDll = dlopen("libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
  if (!vulkanDll) {
    vulkanDll = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
  }
  if (!vulkanDll) {
    vulkanDll = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
  }
  if (!vulkanDll) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }

#  else
  void *vulkanDll = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
  if (!vulkanDll) {
    vulkanDll = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
  }
  if (!vulkanDll) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
#  endif

#  define LOAD(name) GET_FUNC_ADDRESS(vulkanDll, name)

  func = (PFN_vkGetInstanceProcAddr)LOAD("vkGetInstanceProcAddr");
  // vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)INSTLOAD(instance,"vkGetDeviceProcAddr");
#  undef LOAD

  return VK_SUCCESS;
}

static void vulkan_dynamic_debug_functions(VKContext *context,
                                           PFN_vkGetInstanceProcAddr /* instload_*/)
{
  VKDebuggingTools &tools = context->debugging_tools_get();

  VkInstance instance = context->instance_get();
  tools.instance = instance;
  PFN_vkGetInstanceProcAddr instload;
  vulkan_dynamic_load(instload);
  if (instload) {

    tools.enabled = false;
    tools.vkCmdBeginDebugUtilsLabelEXT_r = (PFN_vkCmdBeginDebugUtilsLabelEXT)instload(
        instance, "vkCmdBeginDebugUtilsLabelEXT");
    tools.vkCmdEndDebugUtilsLabelEXT_r = (PFN_vkCmdEndDebugUtilsLabelEXT)instload(
        instance, "vkCmdEndDebugUtilsLabelEXT");
    tools.vkCmdInsertDebugUtilsLabelEXT_r = (PFN_vkCmdInsertDebugUtilsLabelEXT)instload(
        instance, "vkCmdInsertDebugUtilsLabelEXT");
    tools.vkCreateDebugUtilsMessengerEXT_r = (PFN_vkCreateDebugUtilsMessengerEXT)instload(
        instance, "vkCreateDebugUtilsMessengerEXT");
    tools.vkDestroyDebugUtilsMessengerEXT_r = (PFN_vkDestroyDebugUtilsMessengerEXT)instload(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    tools.vkQueueBeginDebugUtilsLabelEXT_r = (PFN_vkQueueBeginDebugUtilsLabelEXT)instload(
        instance, "vkQueueBeginDebugUtilsLabelEXT");
    tools.vkQueueEndDebugUtilsLabelEXT_r = (PFN_vkQueueEndDebugUtilsLabelEXT)instload(
        instance, "vkQueueEndDebugUtilsLabelEXT");
    tools.vkQueueInsertDebugUtilsLabelEXT_r = (PFN_vkQueueInsertDebugUtilsLabelEXT)instload(
        instance, "vkQueueInsertDebugUtilsLabelEXT");
    tools.vkSetDebugUtilsObjectNameEXT_r = (PFN_vkSetDebugUtilsObjectNameEXT)instload(
        instance, "vkSetDebugUtilsObjectNameEXT");
    tools.vkSetDebugUtilsObjectTagEXT_r = (PFN_vkSetDebugUtilsObjectTagEXT)instload(
        instance, "vkSetDebugUtilsObjectTagEXT");
    tools.vkSubmitDebugUtilsMessageEXT_r = (PFN_vkSubmitDebugUtilsMessageEXT)instload(
        instance, "vkSubmitDebugUtilsMessageEXT");
    tools.vkCreateDebugUtilsMessengerEXT_r = (PFN_vkCreateDebugUtilsMessengerEXT)instload(
        instance, "vkCreateDebugUtilsMessengerEXT");
    tools.vkDestroyDebugUtilsMessengerEXT_r = (PFN_vkDestroyDebugUtilsMessengerEXT)instload(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (tools.vkCmdBeginDebugUtilsLabelEXT_r) {
      tools.enabled = true;
      vk_instance_s = instance;
    }
  }
  else {
    tools.vkCmdBeginDebugUtilsLabelEXT_r = nullptr;
    tools.vkCmdEndDebugUtilsLabelEXT_r = nullptr;
    tools.vkCmdInsertDebugUtilsLabelEXT_r = nullptr;
    tools.vkCreateDebugUtilsMessengerEXT_r = nullptr;
    tools.vkDestroyDebugUtilsMessengerEXT_r = nullptr;
    tools.vkQueueBeginDebugUtilsLabelEXT_r = nullptr;
    tools.vkQueueEndDebugUtilsLabelEXT_r = nullptr;
    tools.vkQueueInsertDebugUtilsLabelEXT_r = nullptr;
    tools.vkSetDebugUtilsObjectNameEXT_r = nullptr;
    tools.vkSetDebugUtilsObjectTagEXT_r = nullptr;
    tools.vkSubmitDebugUtilsMessageEXT_r = nullptr;
    tools.enabled = false;
  }
}

VKAPI_ATTR VkBool32 VKAPI_CALL
debugUtilsCB(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
             VkDebugUtilsMessageTypeFlagsEXT /* messageType*/,
             const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
             void *userData)
{

  VKDebuggingTools *ctx = reinterpret_cast<VKDebuggingTools *>(userData);
  if (ctx->dbgIgnoreMessages.contains(callbackData->messageIdNumber)) {
    return VK_FALSE;
  }

  const bool use_color = CLG_color_support_get(&LOG);
  bool enabled = false;
  if ((messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) ||
      (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)) {

    // if ((LOG.type->flag & CLG_FLAG_USE) && (LOG.type->level >= CLG_SEVERITY_INFO)) {
    if (true) {
      const char *format = use_color ? CONSOLE_COLOR_FINE "% s\n %s " CONSOLE_COLOR_RESET :
                                       " % s\n %s ";
      CLG_logf(LOG.type,
               CLG_SEVERITY_INFO,
               "",
               "",
               format,
               callbackData->pMessageIdName,
               callbackData->pMessage);
      enabled = true;
    }
  }
  else {

    CLG_Severity clog_severity;
    switch (messageSeverity) {
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        clog_severity = CLG_SEVERITY_WARN;
        break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        clog_severity = CLG_SEVERITY_ERROR;
        break;
      default:
        BLI_assert_unreachable();
    }

    enabled = true;
    if (clog_severity == CLG_SEVERITY_ERROR) {

      const char *format = use_color ? CONSOLE_COLOR_RED "%s\n" CONSOLE_COLOR_RESET " %s \n " :
                                       " %s\n %s ";
      CLG_logf(LOG.type,
               clog_severity,
               "",
               "",
               format,
               callbackData->pMessageIdName,
               callbackData->pMessage);
    }
    else if (LOG.type->level >= CLG_SEVERITY_WARN) {
      const char *format = use_color ? CONSOLE_COLOR_YELLOW "%s\n" CONSOLE_COLOR_RESET " %s \n " :
                                       " %s\n %s ";
      CLG_logf(LOG.type,
               clog_severity,
               "",
               "",
               format,
               callbackData->pMessageIdName,
               callbackData->pMessage);
    }
  }

  if ((enabled) && ((callbackData->objectCount > 0) || (callbackData->cmdBufLabelCount > 0) ||
                    (callbackData->queueLabelCount > 0))) {
    const size_t str_size = 512;
    char message[str_size];
    memset(message, 0, str_size);

    if (callbackData->objectCount > 0) {
      char tmp_message[500];
      const char *title = use_color ? CONSOLE_COLOR_FINE
                              "\n %d Object[s] related \n" CONSOLE_COLOR_RESET :
                                      "\n %d Object[s] related \n";
      const char *format = use_color ? CONSOLE_COLOR_FINE
                               "ObjectType[%s], Handle[0x%p], Name[%s] \n" CONSOLE_COLOR_RESET :
                                       "ObjectType[%s], Handle[0x%p], Name[%s] \n";

      sprintf(tmp_message, title, callbackData->objectCount);
      strcat(message, tmp_message);
      for (uint32_t object = 0; object < callbackData->objectCount; ++object) {
        sprintf(tmp_message,
                format,
                to_string(callbackData->pObjects[object].objectType),
                (void *)(callbackData->pObjects[object].objectHandle),
                callbackData->pObjects[object].pObjectName);
        strcat(message, tmp_message);
      }
    }
    if (callbackData->cmdBufLabelCount > 0) {
      const char *title = use_color ? CONSOLE_COLOR_FINE
                              " \n %d Command Buffer Label[s] \n " CONSOLE_COLOR_RESET :
                                      " \n %d Command Buffer Label[s] \n ";

      const char *format = use_color ? CONSOLE_COLOR_FINE
                               " CmdLabels[%d]  %s \n" CONSOLE_COLOR_RESET :
                                       " CmdLabels[%d]  %s \n";
      char tmp_message[500];
      sprintf(tmp_message, title, callbackData->cmdBufLabelCount);
      strcat(message, tmp_message);
      for (uint32_t label = 0; label < callbackData->cmdBufLabelCount; ++label) {
        sprintf(tmp_message, format, -(int)label, callbackData->pCmdBufLabels[label].pLabelName);
        strcat(message, tmp_message);
      }
    }
    if (callbackData->queueLabelCount > 0) {
      const char *title = use_color ? CONSOLE_COLOR_FINE
                              "\n % d Queue Label[s] \n " CONSOLE_COLOR_RESET :
                                      "\n % d Queue Label[s] \n ";
      const char *format = use_color ? CONSOLE_COLOR_FINE
                               " QLabels[%d]  %s \n" CONSOLE_COLOR_RESET :
                                       " QLabels[%d]  %s \n";

      char tmp_message[500];
      sprintf(tmp_message, title, callbackData->queueLabelCount);
      strcat(message, tmp_message);
      for (uint32_t label = 0; label < callbackData->queueLabelCount; ++label) {
        sprintf(tmp_message, format, -(int)label, callbackData->pQueueLabels[label].pLabelName);
        strcat(message, tmp_message);
      }
    }
    printf("%s\n", message);
    fflush(stdout);
  }

  return VK_TRUE;
};

static VkResult CreateDebugUtils(VkDebugUtilsMessageSeverityFlagsEXT flag, VKDebuggingTools &deb)
{

  deb.dbgIgnoreMessages.clear();
  BLI_assert(deb.vkCreateDebugUtilsMessengerEXT_r);

  VkDebugUtilsMessengerCreateInfoEXT dbg_messenger_create_info;
  dbg_messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  dbg_messenger_create_info.pNext = nullptr;
  dbg_messenger_create_info.flags = 0;
  dbg_messenger_create_info.messageSeverity = flag;
  dbg_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  dbg_messenger_create_info.pfnUserCallback = debugUtilsCB;
  dbg_messenger_create_info.pUserData = &deb;
  return deb.vkCreateDebugUtilsMessengerEXT_r(
      deb.instance, &dbg_messenger_create_info, nullptr, &deb.dbgMessenger);
}
static VkResult DestroyDebugUtils(VKDebuggingTools &deb)
{

  BLI_assert(deb.vkDestroyDebugUtilsMessengerEXT_r);
  deb.vkDestroyDebugUtilsMessengerEXT_r(deb.instance, deb.dbgMessenger, nullptr);

  deb.dbgIgnoreMessages.clear();
  deb.dbgMessenger = nullptr;
  deb.instance = VK_NULL_HANDLE;

  return VK_SUCCESS;
}

static bool CreateDebug(VKDebuggingTools &deb)
{
  /*Associate flag settings with interfaces?*/
  VkDebugUtilsMessageSeverityFlagsEXT flag = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                             VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

  flag = (VkDebugReportFlagBitsEXT)(flag | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT);

  CreateDebugUtils(flag, deb);
  return true;
};

#endif

bool init_callbacks(VKContext *context, PFN_vkGetInstanceProcAddr instload)
{
  if (vk_instance_s == VK_NULL_HANDLE) {
    vulkan_dynamic_debug_functions(context, instload);
    CreateDebug(context->debugging_tools_get());
    return true;
  };
  return false;
}

void destroy_callbacks(VKContext *context, VKDebuggingTools &tools)
{
  if (tools.enabled) {
    vulkan_dynamic_debug_functions(context, nullptr);
    DestroyDebugUtils(tools);
    vk_instance_s = VK_NULL_HANDLE;
  }
}

VKDebuggingTools &VKDebuggingTools::operator=(VKDebuggingTools &tools)
{

  this->instance = tools.instance;
  this->dbgMessenger = tools.dbgMessenger;
  this->vkDestroyDebugUtilsMessengerEXT_r = tools.vkDestroyDebugUtilsMessengerEXT_r;
  this->vkCreateDebugUtilsMessengerEXT_r = tools.vkCreateDebugUtilsMessengerEXT_r;
  this->enabled = tools.enabled;
  return *this;
}

void object_label(VKContext *context, VkObjectType objType, uint64_t obj, const char *name)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &tools = context->debugging_tools_get();
    if (tools.enabled) {
      VkDebugUtilsObjectNameInfoEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
      info.objectType = objType;
      info.objectHandle = obj;
      info.pObjectName = name;
      tools.vkSetDebugUtilsObjectNameEXT_r(context->device_get(), &info);
    }
  }
}

void push_marker(VKContext *context, VkCommandBuffer cmd, const char *name)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &tools = context->debugging_tools_get();
    if (tools.enabled) {
      VkDebugUtilsLabelEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      info.pLabelName = name;
      tools.vkCmdBeginDebugUtilsLabelEXT_r(cmd, &info);
    }
  }
}

void set_marker(VKContext *context, VkCommandBuffer cmd, const char *name)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &tools = context->debugging_tools_get();
    if (tools.enabled) {
      VkDebugUtilsLabelEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      info.pLabelName = name;
      tools.vkCmdInsertDebugUtilsLabelEXT_r(cmd, &info);
    }
  }
}

void pop_marker(VKContext *context, VkCommandBuffer cmd)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &tools = context->debugging_tools_get();
    if (tools.enabled) {
      tools.vkCmdEndDebugUtilsLabelEXT_r(cmd);
    }
  }
}

void push_marker(VKContext *context, VkQueue queue, const char *name)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &tools = context->debugging_tools_get();
    if (tools.enabled) {
      VkDebugUtilsLabelEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      info.pLabelName = name;
      tools.vkQueueBeginDebugUtilsLabelEXT_r(queue, &info);
    }
  }
}

void set_marker(VKContext *context, VkQueue queue, const char *name)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &tools = context->debugging_tools_get();
    if (tools.enabled) {
      VkDebugUtilsLabelEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      info.pLabelName = name;
      tools.vkQueueInsertDebugUtilsLabelEXT_r(queue, &info);
    }
  }
}

void pop_marker(VKContext *context, VkQueue queue)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &tools = context->debugging_tools_get();
    if (tools.enabled) {
      tools.vkQueueEndDebugUtilsLabelEXT_r(queue);
    }
  }
}

};  // namespace debug

}  // namespace blender::gpu

namespace blender::gpu {
void VKContext::debug_group_begin(const char *, int) {}

void VKContext::debug_group_end() {}

bool VKContext::debug_capture_begin()
{
  return VKBackend::get().debug_capture_begin(vk_instance_);
}

void VKContext::debug_capture_title(const char* title)
{
  VKBackend::get().debug_capture_title(title);
}

void VKBackend::debug_capture_title(const char* title)
{
#ifdef WITH_RENDERDOC
  return renderdoc_api_.set_capture_title(title);
#else
  UNUSED_VARS(vk_instance);
  return false;
#endif
};

bool VKBackend::debug_capture_begin(VkInstance vk_instance)
{
#ifdef WITH_RENDERDOC
  return renderdoc_api_.start_frame_capture(vk_instance, nullptr);
#else
  UNUSED_VARS(vk_instance);
  return false;
#endif
}

void VKContext::debug_capture_end()
{
  VKBackend::get().debug_capture_end(vk_instance_);
}

void VKBackend::debug_capture_end(VkInstance vk_instance)
{
#ifdef WITH_RENDERDOC
  renderdoc_api_.end_frame_capture(vk_instance, nullptr);
#else
  UNUSED_VARS(vk_instance);
#endif
}

void *VKContext::debug_capture_scope_create(const char * /*name*/)
{
  return nullptr;
}

bool VKContext::debug_capture_scope_begin(void * /*scope*/)
{
  return false;
}

void VKContext::debug_capture_scope_end(void * /*scope*/) {}
}  // namespace blender::gpu
