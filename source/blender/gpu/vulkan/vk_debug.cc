/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

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

#include "GHOST_C-api.h"



#  include <sstream>
#  include <unordered_set>


static CLG_LogRef LOG = {"gpu.debug.vulkan"};


namespace blender {
namespace gpu {
namespace debug {
struct VKDebuggingTools {

  /*One -on -one for instance.*/
  VkInstance instance = VK_NULL_HANDLE;

  VkDevice device = VK_NULL_HANDLE;
  PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessengerEXT = nullptr;
  PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessengerEXT = nullptr;
  VkDebugUtilsMessengerEXT dbgMessenger = nullptr;
  std::unordered_set<int32_t> dbgIgnoreMessages;
  VKDebuggingTools()
  {
    instance = VK_NULL_HANDLE;
    device = VK_NULL_HANDLE;
    dbgIgnoreMessages.clear();
    dbgMessenger = nullptr;
    createDebugUtilsMessengerEXT = nullptr;
    destroyDebugUtilsMessengerEXT = nullptr;
  }
};
}  // namespace debug
}  // namespace gpu
}  // namespace blender



namespace blender {
namespace gpu {
namespace debug {

/*If we don't have multiple instances, VKDebuggingTools is Singleton.*/
static VKDebuggingTools tools;

VKAPI_ATTR VkBool32 VKAPI_CALL
debugUtilsCB(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
             VkDebugUtilsMessageTypeFlagsEXT messageType,
             const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
             void *userData)
{

  VKDebuggingTools *ctx = reinterpret_cast<VKDebuggingTools *>(userData);
  if (ctx->dbgIgnoreMessages.find(callbackData->messageIdNumber) != ctx->dbgIgnoreMessages.end())
    return VK_FALSE;

  std::ostringstream message;
  message << "DebugUtils Callee :: ";

  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    message << ("VERBOSE: %s \n --> %s\n", callbackData->pMessageIdName, callbackData->pMessage);
  }
  else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    message << ("INFO: %s \n --> %s\n", callbackData->pMessageIdName, callbackData->pMessage);
  }
  else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    message << ("WARNING: %s \n --> %s\n", callbackData->pMessageIdName, callbackData->pMessage);
  }
  else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    message << ("ERROR: %s \n --> %s\n", callbackData->pMessageIdName, callbackData->pMessage);
  }
  else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
    message << ("GENERAL: %s \n --> %s\n", callbackData->pMessageIdName, callbackData->pMessage);
  }
  else {
    message << ("%s \n --> %s\n", callbackData->pMessageIdName, callbackData->pMessage);
  }

#ifdef _WIN32
  // MessageBoxA(NULL, message.str().c_str(), "Alert", MB_OK);
  std::cout << message.str() << std::endl;
#else
  std::cout << message.str() << std::endl;
#endif
  // this seems redundant with the info already in callbackData->pMessage
#if 0

	if (callbackData->objectCount > 0)
	{
		for (uint32_t object = 0; object < callbackData->objectCount; ++object)
		{
			std::string otype = ObjectTypeToString(callbackData->pObjects[object].objectType);
			LOGI(" Object[%d] - Type %s, Value %p, Name \"%s\"\n", object, otype.c_str(),
				(void*)(callbackData->pObjects[object].objectHandle), callbackData->pObjects[object].pObjectName);
		}
	}
	if (callbackData->cmdBufLabelCount > 0)
	{
		for (uint32_t label = 0; label < callbackData->cmdBufLabelCount; ++label)
		{
			LOGI(" Label[%d] - %s { %f, %f, %f, %f}\n", label, callbackData->pCmdBufLabels[label].pLabelName,
				callbackData->pCmdBufLabels[label].color[0], callbackData->pCmdBufLabels[label].color[1],
				callbackData->pCmdBufLabels[label].color[2], callbackData->pCmdBufLabels[label].color[3]);
		}
#endif
  // Don't bail out, but keep going.
  return VK_FALSE;
}

 static VkResult CreateDebugUtils(
                                 VkDebugUtilsMessageSeverityFlagsEXT flag,
                                 VKDebuggingTools &deb)
{
  deb.dbgIgnoreMessages.clear();
  deb.createDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      deb.instance, "vkCreateDebugUtilsMessengerEXT");
  deb.destroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      deb.instance, "vkDestroyDebugUtilsMessengerEXT");

  if (deb.createDebugUtilsMessengerEXT != nullptr) {
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
    return deb.createDebugUtilsMessengerEXT(
        deb.instance, &dbg_messenger_create_info, nullptr, &deb.dbgMessenger);
  }
  return VK_ERROR_UNKNOWN;
}
static VkResult DestroyDebugUtils(VKDebuggingTools &deb)
{

  if (deb.destroyDebugUtilsMessengerEXT) {
    deb.destroyDebugUtilsMessengerEXT(deb.instance, deb.dbgMessenger, nullptr);
  }
  deb.createDebugUtilsMessengerEXT = nullptr;
  deb.destroyDebugUtilsMessengerEXT = nullptr;
  deb.dbgIgnoreMessages.clear();
  deb.dbgMessenger = nullptr;
  deb.instance = VK_NULL_HANDLE;

  return VK_SUCCESS;
}

 static GHOST_TSuccess CreateDebug(VKDebuggingTools &deb)
{


    VkDebugUtilsMessageSeverityFlagsEXT flag = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    flag = (VkDebugReportFlagBitsEXT)(flag | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT);

    VK_CHECK(CreateDebugUtils( flag, deb));
    return GHOST_kSuccess;

};

