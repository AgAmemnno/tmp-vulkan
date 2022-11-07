/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */
#include "BLI_assert.h"
#include "BLI_utildefines.h"
#include "GHOST_ContextVK.h"

#ifdef _WIN32
#  include <vulkan/vulkan.h>
#  include <vulkan/vulkan_win32.h>

#  include <vulkan/extensions_vk.hpp>

#elif defined(__APPLE__)
#  include <MoltenVK/vk_mvk_moltenvk.h>
#else /* X11 */
#  include <vulkan/vulkan_xlib.h>
#  ifdef WITH_GHOST_WAYLAND
#    include <vulkan/vulkan_wayland.h>
#  endif
#endif

#include <vector>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>

/* Set to 0 to allow devices that do not have the required features.
 * This allows development on OSX until we really needs these features. */
#define STRICT_REQUIREMENTS 1

    using namespace std;

const char *vulkan_error_as_string(VkResult result)
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




#define DEBUG_PRINTF(...) \
  if (m_debug) { \
    printf(__VA_ARGS__); \
  }


static VkPhysicalDeviceMemoryProperties __memoryProperties;


uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties)
{
  // Iterate over all memory types available for the device used in this example
  for (uint32_t i = 0; i < __memoryProperties.memoryTypeCount; i++) {
    if ((typeBits & 1) == 1) {
      if ((__memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
        return i;
      }
    }
    typeBits >>= 1;
  }

  throw "Could not find a suitable memory type!";
}

uint32_t getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound)
{

  for (uint32_t i = 0; i < __memoryProperties.memoryTypeCount; i++)
  {
    if ((typeBits & 1) == 1)
    {
      if ((__memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
      {
        if (memTypeFound)
        {
          *memTypeFound = true;
        }
        return i;
      }
    }
    typeBits >>= 1;
  }

  if (memTypeFound)
  {
    *memTypeFound = false;
    return 0;
  }
  else
  {
    throw  "Could not find a matching memory type";
  }
}
bool MemoryTypeFromProperties(uint32_t nMemoryTypeBits,
                              VkMemoryPropertyFlags nMemoryProperties,
                              uint32_t *pTypeIndexOut)
{
  for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
    if ((nMemoryTypeBits & 1) == 1) {
      // Type is available, does it match user properties?
      if ((__memoryProperties.memoryTypes[i].propertyFlags & nMemoryProperties) ==
          nMemoryProperties) {
        *pTypeIndexOut = i;
        return true;
      }
    }
    nMemoryTypeBits >>= 1;
  }

  // No memory types matched, return failure
  return false;
};

#ifdef VULKAN_ERROR_CHECK
typedef struct MIBmvk {
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDescriptorBufferInfo info;
  VkBuffer buffer = VK_NULL_HANDLE;
  void *mapped;
  VkDeviceSize size;

  int version = {-1};
  void dealloc(VkDevice device)
  {
    if (buffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, buffer, nullptr);
      vkFreeMemory(device, memory, nullptr);
      buffer = VK_NULL_HANDLE;
    };
  };
  bool isValid()
  {
    return buffer != VK_NULL_HANDLE;
  }

} MIBmvk;

static std::vector<MIBmvk> test_buf;

void test_destroy(VkDevice& device)
{
  
    for (auto mib : test_buf) { mib.dealloc(device);}
    test_buf.clear();

}
template<class Ctx> 
GHOST_TSuccess test_createBuffer(Ctx &ctx,
                   MIBmvk &ubo,
                    VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VkMemoryPropertyFlags memTy = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
{

  VkInstance instance;
  VkDevice device;
  VkPhysicalDevice physical_device;
  uint32_t queueFamily;
  ctx.getVulkanHandles(&instance, &physical_device, &device, &queueFamily);

  {

    VkMemoryRequirements memReqs;
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkBufferCreateInfo BufferInfo = {};
    BufferInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BufferInfo.size = ubo.size;
    BufferInfo.usage  = usage;
    VK_CHECK(vkCreateBuffer(device, &BufferInfo, nullptr, &ubo.buffer));
    vkGetBufferMemoryRequirements(device, ubo.buffer, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, memTy);

    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &(ubo.memory)));
    VK_CHECK(vkBindBufferMemory(device, ubo.buffer, ubo.memory, 0));

    ubo.info.buffer = ubo.buffer;
    ubo.info.offset = 0;
    ubo.info.range = memReqs.size;

    VK_CHECK(vkMapMemory(device, ubo.memory, 0, ubo.size, 0, (void **)&ubo.mapped));

   
    printf(" Buffer Size %zu ========>>>>>>>    ReqSize %zu = [reqAlign %zu ]   \n",
            ubo.size,
            memReqs.size,
            memReqs.alignment);
  }


 

  return GHOST_kSuccess;


};



#include <unordered_set>
struct DebugMaster {

  PFN_vkCreateDebugUtilsMessengerEXT    createDebugUtilsMessengerEXT = nullptr;
  PFN_vkDestroyDebugUtilsMessengerEXT  destroyDebugUtilsMessengerEXT = nullptr;
  VkDebugUtilsMessengerEXT dbgMessenger = nullptr;
  std::unordered_set<int32_t> dbgIgnoreMessages;
  VkDevice device;
  void ignoreDebugMessage(int32_t msgID)
  {
    dbgIgnoreMessages.insert(msgID);
  }



  void _setObjectName(uint64_t object,const  std::string &name, VkObjectType t)
  {

      VkDebugUtilsObjectNameInfoEXT s{
          VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, nullptr, t, object, name.c_str()};
      vkSetDebugUtilsObjectNameEXT(device, &s);

  }

#  if VK_NV_ray_tracing
  void setObjectName(VkAccelerationStructureNV object, const std::string &name)
  {
    _setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV);
  }
