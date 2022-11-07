/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_backend.hh"

#include "vk_batch.hh"
#include "vk_context.hh"
#include "vk_drawlist.hh"
#include "vk_framebuffer.hh"
#include "vk_index_buffer.hh"
#include "vk_vertex_buffer.hh"
#include "vk_query.hh"
#include "gpu_platform_private.hh"

namespace blender::gpu {
VKBackend::~VKBackend()
{
  GPG.clear();
}
 



void VKBackend::delete_resources()
{
}

void VKBackend::samplers_update()
{
}

void VKBackend::compute_dispatch(int /*groups_x_len*/, int /*groups_y_len*/, int /*groups_z_len*/)
{
}

void VKBackend::compute_dispatch_indirect(StorageBuf * /*indirect_buf*/)
{
}

Context *VKBackend::context_alloc(void *ghost_window, void *ghost_context)
{
  return new VKContext(ghost_window, ghost_context);
}

Batch *VKBackend::batch_alloc()
{
  return new VKBatch();
}

DrawList *VKBackend::drawlist_alloc(int /*list_length*/)
{
  return new VKDrawList();
}

FrameBuffer *VKBackend::framebuffer_alloc(const char *name)
{
  return new VKFrameBuffer(name);
}

IndexBuf *VKBackend::indexbuf_alloc()
{
  return new VKIndexBuffer();
}

QueryPool *VKBackend::querypool_alloc()
{
  return new VKQueryPool();
}

Shader *VKBackend::shader_alloc(const char * /*name*/)
{
  return nullptr;
}

Texture *VKBackend::texture_alloc(const char * /*name*/)
{
  return nullptr;
}

UniformBuf *VKBackend::uniformbuf_alloc(int /*size*/, const char * /*name*/)
{
  return nullptr;
}

StorageBuf *VKBackend::storagebuf_alloc(int /*size*/,
                                        GPUUsageType /*usage*/,
                                        const char * /*name*/)
{
  return nullptr;
}

VertBuf *VKBackend::vertbuf_alloc()
{
  return new VKVertBuf();
}
void VKBackend::render_begin()
{
}

void VKBackend::render_end()
{
}

void VKBackend::render_step()
{
}





void VKBackend::platform_init(VKContext *ctx)
{

   BLI_assert(!GPG.initialized);

  GHOST_ContextVK *ctxVk = ctx->ghost_context_;
   VkInstance instance;
  VkPhysicalDevice physical_device;
   VkDevice device;
  uint32_t r_graphic_queue_familly;
   ctxVk->getVulkanHandles(&instance, &physical_device, &device, &r_graphic_queue_familly);

  VkPhysicalDeviceProperties properties;
 
  vkGetPhysicalDeviceProperties(physical_device, &properties);

   
  VkPhysicalDeviceShaderSMBuiltinsFeaturesNV Vkpdss = {};
  Vkpdss.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_FEATURES_NV;
  Vkpdss.pNext = NULL;

  VkPhysicalDeviceFeatures fet = {};
  VkPhysicalDeviceFeatures2 fet2 = {};
  fet2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  fet2.pNext = &Vkpdss;
  fet2.features = fet;
  vkGetPhysicalDeviceFeatures2(physical_device, &fet2);

  char apiversion[16];
  sprintf(apiversion,
          "%d.%d",
          VK_API_VERSION_MAJOR(properties.apiVersion),
          VK_API_VERSION_MINOR(properties.apiVersion));

  const char *vendor = properties.deviceName;
  const char *renderer = "Vulkan API ";
  const char *version = apiversion;
  GPU_VK_DEBUG_PRINTF("Vulkan API - DETECTED GPU: %s  API %s \n", vendor, version);


  eGPUDeviceType edevice = GPU_DEVICE_UNKNOWN;
  eGPUOSType eos = GPU_OS_ANY;
  eGPUDriverType edriver = GPU_DRIVER_ANY;
  eGPUSupportLevel esupport_level = GPU_SUPPORT_LEVEL_SUPPORTED;

    if (!vendor) {
    printf("Warning: No OpenGL vendor detected.\n");
    edevice = GPU_DEVICE_UNKNOWN;
    edriver = GPU_DRIVER_ANY;
  }
  else if (strstr(vendor, "ATI") || strstr(vendor, "AMD")) {
    edevice = GPU_DEVICE_ATI;
    edriver = GPU_DRIVER_OFFICIAL;
  }
  else if (strstr(vendor, "NVIDIA")) {
    edevice = GPU_DEVICE_NVIDIA;
    edriver = GPU_DRIVER_OFFICIAL;
  }
  else if (strstr(vendor, "Intel") ||
           /* src/mesa/drivers/dri/intel/intel_context.c */
            strstr(renderer, "Mesa DRI Intel") || strstr(renderer, "Mesa DRI Mobile Intel")) {
                                      edevice = GPU_DEVICE_INTEL;
                                      edriver = GPU_DRIVER_OFFICIAL;
                                      if (strstr(renderer, "UHD Graphics") ||
                                          /* Not UHD but affected by the same bugs. */
                                          strstr(renderer, "HD Graphics 530") || strstr(renderer, "Kaby Lake GT2") ||
                                          strstr(renderer, "Whiskey Lake")) {
                                        edevice |= GPU_DEVICE_INTEL_UHD;
                                      }
  }else if (strstr(renderer, "Mesa DRI R") ||
           (strstr(renderer, "Radeon") && strstr(vendor, "X.Org")) ||
           (strstr(renderer, "AMD") && strstr(vendor, "X.Org")) ||
           (strstr(renderer, "Gallium ") && strstr(renderer, " on ATI ")) ||
           (strstr(renderer, "Gallium ") && strstr(renderer, " on AMD "))) {
    edevice = GPU_DEVICE_ATI;
    edriver = GPU_DRIVER_OPENSOURCE;
  }
  else if (strstr(renderer, "Nouveau") || strstr(vendor, "nouveau")) {
    edevice = GPU_DEVICE_NVIDIA;
    edriver = GPU_DRIVER_OPENSOURCE;
  }
  else if (strstr(vendor, "Mesa")) {
    edevice = GPU_DEVICE_SOFTWARE;
    edriver = GPU_DRIVER_SOFTWARE;
  }
  else if (strstr(vendor, "Microsoft")) {
    edevice = GPU_DEVICE_SOFTWARE;
    edriver = GPU_DRIVER_SOFTWARE;
  }
  else if (strstr(vendor, "Apple")) {
    /* Apple Silicon. */
    edevice = GPU_DEVICE_APPLE;
    edriver = GPU_DRIVER_OFFICIAL;
  }
  else if (strstr(renderer, "Apple Software Renderer")) {
    edevice = GPU_DEVICE_SOFTWARE;
    edriver = GPU_DRIVER_SOFTWARE;
  }
  else if (strstr(renderer, "llvmpipe") || strstr(renderer, "softpipe")) {
    edevice = GPU_DEVICE_SOFTWARE;
    edriver = GPU_DRIVER_SOFTWARE;
  }
  else {
    printf("Warning: Could not find a matching GPU name. Things may not behave as expected.\n");
    printf("Detected OpenGL configuration:\n");
    printf("Vendor: %s\n", vendor);
    printf("Renderer: %s\n", renderer);
  }


/* macOS is the only supported platform, but check to ensure we are not building with Metal
   * enablement on another platform. */
#ifdef _WIN32
  eos = GPU_OS_WIN;
#else
  fprintf(stderr, "not found os implemented.");
  BLI_assert(false);
#endif

  GPG.init(edevice, eos, edriver, esupport_level, GPU_BACKEND_METAL, vendor, renderer, version);



  
  VkPhysicalDeviceProperties2 properties2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT VkpdBOAprop{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_PROPERTIES_EXT};
  properties2.pNext = &VkpdBOAprop;
    vkGetPhysicalDeviceProperties2(
      physical_device, &properties2);

  BLI_assert(VkpdBOAprop.advancedBlendAllOperations);



}

}  // namespace blender::gpu