GHOST_TSuccess init_vk_callbacks(void* instance)
{
  CLOG_ENSURE(&LOG);
  BLI_assert( (tools.instance == VK_NULL_HANDLE)  || ((tools.instance != VK_NULL_HANDLE) && (instance == tools.instance)));
  if (tools.instance != VK_NULL_HANDLE) {
    return GHOST_kSuccess;
  }
  tools.instance = (VkInstance)instance;
  CreateDebug(tools);
  return GHOST_kSuccess;
}
void destroy_vk_callbacks()
{
  if (tools.dbgMessenger) {
    DestroyDebugUtils(tools);
  }
}

void object_vk_label(VkDevice device, VkObjectType objType, uint64_t obj, const std::string &name)
{
 if ( G.debug & G_DEBUG_GPU ) {
    VkDebugUtilsObjectNameInfoEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = objType;
    info.objectHandle = obj;
    info.pObjectName = name.c_str();
    vkSetDebugUtilsObjectNameEXT(device, &info);
 }
}


template<> void object_vk_label(VkDevice device, VkPipeline obj, const std::string &name)
{
  static int stat = 0;
  auto name_ = name  + "_" + std::to_string(stat++);
  object_vk_label(device, VK_OBJECT_TYPE_PIPELINE, (uint64_t)obj, name_);
}
template<> void object_vk_label(VkDevice device, VkFramebuffer obj, const std::string &name)
{
  static int stat = 0;
  auto name_ = name + "_" + std::to_string(stat++);
  object_vk_label(device, VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)obj, name_);
}
template<> void object_vk_label(VkDevice device, VkImage obj, const std::string &name)
{
  static int stat = 0;
  auto name_ = name + "_" + std::to_string(stat++);
  object_vk_label(device, VK_OBJECT_TYPE_IMAGE, (uint64_t)obj, name_);
}
template<> void object_vk_label(VkDevice device, VkSampler obj, const std::string &name)
{
  static int stat = 0;
  auto name_ = name + "_" + std::to_string(stat++);
  object_vk_label(device, VK_OBJECT_TYPE_SAMPLER, (uint64_t)obj, name_);
}
template<> void object_vk_label(VkDevice device, VkBuffer obj, const std::string &name)
{
  static int stat = 0;
  auto name_ = name + "_" + std::to_string(stat++);
  object_vk_label(device, VK_OBJECT_TYPE_BUFFER, (uint64_t)obj, name_);
}
template<> void object_vk_label(VkDevice device, VkSemaphore obj, const std::string &name)
{
  static int stat = 0;
  auto name_ = name + "_" + std::to_string(stat++);
  object_vk_label(device, VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)obj, name_);
}
template<> void object_vk_label(VkDevice device, VkRenderPass obj, const std::string &name)
{
  static int stat = 0;
  auto name_ = name + "_" + std::to_string(stat++);
  object_vk_label(device, VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)obj, name_);
}
template<> void object_vk_label(VkDevice device, VkFence obj, const std::string &name)
{
  static int stat = 0;
  auto name_ = name + "_" + std::to_string(stat++);
  object_vk_label(device, VK_OBJECT_TYPE_FENCE, (uint64_t)obj, name_);
}
template<> void object_vk_label(VkDevice device, VkDescriptorSet obj, const std::string &name)
{
  static int stat = 0;
  auto name_ = name + "_" + std::to_string(stat++);
  object_vk_label(device, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)obj, name_);
}
template<> void object_vk_label(VkDevice device, VkDescriptorSetLayout obj, const std::string &name)
{
  static int stat = 0;
  auto name_ = name + "_" + std::to_string(stat++);
  object_vk_label(device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)obj, name_);
}
template<>void object_vk_label(VkDevice device, VkShaderModule obj, const std::string &name)
{
  static int stat = 0;
  auto name_ = name + "_" + std::to_string(stat++);
  object_vk_label(device, VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)obj, name_);
}
template<> void object_vk_label(VkDevice device, VkQueue obj, const std::string &name)
{
  static int stat = 0;
  auto name_ = name + "_" + std::to_string(stat++);
  object_vk_label(device, VK_OBJECT_TYPE_QUEUE, (uint64_t)obj, name_);
}
template<> void object_vk_label(VkDevice device, VkDescriptorPool obj, const std::string &name)
{
  static int stat = 0;
  auto name_ = name + "_" + std::to_string(stat++);
  object_vk_label(device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, (uint64_t)obj, name_);
}
void pushMarker(VkCommandBuffer cmd, const std::string &name)
{
  if (G.debug & G_DEBUG_GPU) {
    VkDebugUtilsLabelEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    info.pLabelName = name.c_str();
    vkCmdBeginDebugUtilsLabelEXT(cmd, &info);
  }
}
void setMarker(VkCommandBuffer cmd, const std::string &name)
{
  if (G.debug & G_DEBUG_GPU) {
    VkDebugUtilsLabelEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    info.pLabelName = name.c_str();
    vkCmdInsertDebugUtilsLabelEXT(cmd, &info);
  }
}
void popMarker(VkCommandBuffer cmd)
{
  if (G.debug & G_DEBUG_GPU) {
    vkCmdEndDebugUtilsLabelEXT(cmd);
  }
}
void pushMarker(VkQueue q, const std::string &name)
{
  if (G.debug & G_DEBUG_GPU) {
    VkDebugUtilsLabelEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    info.pLabelName = name.c_str();
    vkQueueBeginDebugUtilsLabelEXT(q, &info);
  }
}
void setMarker(VkQueue q, const std::string &name)
{
  if (G.debug & G_DEBUG_GPU) {
    VkDebugUtilsLabelEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    info.pLabelName = name.c_str();
    vkQueueInsertDebugUtilsLabelEXT(q, &info);
  }
}