#  endif
  void setObjectName(VkBuffer object, const std::string &name)
  {
    _setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_BUFFER);
  }
  #if 0
  void setObjectName(VkBufferView object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_BUFFER_VIEW);
  }
  void setObjectName(VkCommandBuffer object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_COMMAND_BUFFER);
  }
  void setObjectName(VkCommandPool object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_COMMAND_POOL);
  }
  void setObjectName(VkDescriptorPool object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_DESCRIPTOR_POOL);
  }
  void setObjectName(VkDescriptorSet object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_DESCRIPTOR_SET);
  }
  void setObjectName(VkDescriptorSetLayout object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
  }
  void setObjectName(VkDeviceMemory object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_DEVICE_MEMORY);
  }
  void setObjectName(VkFramebuffer object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_FRAMEBUFFER);
  }
  void setObjectName(VkImage object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_IMAGE);
  }
  void setObjectName(VkImageView object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_IMAGE_VIEW);
  }
  void setObjectName(VkPipeline object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_PIPELINE);
  }
  void setObjectName(VkPipelineLayout object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_PIPELINE_LAYOUT);
  }
  void setObjectName(VkQueryPool object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_QUERY_POOL);
  }
  void setObjectName(VkQueue object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_QUEUE);
  }
  void setObjectName(VkRenderPass object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_RENDER_PASS);
  }
  void setObjectName(VkSampler object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_SAMPLER);
  }
  void setObjectName(VkSemaphore object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_SEMAPHORE);
  }
  void setObjectName(VkShaderModule object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_SHADER_MODULE);
  }
  void setObjectName(VkSwapchainKHR object, const std::string &name)
  {
    setObjectName((uint64_t)object, name, VK_OBJECT_TYPE_SWAPCHAIN_KHR);
  }
  #endif
  // clang-format on
  //
  //---------------------------------------------------------------------------
  //
  void beginLabel(VkCommandBuffer cmdBuf, const std::string &label)
  {

      VkDebugUtilsLabelEXT s{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                             nullptr,
                             label.c_str(),
                             {1.0f, 1.0f, 1.0f, 1.0f}};
      vkCmdBeginDebugUtilsLabelEXT(cmdBuf, &s);

  }
  void endLabel(VkCommandBuffer cmdBuf)
  {

      vkCmdEndDebugUtilsLabelEXT(cmdBuf);

  }
  void insertLabel(VkCommandBuffer cmdBuf, const std::string &label)
  {

      VkDebugUtilsLabelEXT s{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                             nullptr,
                             label.c_str(),
                             {1.0f, 1.0f, 1.0f, 1.0f}};
      vkCmdInsertDebugUtilsLabelEXT(cmdBuf, &s);

  }
  //
  // Begin and End Command Label MUST be balanced, this helps as it will always close the opened
  // label
  //
  struct ScopedCmdLabel {
    ScopedCmdLabel(VkCommandBuffer cmdBuf, const std::string &label) : m_cmdBuf(cmdBuf)
    {
      if (s_enabled) {
        VkDebugUtilsLabelEXT s{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                               nullptr,
                               label.c_str(),
                               {1.0f, 1.0f, 1.0f, 1.0f}};
        vkCmdBeginDebugUtilsLabelEXT(cmdBuf, &s);
      }
    }
    ~ScopedCmdLabel()
    {
      if (s_enabled) {
        vkCmdEndDebugUtilsLabelEXT(m_cmdBuf);
      }
    }
    void setLabel(const std::string &label)
    {
      if (s_enabled) {
        VkDebugUtilsLabelEXT s{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                               nullptr,
                               label.c_str(),
                               {1.0f, 1.0f, 1.0f, 1.0f}};
        vkCmdInsertDebugUtilsLabelEXT(m_cmdBuf, &s);
      }
    }

   private:
    VkCommandBuffer m_cmdBuf;
  };

  ScopedCmdLabel scopeLabel(VkCommandBuffer cmdBuf, const std::string &label)
  {
    return ScopedCmdLabel(cmdBuf, label);
  }

 private:
  static bool s_enabled;
};
static DebugMaster deb;

#endif




#ifdef VULKAN_ERROR_CHECK




#  include <sstream>
VKAPI_ATTR VkBool32 VKAPI_CALL dbgReportCB(VkDebugReportFlagsEXT msgFlags,
                                           VkDebugReportObjectTypeEXT objType,
                                           uint64_t srcObject,
                                           size_t location,
                                           int32_t msgCode,
                                           const char *pLayerPrefix,
                                           const char *pMsg,
                                           void *pUserData)
{
  std::ostringstream message;
  message << "DebugReport Callee :: ";
  if (msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
    message << "ERROR: ";
  }
  else if (msgFlags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
    message << "WARNING: ";
  }
  else if (msgFlags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
    message << "PERFORMANCE WARNING: ";
  }
  else if (msgFlags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
    message << "INFO: ";
  }
  else if (msgFlags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
    message << "DEBUG: ";
  }
  message << "[" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg;

#  ifdef _WIN32
  //MessageBoxA(NULL, message.str().c_str(), "Alert", MB_OK);
  std::cout << message.str() << std::endl;

#  else
  std::cout << message.str() << std::endl;
#  endif

  /*
   * false indicates that layer should not bail-out of an
   * API call that had validation failures. This may mean that the
   * app dies inside the driver due to invalid parameter(s).
   * That's what would happen without validation layers, so we'll
   * keep that behavior here.
   */
  return false;
}

VKAPI_ATTR VkBool32 VKAPI_CALL
debugUtilsCB(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                            VkDebugUtilsMessageTypeFlagsEXT messageType,
                            const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
                            void *userData)
{

  DebugMaster *ctx = reinterpret_cast<DebugMaster *>(userData);
  if (ctx->dbgIgnoreMessages.find(callbackData->messageIdNumber) != ctx->dbgIgnoreMessages.end())
    return VK_FALSE;
  std::ostringstream message;
  message << "DebugUtils Callee :: ";
  // repeating nvprintfLevel to help with breakpoints : so we can selectively break right after the
  // print
  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
   message << ("VERBOSE: %s \n --> %s\n", callbackData->pMessageIdName, callbackData->pMessage);
  }
  else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
     message <<  ("INFO: %s \n --> %s\n", callbackData->pMessageIdName, callbackData->pMessage);
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


#  ifdef _WIN32
  //MessageBoxA(NULL, message.str().c_str(), "Alert", MB_OK);
  std::cout << message.str() << std::endl;
#  else
  std::cout << message.str() << std::endl;
#  endif
  // this seems redundant with the info already in callbackData->pMessage
#  if 0

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
#  endif
  // Don't bail out, but keep going.
  return VK_FALSE;
}



static VkDebugReportCallbackEXT debug_report_callback;
static VkResult CreateDebugReport(VkInstance instance, VkDebugReportFlagsEXT flag)
{
  PFN_vkCreateDebugReportCallbackEXT dbgCreateDebugReportCallback;

  dbgCreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugReportCallbackEXT");
  if (!dbgCreateDebugReportCallback) {
    std::cout << "GetInstanceProcAddr: Unable to find "
                 "vkCreateDebugReportCallbackEXT function."
              << std::endl;
    exit(1);
  }
  /*
  typedef enum VkDebugReportFlagBitsEXT {
    VK_DEBUG_REPORT_INFORMATION_BIT_EXT = 0x00000001,
    VK_DEBUG_REPORT_WARNING_BIT_EXT = 0x00000002,
    VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT = 0x00000004,
    VK_DEBUG_REPORT_ERROR_BIT_EXT = 0x00000008,
    VK_DEBUG_REPORT_DEBUG_BIT_EXT = 0x00000010,
    VK_DEBUG_REPORT_FLAG_BITS_MAX_ENUM_EXT = 0x7FFFFFFF
  } VkDebugReportFlagBitsEXT;
  */
  VkDebugReportCallbackCreateInfoEXT create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
  create_info.pNext = NULL;
  create_info.flags = flag;

  create_info.pfnCallback = dbgReportCB;
  create_info.pUserData = NULL;

  VkResult res = dbgCreateDebugReportCallback(
      instance, &create_info, NULL, &debug_report_callback);
  return res;
}
static VkResult DestroyDebugReport(VkInstance instance)
{
  PFN_vkDestroyDebugReportCallbackEXT dbgDestroyDebugReportCallback = VK_NULL_HANDLE;
  dbgDestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugReportCallbackEXT");
  if (!dbgDestroyDebugReportCallback) {
    std::cout << "GetInstanceProcAddr: Unable to find "
                 "vkDestroyDebugReportCallbackEXT function."
              << std::endl;
    // exit(1);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }

  dbgDestroyDebugReportCallback(instance, debug_report_callback, NULL);
  return VK_SUCCESS;
}

static VkResult CreateDebugUtils(VkInstance instance, VkDebugUtilsMessageSeverityFlagsEXT flag)
{
  deb.dbgIgnoreMessages.clear();
  deb.createDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT");
  deb.destroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

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
        instance, &dbg_messenger_create_info, nullptr, &deb.dbgMessenger);
  }
  return VK_ERROR_UNKNOWN;
}
static VkResult DestroyDebugUtils(VkInstance instance)
{
  if (deb.destroyDebugUtilsMessengerEXT) {
    deb.destroyDebugUtilsMessengerEXT(instance, deb.dbgMessenger, nullptr);
  }
  deb.createDebugUtilsMessengerEXT = nullptr;
  deb.destroyDebugUtilsMessengerEXT = nullptr;
  deb.dbgIgnoreMessages.clear();
  deb.dbgMessenger = nullptr;


  return VK_SUCCESS;
}

#endif

static GHOST_TSuccess CreateDebug(VULKAN_DEBUG_TYPE mode,VkInstance instance)
{
  if (mode == VULKAN_DEBUG_REPORT || mode == VULKAN_DEBUG_REPORT_ALL ||
      mode == VULKAN_DEBUG_BOTH) {
    VkDebugReportFlagBitsEXT flag =
        (VkDebugReportFlagBitsEXT)(VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
                                   VK_DEBUG_REPORT_WARNING_BIT_EXT |
                                   VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
                                   VK_DEBUG_REPORT_ERROR_BIT_EXT);

    if (mode == VULKAN_DEBUG_REPORT_ALL)
      flag = (VkDebugReportFlagBitsEXT)(flag |VK_DEBUG_REPORT_DEBUG_BIT_EXT);
    VK_CHECK(CreateDebugReport(instance, flag));

  }
  if (mode == VULKAN_DEBUG_UTILS || mode == VULKAN_DEBUG_UTILS_ALL || mode == VULKAN_DEBUG_BOTH) {
    VkDebugUtilsMessageSeverityFlagsEXT  flag =  VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    if (mode == VULKAN_DEBUG_UTILS_ALL)
      flag = (VkDebugReportFlagBitsEXT)(flag | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT);
    
    VK_CHECK(CreateDebugUtils(instance,flag));
  
  
  }
 

 return GHOST_kSuccess;
};
static GHOST_TSuccess DestroyDebug(VULKAN_DEBUG_TYPE mode, VkInstance instance)
{
  {
    if (mode == VULKAN_DEBUG_REPORT || mode == VULKAN_DEBUG_BOTH)
      VK_CHECK(DestroyDebugReport(instance));

    if (mode == VULKAN_DEBUG_UTILS || mode == VULKAN_DEBUG_BOTH)
      VK_CHECK(DestroyDebugUtils(instance));

  }
  return GHOST_kSuccess;
};




/* Tripple buffering. */
const int MAX_FRAMES_IN_FLIGHT = 2;

GHOST_ContextVK::GHOST_ContextVK(bool stereoVisual,
#ifdef _WIN32
                                 HWND hwnd,
#elif defined(__APPLE__)
                                 CAMetalLayer *metal_layer,
#else
                                 GHOST_TVulkanPlatformType platform,
                                 /* X11 */
                                 Window window,
                                 Display *display,
                                 /* Wayland */
                                 wl_surface *wayland_surface,
                                 wl_display *wayland_display,
#endif
                                 int contextMajorVersion,
                                 int contextMinorVersion,
                                 int debug
#ifdef VULKAN_ERROR_CHECK
                  ,
                  VULKAN_DEBUG_TYPE dmode
#endif
) : GHOST_Context(stereoVisual),
#ifdef _WIN32
      m_hwnd(hwnd),
#elif defined(__APPLE__)
      m_metal_layer(metal_layer),
#else
      m_platform(platform),
      /* X11 */
      m_display(display),
      m_window(window),
      /* Wayland */
      m_wayland_surface(wayland_surface),
      m_wayland_display(wayland_display),
#endif
      m_context_major_version(contextMajorVersion),
      m_context_minor_version(contextMinorVersion),
      m_debug(debug),
      m_instance(VK_NULL_HANDLE),
      m_physical_device(VK_NULL_HANDLE),
      m_device(VK_NULL_HANDLE),
      m_command_pool(VK_NULL_HANDLE),
      m_surface(VK_NULL_HANDLE),
      m_swapchain(VK_NULL_HANDLE),
      m_render_pass(VK_NULL_HANDLE)