void popMarker(VkQueue q)
{
  if (G.debug & G_DEBUG_GPU) {
    vkQueueEndDebugUtilsLabelEXT(q);
  }
}


 void init_vk_debug_layer(){};
void raise_vk_error(const char *info){};
void check_vk_resources(const char *info){};



}  // namespace debug
}  // namespace gpu
}  // namespace blender



/*
 * TODO:: Comment
 */
void GHOST_VulkanDebugUtilsRegister(void *m_instance)
{
  blender::gpu::debug::init_vk_callbacks(m_instance);
};

const char *GHOST_VulkanErrorAsString(int64_t result)
{
#define FORMAT_ERROR(X) \
  case X: { \
    return "" #X; \
  }

  switch ((VkResult)result) {
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

void GHOST_VulkanDebugUtilsUnregister()
{
  blender::gpu::debug::destroy_vk_callbacks();
}
#if 0
  void DebugMaster::ignoreDebugMessage(int32_t msgID)
{
  dbgIgnoreMessages.insert(msgID);
}

void DebugMaster::_setObjectName(uint64_t object, const std::string &name, VkObjectType t)
{

  VkDebugUtilsObjectNameInfoEXT s{
      VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, nullptr, t, object, name.c_str()};
  vkSetDebugUtilsObjectNameEXT(device, &s);
}

#if VK_NV_ray_tracing
void DebugMaster::setObjectName(VkAccelerationStructureNV object, const std::string &name)
{
  _setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV);
}
#endif
void DebugMaster::setObjectName(VkBuffer object, const std::string &name)
{
  _setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_BUFFER);
}

void DebugMaster::beginLabel(VkCommandBuffer cmdBuf, const std::string &label)
{

  VkDebugUtilsLabelEXT s{
      VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, label.c_str(), {1.0f, 1.0f, 1.0f, 1.0f}};
  vkCmdBeginDebugUtilsLabelEXT(cmdBuf, &s);
}
void DebugMaster::endLabel(VkCommandBuffer cmdBuf)
{

  vkCmdEndDebugUtilsLabelEXT(cmdBuf);
}
void DebugMaster::insertLabel(VkCommandBuffer cmdBuf, const std::string &label)
{

  VkDebugUtilsLabelEXT s{
      VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, label.c_str(), {1.0f, 1.0f, 1.0f, 1.0f}};
  vkCmdInsertDebugUtilsLabelEXT(cmdBuf, &s);
}
#endif