#ifdef VULKAN_ERROR_CHECK
     ,m_debugMode(dmode)
    #endif
{
}

GHOST_ContextVK::~GHOST_ContextVK()
{
  if (m_device) {
    vkDeviceWaitIdle(m_device);
  }

  destroySwapchain();

 #ifdef VULKAN_ERROR_CHECK
  test_destroy(m_device);
#endif


  if (m_command_pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(m_device, m_command_pool, NULL);
  }

  if (m_device != VK_NULL_HANDLE) {
    vkDestroyDevice(m_device, NULL);
  }
  if (m_surface != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(m_instance, m_surface, NULL);
  }


  DestroyDebug(m_debugMode,m_instance);
  if (m_instance != VK_NULL_HANDLE) {
    vkDestroyInstance(m_instance, NULL);
  }
}

GHOST_TSuccess GHOST_ContextVK::getQueue(uint32_t qty,
                                         void *r_queue,
                                         uint32_t *r_graphic_queue_familly)
{
  const uint32_t QTY_GRAPHICS  = 0;
  const uint32_t QTY_PRESENTS  = 1;
  const uint32_t QTY_TRANSFER = 2;

  switch (qty) {
    case QTY_GRAPHICS:
      *((VkQueue *)r_queue) = m_graphic_queue;
      *(r_graphic_queue_familly) = m_queue_family_graphic;
      break;
    case QTY_PRESENTS:
      *((VkQueue *)r_queue) = m_present_queue;
      *(r_graphic_queue_familly) = m_queue_family_present;
      break;
    case QTY_TRANSFER:
      *((VkQueue *)r_queue) = m_transfer_queue;
      *(r_graphic_queue_familly) = m_queue_family_transfer;
      break;
    default:
      fprintf(stderr, "Unreachable NotFound Queue Type.");
      BLI_assert(false);
  }
  return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_ContextVK::destroySwapchain(void)
{
  if (m_device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_device);
  }

  m_in_flight_images.resize(0);

  for (auto semaphore : m_image_available_semaphores) {
    vkDestroySemaphore(m_device, semaphore, NULL);
  }
  for (auto semaphore : m_render_finished_semaphores) {
    vkDestroySemaphore(m_device, semaphore, NULL);
  }
  for (auto fence : m_in_flight_fences) {
    vkDestroyFence(m_device, fence, NULL);
  }
  for (auto framebuffer : m_swapchain_framebuffers) {
    vkDestroyFramebuffer(m_device, framebuffer, NULL);
  }
  if (m_render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(m_device, m_render_pass, NULL);
  }
  for (auto command_buffer : m_command_buffers) {
    vkFreeCommandBuffers(m_device, m_command_pool, 1, &command_buffer);
  }
    /// <summary>
    /// when recreating this swapchain, memoryleak  m_command_pool.
    /// </summary>
    if (m_command_pool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(m_device, m_command_pool, NULL);
      m_command_pool = VK_NULL_HANDLE;
    }

  for (auto imageView : m_swapchain_image_views) {
    vkDestroyImageView(m_device, imageView, NULL);
  }
  if (m_swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(m_device, m_swapchain, NULL);
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::swapBuffers()
{
  if (m_swapchain == VK_NULL_HANDLE) {
    return GHOST_kFailure;
  }

  vkWaitForFences(m_device, 1, &m_in_flight_fences[m_currentFrame], VK_TRUE, UINT64_MAX);

  VkResult result = vkAcquireNextImageKHR(m_device,
                                          m_swapchain,
                                          UINT64_MAX,
                                          m_image_available_semaphores[m_currentFrame],
                                          VK_NULL_HANDLE,
                                          &m_currentImage);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    /* Swapchain is out of date. Recreate swapchain and skip this frame. */
    destroySwapchain();
    createSwapchain();
    return GHOST_kSuccess;
  }
  else if (result != VK_SUCCESS) {
    fprintf(stderr,
            "Error: Failed to acquire swap chain image : %s\n",
            vulkan_error_as_string(result));
    return GHOST_kFailure;
  }

  /* Check if a previous frame is using this image (i.e. there is its fence to wait on) */
  if (m_in_flight_images[m_currentImage] != VK_NULL_HANDLE) {
    vkWaitForFences(m_device, 1, &m_in_flight_images[m_currentImage], VK_TRUE, UINT64_MAX);
  }
  m_in_flight_images[m_currentImage] = m_in_flight_fences[m_currentFrame];

  VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};

  VkSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &m_image_available_semaphores[m_currentFrame];
  submit_info.pWaitDstStageMask = wait_stages;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &m_command_buffers[m_currentImage];
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &m_render_finished_semaphores[m_currentFrame];

  vkResetFences(m_device, 1, &m_in_flight_fences[m_currentFrame]);

  VK_CHECK(vkQueueSubmit(m_graphic_queue, 1, &submit_info, m_in_flight_fences[m_currentFrame]));

  do {
    result = vkWaitForFences(m_device, 1, &m_in_flight_fences[m_currentFrame], VK_TRUE, 10000);
  } while (result == VK_TIMEOUT);

  VK_CHECK(vkQueueWaitIdle(m_graphic_queue));



  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &m_render_finished_semaphores[m_currentFrame];
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &m_swapchain;
  present_info.pImageIndices = &m_currentImage;
  present_info.pResults = NULL;

  result = vkQueuePresentKHR(m_present_queue, &present_info);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    /* Swapchain is out of date. Recreate swapchain and skip this frame. */
    destroySwapchain();
    createSwapchain();
    return GHOST_kSuccess;
  }
  else if (result != VK_SUCCESS) {
    fprintf(stderr,
            "Error: Failed to present swap chain image : %s\n",
            vulkan_error_as_string(result));
    return GHOST_kFailure;
  }





  m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::getVulkanBackbuffer(void *image,
                                                    void *framebuffer,
                                                    void *command_buffer,
                                                    void *render_pass,
                                                    void *extent,
                                                    uint32_t *fb_id)
{
  if (m_swapchain == VK_NULL_HANDLE) {
    return GHOST_kFailure;
  }
  *((VkImage *)image) = m_swapchain_images[m_currentImage];
  *((VkFramebuffer *)framebuffer) = m_swapchain_framebuffers[m_currentImage];
  *((VkCommandBuffer *)command_buffer) = m_command_buffers[m_currentImage];
  *((VkRenderPass *)render_pass) = m_render_pass;
  *((VkExtent2D *)extent) = m_render_extent;
  *fb_id = m_swapchain_id * 10 + m_currentFrame;

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::getVulkanHandles(void *r_instance,
                                                 void *r_physical_device,
                                                 void *r_device,
                                                 uint32_t *r_graphic_queue_familly)
{
  *((VkInstance *)r_instance) = m_instance;
  *((VkPhysicalDevice *)r_physical_device) = m_physical_device;
  *((VkDevice *)r_device) = m_device;
  *r_graphic_queue_familly = m_queue_family_graphic;

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::activateDrawingContext()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::releaseDrawingContext()
{
  return GHOST_kSuccess;
}

static vector<VkExtensionProperties> getExtensionsAvailable()
{
  uint32_t extension_count = 0;
  vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);

  vector<VkExtensionProperties> extensions(extension_count);
  vkEnumerateInstanceExtensionProperties(NULL, &extension_count, extensions.data());

  return extensions;
}

static bool checkExtensionSupport(vector<VkExtensionProperties> &extensions_available,
                                  const char *extension_name)
{
  for (const auto &extension : extensions_available) {
    if (strcmp(extension_name, extension.extensionName) == 0) {
      return true;
    }
  }
  return false;
}

static void requireExtension(vector<VkExtensionProperties> &extensions_available,
                             vector<const char *> &extensions_enabled,
                             const char *extension_name)
{
  if (checkExtensionSupport(extensions_available, extension_name)) {
    extensions_enabled.push_back(extension_name);
  }
  else {
    fprintf(stderr, "Error: %s not found.\n", extension_name);
  }
}

static vector<VkLayerProperties> getLayersAvailable()
{
  uint32_t layer_count = 0;
  vkEnumerateInstanceLayerProperties(&layer_count, NULL);

  vector<VkLayerProperties> layers(layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

  return layers;
}

static bool checkLayerSupport(vector<VkLayerProperties> &layers_available, const char *layer_name)
{
  for (const auto &layer : layers_available) {
    if (strcmp(layer_name, layer.layerName) == 0) {
      return true;
    }
  }
  return false;
}

static void enableLayer(vector<VkLayerProperties> &layers_available,
                        vector<const char *> &layers_enabled,
                        const char *layer_name)
{
  if (checkLayerSupport(layers_available, layer_name)) {
    layers_enabled.push_back(layer_name);
  }
  else {
    fprintf(stderr, "Error: %s not supported.\n", layer_name);
  }
}

static bool device_extensions_support(VkPhysicalDevice device, vector<const char *> required_exts)
{
  uint32_t ext_count;
  vkEnumerateDeviceExtensionProperties(device, NULL, &ext_count, NULL);

  vector<VkExtensionProperties> available_exts(ext_count);
  vkEnumerateDeviceExtensionProperties(device, NULL, &ext_count, available_exts.data());

  for (const auto &extension_needed : required_exts) {
    bool found = false;
    for (const auto &extension : available_exts) {
      if (strcmp(extension_needed, extension.extensionName) == 0) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

GHOST_TSuccess GHOST_ContextVK::pickPhysicalDevice(vector<const char *> required_exts)
{
  m_physical_device = VK_NULL_HANDLE;

  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(m_instance, &device_count, NULL);

  vector<VkPhysicalDevice> physical_devices(device_count);
  vkEnumeratePhysicalDevices(m_instance, &device_count, physical_devices.data());

  int best_device_score = -1;
  for (const auto &physical_device : physical_devices) {
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(physical_device, &device_properties);

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(physical_device, &features);

    DEBUG_PRINTF("%s : \n", device_properties.deviceName);

    if (!device_extensions_support(physical_device, required_exts)) {
      DEBUG_PRINTF("  - Device does not support required device extensions.\n");
      continue;
    }

    if (m_surface != VK_NULL_HANDLE) {
      uint32_t format_count;
      vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, m_surface, &format_count, NULL);

      uint32_t present_count;
      vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, m_surface, &present_count, NULL);

      /* For now anything will do. */
      if (format_count == 0 || present_count == 0) {
        DEBUG_PRINTF("  - Device does not support presentation.\n");
        continue;
      }
    }

    if (!features.geometryShader) {
      /* Needed for wide lines emulation and barycentric coords and a few others. */
      DEBUG_PRINTF("  - Device does not support geometryShader.\n");
    }
    if (!features.dualSrcBlend) {
      DEBUG_PRINTF("  - Device does not support dualSrcBlend.\n");
    }
    if (!features.logicOp) {
      /* Needed by UI. */
      DEBUG_PRINTF("  - Device does not support logicOp.\n");
    }

#if STRICT_REQUIREMENTS
    if (!features.geometryShader || !features.dualSrcBlend || !features.logicOp) {
      continue;
    }
#endif

    int device_score = 0;
    switch (device_properties.deviceType) {
      case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        device_score = 400;
        break;
      case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        device_score = 300;
        break;
      case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        device_score = 200;
        break;
      case VK_PHYSICAL_DEVICE_TYPE_CPU:
        device_score = 100;
        break;
      default:
        break;
    }
    if (device_score > best_device_score) {
      m_physical_device = physical_device;
      best_device_score = device_score;
    }
    DEBUG_PRINTF("  - Device suitable.\n");
  }

  if (m_physical_device == VK_NULL_HANDLE) {
    fprintf(stderr, "Error: No suitable Vulkan Device found!\n");
    return GHOST_kFailure;
  }

  return GHOST_kSuccess;
}

static GHOST_TSuccess getGraphicQueueFamily(VkPhysicalDevice device, uint32_t *r_queue_index)
{
  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);

  vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

  *r_queue_index = 0;
  for (const auto &queue_family : queue_families) {
    if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      return GHOST_kSuccess;
    }
    (*r_queue_index)++;
  }

  fprintf(stderr, "Couldn't find any Graphic queue familly on selected device\n");
  return GHOST_kFailure;
}

static GHOST_TSuccess getTransferQueueFamily(VkPhysicalDevice device, uint32_t *r_queue_index)
{
  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);

  vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

  *r_queue_index = 0;
  for (const auto &queue_family : queue_families) {
    if (   
            ( !(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) )
        &&  ( queue_family.queueFlags &VK_QUEUE_TRANSFER_BIT) 
        )  {
      return GHOST_kSuccess;
    }
    (*r_queue_index)++;
  }

  fprintf(stderr, "Couldn't find any Graphic queue familly on selected device\n");
  return GHOST_kFailure;
}


static GHOST_TSuccess getPresetQueueFamily(VkPhysicalDevice device,
                                           VkSurfaceKHR surface,
                                           uint32_t *r_queue_index)
{
  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);

  vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

  *r_queue_index = 0;
  for (int i = 0; i < queue_family_count; i++) {
    VkBool32 present_support = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, *r_queue_index, surface, &present_support);

    if (present_support) {
      return GHOST_kSuccess;
    }
    (*r_queue_index)++;
  }

  fprintf(stderr, "Couldn't find any Present queue familly on selected device\n");
  return GHOST_kFailure;
}

static GHOST_TSuccess create_render_pass(VkDevice device,
                                         VkFormat format,
                                         VkRenderPass *r_renderPass)
{
  VkAttachmentDescription colorAttachment = {};
  colorAttachment.format = format;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef = {};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;

  VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, NULL, r_renderPass));

  return GHOST_kSuccess;
}

static GHOST_TSuccess selectPresentMode(VkPhysicalDevice device,
                                        VkSurfaceKHR surface,
                                        VkPresentModeKHR *r_presentMode)
{
  uint32_t present_count;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_count, NULL);
  vector<VkPresentModeKHR> presents(present_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_count, presents.data());
  /* MAILBOX is the lowest latency V-Sync enabled mode so use it if available */
  for (auto present_mode : presents) {
    if (present_mode == VK_PRESENT_MODE_FIFO_KHR) {
      *r_presentMode = present_mode;
      return GHOST_kSuccess;
    }
  }
  /* FIFO present mode is always available. */
  for (auto present_mode : presents) {
    if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      *r_presentMode = present_mode;
      return GHOST_kSuccess;
    }
  }

  fprintf(stderr, "Error: FIFO present mode is not supported by the swap chain!\n");

  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextVK::createCommandBuffers(void)
{
  /*
   VUID-vkBeginCommandBuffer-commandBuffer-00050(ERROR / SPEC): 
   msgNum: -1303445259 - Validation Error: [ VUID-vkBeginCommandBuffer-commandBuffer-00050 ] Object 0: handle = 0x20e874b0d10, 
   type = VK_OBJECT_TYPE_COMMAND_BUFFER; 
   Object 1: handle = 0x2e2cd000000002b, type = VK_OBJECT_TYPE_COMMAND_POOL; | MessageID = 0xb24f00f5 | 
   Call to vkBeginCommandBuffer() on VkCommandBuffer 0x20e874b0d10[] attempts to implicitly reset cmdBuffer created from VkCommandPool 0x2e2cd000000002b[] that does NOT have the VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT bit set. 
   The Vulkan spec states: 
   If commandBuffer was allocated from a VkCommandPool which did not have the VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT flag set, 
   commandBuffer must be in the initial state (https://vulkan.lunarg.com/doc/view/1.2.189.0/windows/1.2-extensions/vkspec.html#VUID-vkBeginCommandBuffer-commandBuffer-00050)
  */
  m_command_buffers.resize(m_swapchain_image_views.size());

  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;  //0;
  poolInfo.queueFamilyIndex = m_queue_family_graphic;

  VK_CHECK(vkCreateCommandPool(m_device, &poolInfo, NULL, &m_command_pool));
  printf("======================Device %llx GHOST_ContextVK COMMAND BUFFER   %llx \n",
         (int64_t)m_device,
         (int64_t)m_command_pool);

  VkCommandBufferAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = m_command_pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = static_cast<uint32_t>(m_command_buffers.size());

  VK_CHECK(vkAllocateCommandBuffers(m_device, &alloc_info, m_command_buffers.data()));

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::createSwapchain(void)
{
  m_swapchain_id++;

  VkPhysicalDevice device = m_physical_device;

  uint32_t format_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, NULL);
  vector<VkSurfaceFormatKHR> formats(format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, formats.data());

  /* TODO choose appropriate format. */
  VkSurfaceFormatKHR format = formats[0];

  VkPresentModeKHR present_mode;
  if (!selectPresentMode(device, m_surface, &present_mode)) {
    return GHOST_kFailure;
  }

  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &capabilities);

  m_render_extent = capabilities.currentExtent;
  if (m_render_extent.width == UINT32_MAX) {
    /* Window Manager is going to set the surface size based on the given size.
     * Choose something between minImageExtent and maxImageExtent. */
    m_render_extent.width = 1280;
    m_render_extent.height = 720;
    if (capabilities.minImageExtent.width > m_render_extent.width) {
      m_render_extent.width = capabilities.minImageExtent.width;
    }
    if (capabilities.minImageExtent.height > m_render_extent.height) {
      m_render_extent.height = capabilities.minImageExtent.height;
    }
  }

  /* Driver can stall if only using minimal image count. */
  uint32_t image_count = capabilities.minImageCount;
  /* Note: maxImageCount == 0 means no limit. */
  if (image_count > capabilities.maxImageCount && capabilities.maxImageCount > 0) {
    image_count = capabilities.maxImageCount;
  }

 ///https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkSwapchainCreateInfoKHR.html
  VkSwapchainCreateInfoKHR create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = m_surface;
  create_info.minImageCount = image_count;
  create_info.imageFormat = format.format;
  create_info.imageColorSpace = format.colorSpace;
  create_info.imageExtent = m_render_extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  create_info.preTransform = capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  create_info.clipped = VK_TRUE;
  create_info.oldSwapchain = VK_NULL_HANDLE; /* TODO Window resize */

  uint32_t queueFamilyIndices[] = {m_queue_family_graphic, m_queue_family_present};
  uint32_t queueFamilyIndices2[] = {m_queue_family_graphic};

  if (m_queue_family_graphic != m_queue_family_present) {
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = 2;
    create_info.pQueueFamilyIndices = queueFamilyIndices;
  }
  else {

    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = NULL;

  }

  VK_CHECK(vkCreateSwapchainKHR(m_device, &create_info, NULL, &m_swapchain));

  create_render_pass(m_device, format.format, &m_render_pass);

  /* image_count may not be what we requested! Getter for final value. */
  vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, NULL);
  m_swapchain_images.resize(image_count);
  vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, m_swapchain_images.data());

  m_in_flight_images.resize(image_count, VK_NULL_HANDLE);
  m_swapchain_image_views.resize(image_count);
  m_swapchain_framebuffers.resize(image_count);
  for (int i = 0; i < image_count; i++) {
    VkImageViewCreateInfo view_create_info = {};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = m_swapchain_images[i];
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create_info.format = format.format;
    view_create_info.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
    };
    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(m_device, &view_create_info, NULL, &m_swapchain_image_views[i]));

    VkImageView attachments[] = {m_swapchain_image_views[i]};

    VkFramebufferCreateInfo fb_create_info = {};
    fb_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_create_info.renderPass = m_render_pass;
    fb_create_info.attachmentCount = 1;
    fb_create_info.pAttachments = attachments;
    fb_create_info.width = m_render_extent.width;
    fb_create_info.height = m_render_extent.height;
    fb_create_info.layers = 1;

    VK_CHECK(vkCreateFramebuffer(m_device, &fb_create_info, NULL, &m_swapchain_framebuffers[i]));
  }

  m_image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
  m_render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
  m_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VK_CHECK(vkCreateSemaphore(m_device, &semaphore_info, NULL, &m_image_available_semaphores[i]));
    VK_CHECK(vkCreateSemaphore(m_device, &semaphore_info, NULL, &m_render_finished_semaphores[i]));

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK(vkCreateFence(m_device, &fence_info, NULL, &m_in_flight_fences[i]));
  }

  createCommandBuffers();

  return GHOST_kSuccess;
}

const char *GHOST_ContextVK::getPlatformSpecificSurfaceExtension() const
{
#ifdef _WIN32
  return VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#elif defined(__APPLE__)
  return VK_EXT_METAL_SURFACE_EXTENSION_NAME;
#else /* UNIX/Linux */
  switch (m_platform) {
    case GHOST_kVulkanPlatformX11:
      return VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
      break;
#  ifdef WITH_GHOST_WAYLAND
    case GHOST_kVulkanPlatformWayland:
      return VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
      break;
#  endif
  }
#endif
  return NULL;
}




GHOST_TSuccess GHOST_ContextVK::initializeDrawingContext()
{
#ifdef _WIN32
  const bool use_window_surface = (m_hwnd != NULL);
#elif defined(__APPLE__)
  const bool use_window_surface = (m_metal_layer != NULL);
#else /* UNIX/Linux */
  bool use_window_surface = false;
  switch (m_platform) {
    case GHOST_kVulkanPlatformX11:
      use_window_surface = (m_display != NULL) && (m_window != (Window)NULL);
      break;
#  ifdef WITH_GHOST_WAYLAND
    case GHOST_kVulkanPlatformWayland:
      use_window_surface = (m_wayland_display != NULL) && (m_wayland_surface != NULL);
      break;
#  endif
  }
#endif

  auto layers_available        =  getLayersAvailable();
  auto extensions_available =  getExtensionsAvailable();

  vector<const char *> extensions_device;
  vector<const char *> extensions_enabled;


  vector<const char *> layers_enabled;
  if (m_debug) {
    enableLayer(layers_available, layers_enabled, "VK_LAYER_KHRONOS_validation");

    if ( m_debugMode == VULKAN_DEBUG_UTILS ||  m_debugMode == VULKAN_DEBUG_BOTH)
        requireExtension(extensions_available, extensions_enabled, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (m_debugMode == VULKAN_DEBUG_REPORT || m_debugMode == VULKAN_DEBUG_BOTH)
        requireExtension(extensions_available, extensions_enabled, VK_EXT_DEBUG_REPORT_EXTENSION_NAME);


   requireExtension(extensions_available,
                     extensions_enabled,
                     VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

  }



  if (use_window_surface) {
    const char *native_surface_extension_name = getPlatformSpecificSurfaceExtension();

    requireExtension(extensions_available, extensions_enabled, "VK_KHR_surface");
    requireExtension(extensions_available, extensions_enabled, native_surface_extension_name);
   
    extensions_device.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  }

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Blender";
  app_info.applicationVersion = VK_MAKE_VERSION(3, 4, 0);
  app_info.pEngineName = "Blender";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  //app_info.apiVersion = VK_MAKE_VERSION(m_context_major_version, m_context_minor_version, 0);
  app_info.apiVersion = VK_API_VERSION_1_2;
  

  VkInstanceCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledLayerCount = static_cast<uint32_t>(layers_enabled.size());
  create_info.ppEnabledLayerNames = layers_enabled.data();
  create_info.enabledExtensionCount = static_cast<uint32_t>(extensions_enabled.size());
  create_info.ppEnabledExtensionNames = extensions_enabled.data();

  VK_CHECK(vkCreateInstance(&create_info, NULL, &m_instance));

  if(m_debug) {
       CreateDebug(m_debugMode,m_instance);
  }

  if (use_window_surface) {
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR surface_create_info = {};
    surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.hinstance = GetModuleHandle(NULL);
    surface_create_info.hwnd = m_hwnd;
    VK_CHECK(vkCreateWin32SurfaceKHR(m_instance, &surface_create_info, NULL, &m_surface));
#elif defined(__APPLE__)
    VkMetalSurfaceCreateInfoEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    info.pNext = NULL;
    info.flags = 0;
    info.pLayer = m_metal_layer;
    VK_CHECK(vkCreateMetalSurfaceEXT(m_instance, &info, nullptr, &m_surface));
#else
    switch (m_platform) {
      case GHOST_kVulkanPlatformX11: {
        VkXlibSurfaceCreateInfoKHR surface_create_info = {};
        surface_create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        surface_create_info.dpy = m_display;
        surface_create_info.window = m_window;
        VK_CHECK(vkCreateXlibSurfaceKHR(m_instance, &surface_create_info, NULL, &m_surface));
        break;
      }
#  ifdef WITH_GHOST_WAYLAND
      case GHOST_kVulkanPlatformWayland: {
        VkWaylandSurfaceCreateInfoKHR surface_create_info = {};
        surface_create_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        surface_create_info.display = m_wayland_display;
        surface_create_info.surface = m_wayland_surface;
        VK_CHECK(vkCreateWaylandSurfaceKHR(m_instance, &surface_create_info, NULL, &m_surface));
        break;
      }
#  endif
    }

#endif
  }

  if (!pickPhysicalDevice(extensions_device)) {
    return GHOST_kFailure;
  }

  vector<VkDeviceQueueCreateInfo> queue_create_infos;


    /* A graphic queue is required to draw anything. */
    if (!getGraphicQueueFamily(m_physical_device, &m_queue_family_graphic)) {
      return GHOST_kFailure;
    }

    if (!getPresetQueueFamily(m_physical_device, m_surface, &m_queue_family_present)) {
      return GHOST_kFailure;
    }

    VkDeviceQueueCreateInfo graphic_queue_create_info = {};
    graphic_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphic_queue_create_info.queueFamilyIndex = m_queue_family_graphic;
   
    BLI_assert(m_queue_family_graphic == m_queue_family_present);
    vector<float> queue_priorities;
    queue_priorities.push_back(1.f);
    graphic_queue_create_info.queueCount = 1;

    if (use_window_surface) {
      queue_priorities.push_back(1.f);
      graphic_queue_create_info.queueCount = 2;
    }

      graphic_queue_create_info.pQueuePriorities = queue_priorities.data();
      queue_create_infos.push_back(graphic_queue_create_info);

  /*
  if (use_window_surface) {
    // A present queue is required only if we render to a window.
    if (!getPresetQueueFamily(m_physical_device, m_surface, &m_queue_family_present)) {
      return GHOST_kFailure;
    }

    float queue_priorities[] = {1.0f};
    VkDeviceQueueCreateInfo present_queue_create_info = {};
    present_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    present_queue_create_info.queueFamilyIndex = m_queue_family_present;
    present_queue_create_info.queueCount = 1;
    present_queue_create_info.pQueuePriorities = queue_priorities;

    // Eash queue must be unique. 
    if (m_queue_family_graphic != m_queue_family_present) {
      queue_create_infos.push_back(present_queue_create_info);
    }
  }
  */


    /* A transfer queue is required only if we render to a window. */
    if (!getTransferQueueFamily( m_physical_device,  &m_queue_family_transfer ) ) {
      return GHOST_kFailure;
    }

    float queue_priorities2[] = {1.0f};
    VkDeviceQueueCreateInfo transfer_queue_create_info = {};
    transfer_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    transfer_queue_create_info.queueFamilyIndex = m_queue_family_transfer;
    transfer_queue_create_info.queueCount = 1;
    transfer_queue_create_info.pQueuePriorities = queue_priorities2;

    /* Eash queue must be unique. */
    queue_create_infos.push_back(transfer_queue_create_info);
    



  VkPhysicalDeviceFeatures device_features = {};
#if STRICT_REQUIREMENTS
  device_features.geometryShader = VK_TRUE;
  device_features.dualSrcBlend = VK_TRUE;
  device_features.logicOp = VK_TRUE;
  device_features.depthClamp = VK_TRUE;
#endif
  
  extensions_device.push_back(VK_EXT_BLEND_OPERATION_ADVANCED_EXTENSION_NAME);
  extensions_device.push_back(VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME);

  VkDeviceCreateInfo device_create_info = {};
  device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
  device_create_info.pQueueCreateInfos = queue_create_infos.data();
  /* layers_enabled are the same as instance extensions.
   * This is only needed for 1.0 implementations. */
  device_create_info.enabledLayerCount = static_cast<uint32_t>(layers_enabled.size());
  device_create_info.ppEnabledLayerNames = layers_enabled.data();
  device_create_info.enabledExtensionCount = static_cast<uint32_t>(extensions_device.size());
  device_create_info.ppEnabledExtensionNames = extensions_device.data();
  device_create_info.pEnabledFeatures = &device_features;

  VK_CHECK(vkCreateDevice(m_physical_device, &device_create_info, NULL, &m_device));

  vkGetDeviceQueue(m_device, m_queue_family_graphic, 0, &m_graphic_queue);
  vkGetDeviceQueue(m_device, m_queue_family_transfer, 0, &m_transfer_queue);

  vkGetPhysicalDeviceMemoryProperties(m_physical_device, &m_memoryProperties);
  vkGetPhysicalDeviceMemoryProperties(m_physical_device, &__memoryProperties);

  if (use_window_surface) {
    vkGetDeviceQueue(m_device, m_queue_family_present, 1, &m_present_queue);
    createSwapchain();
  }

  #ifdef VULKAN_ERROR_CHECK
      deb.device = m_device;
     load_VK_EXTENSIONS(m_instance, vkGetInstanceProcAddr, m_device, vkGetDeviceProcAddr);

  #endif
 
  return GHOST_kSuccess;
}

#ifdef VULKAN_ERROR_CHECK

GHOST_TSuccess GHOST_ContextVK::Check_Context(int mode)
{
   
    
    MIBmvk mib;
  if (mode == 0) {
    /// <summary>
    /// Size Error  is triggered.
    /// </summary>
    /// <param name="mode"></param>
    /// <returns></returns>
    mib.size = -1;
    test_createBuffer(*this, mib);
    test_buf.push_back(mib);

  }
  else {
    /// <summary>
    /// Undestroyed Error   is triggered.   ( Validation Error  )
    /// </summary>
    /// <param name="mode"></param>
    /// <returns></returns> MIBmvk mib;
    mib.size = 128;
    test_createBuffer(*this, mib);
    
  }

  if (mib.isValid()) {
    if (m_debugMode == VULKAN_DEBUG_UTILS || m_debugMode == VULKAN_DEBUG_UTILS_ALL ||
        m_debugMode == VULKAN_DEBUG_BOTH) {
      deb.setObjectName(mib.buffer, "TEST_UBO_BUFFER");
    }
  }

  return GHOST_kSuccess;
};
#endif


GHOST_TSuccess GHOST_ContextVK::releaseNativeHandles()
{
  return GHOST_kSuccess;
}


