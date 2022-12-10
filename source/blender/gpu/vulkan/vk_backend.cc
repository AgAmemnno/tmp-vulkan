/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_backend.hh"
#include "vk_batch.hh"
#include "vk_drawlist.hh"
#include "vk_framebuffer.hh"
#include "vk_index_buffer.hh"
#include "vk_vertex_buffer.hh"
#include "vk_query.hh"
#include "vk_shader.hh"
#include "vk_texture.hh"
#include "vk_context.hh"
#include "gpu_platform_private.hh"
#include "gpu_capabilities_private.hh"
#include "intern/GHOST_ContextVK.h"

namespace blender::gpu {

  namespace vulkan {

static VkPhysicalDeviceProperties properties;
VkPhysicalDeviceProperties& getProperties() {
  ///BLI_assert(GPG.initialized);
  return properties;
};

};
 

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
  return new VKContext(ghost_window, ghost_context, shared_orphan_list_);
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

Shader *VKBackend::shader_alloc(const char * name)
{
  return new VKShader(name);
}

Texture *VKBackend::texture_alloc(const char * name)
{
  return new VKTexture(name,VKContext::get());
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

  GHOST_ContextVK *ctxVk = (GHOST_ContextVK *)ctx->ghost_context_;
   VkInstance instance;
  VkPhysicalDevice physical_device;
   VkDevice device;
  uint32_t r_graphic_queue_familly;
   ctxVk->getVulkanHandles(&instance, &physical_device, &device, &r_graphic_queue_familly);


  auto &properties = vulkan::properties;
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

void VKBackend::capabilities_init(VKContext *ctx)
{
  
   GHOST_ContextVK *ctxVk = (GHOST_ContextVK *)( ctx->ghost_context_);
   VkInstance instance;
   VkPhysicalDevice physical_device;
   VkDevice device;
   uint32_t r_graphic_queue_familly;
   ctxVk->getVulkanHandles(&instance, &physical_device, &device, &r_graphic_queue_familly);
   VkPhysicalDeviceProperties &properties = vulkan::properties;
  vkGetPhysicalDeviceProperties(physical_device, &properties);
   VkPhysicalDeviceLimits limits = properties.limits;
  GCaps.max_texture_size = max(
      limits.maxImageDimension3D,
      max(limits.maxImageDimension1D, limits.maxImageDimension2D));
  GCaps.max_texture_3d_size = limits.maxImageDimension3D;
  GCaps.max_texture_layers = limits.maxImageArrayLayers;
  GCaps.max_textures = limits.maxDescriptorSetSampledImages;
  GCaps.max_textures_vert = limits.maxPerStageDescriptorSampledImages;
  GCaps.max_textures_geom = limits.maxPerStageDescriptorSampledImages;
  GCaps.max_textures_frag = limits.maxPerStageDescriptorSampledImages;
  GCaps.max_samplers = limits.maxSamplerAllocationCount;
  for (int i = 0; i < 3; i++) {
    GCaps.max_work_group_count[i] = limits.maxComputeWorkGroupCount[i];
    GCaps.max_work_group_size[i] = limits.maxComputeWorkGroupSize[i];
  }
  GCaps.max_uniforms_vert = limits.maxPerStageDescriptorUniformBuffers;
  GCaps.max_uniforms_frag = limits.maxPerStageDescriptorUniformBuffers;
  GCaps.max_batch_indices = limits.maxDrawIndirectCount;
  GCaps.max_batch_vertices = limits.maxDrawIndexedIndexValue;
  GCaps.max_vertex_attribs = limits.maxVertexInputAttributes;
  GCaps.max_varying_floats = limits.maxVertexOutputComponents;
  GCaps.max_shader_storage_buffer_bindings = limits.maxPerStageDescriptorStorageBuffers;
  GCaps.max_compute_shader_storage_blocks = limits.maxPerStageDescriptorStorageBuffers;
  ///TODO 
  #if 0 

  glGetIntegerv(GL_NUM_EXTENSIONS, &GCaps.extensions_len);
  GCaps.extension_get = gl_extension_get;

  GCaps.max_samplers = GCaps.max_textures;
  GCaps.mem_stats_support = epoxy_has_gl_extension("GL_NVX_gpu_memory_info") ||
                            epoxy_has_gl_extension("GL_ATI_meminfo");
  GCaps.shader_image_load_store_support = epoxy_has_gl_extension("GL_ARB_shader_image_load_store");
  GCaps.shader_draw_parameters_support = epoxy_has_gl_extension("GL_ARB_shader_draw_parameters");
  GCaps.compute_shader_support = epoxy_has_gl_extension("GL_ARB_compute_shader") &&
                                 epoxy_gl_version() >= 43;
  GCaps.max_samplers = GCaps.max_textures;

  if (GCaps.compute_shader_support) {
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &GCaps.max_work_group_count[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &GCaps.max_work_group_count[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &GCaps.max_work_group_count[2]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &GCaps.max_work_group_size[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &GCaps.max_work_group_size[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &GCaps.max_work_group_size[2]);
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,
                  &GCaps.max_shader_storage_buffer_bindings);
    glGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, &GCaps.max_compute_shader_storage_blocks);
  }
  GCaps.shader_storage_buffer_objects_support = epoxy_has_gl_extension(
      "GL_ARB_shader_storage_buffer_object");
  GCaps.transform_feedback_support = true;

  /* GL specific capabilities. */
  glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &GCaps.max_texture_3d_size);
  glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &GLContext::max_cubemap_size);
  glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &GLContext::max_ubo_binds);
  glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &GLContext::max_ubo_size);
  if (GCaps.shader_storage_buffer_objects_support) {
    GLint max_ssbo_binds;
    GLContext::max_ssbo_binds = 999999;
    glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &max_ssbo_binds);
    GLContext::max_ssbo_binds = min_ii(GLContext::max_ssbo_binds, max_ssbo_binds);
    glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &max_ssbo_binds);
    GLContext::max_ssbo_binds = min_ii(GLContext::max_ssbo_binds, max_ssbo_binds);
    glGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, &max_ssbo_binds);
    GLContext::max_ssbo_binds = min_ii(GLContext::max_ssbo_binds, max_ssbo_binds);
    if (GLContext::max_ssbo_binds < 8) {
      /* Does not meet our minimum requirements. */
      GCaps.shader_storage_buffer_objects_support = false;
    }
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &GLContext::max_ssbo_size);
  }
  GLContext::base_instance_support = epoxy_has_gl_extension("GL_ARB_base_instance");
  GLContext::clear_texture_support = epoxy_has_gl_extension("GL_ARB_clear_texture");
  GLContext::copy_image_support = epoxy_has_gl_extension("GL_ARB_copy_image");
  GLContext::debug_layer_support = epoxy_gl_version() >= 43 ||
                                   epoxy_has_gl_extension("GL_KHR_debug") ||
                                   epoxy_has_gl_extension("GL_ARB_debug_output");
  GLContext::direct_state_access_support = epoxy_has_gl_extension("GL_ARB_direct_state_access");
  GLContext::explicit_location_support = epoxy_gl_version() >= 43;
  GLContext::geometry_shader_invocations = epoxy_has_gl_extension("GL_ARB_gpu_shader5");
  GLContext::fixed_restart_index_support = epoxy_has_gl_extension("GL_ARB_ES3_compatibility");
  GLContext::layered_rendering_support = epoxy_has_gl_extension("GL_AMD_vertex_shader_layer");
  GLContext::native_barycentric_support = epoxy_has_gl_extension(
      "GL_AMD_shader_explicit_vertex_parameter");
  GLContext::multi_bind_support = epoxy_has_gl_extension("GL_ARB_multi_bind");
  GLContext::multi_draw_indirect_support = epoxy_has_gl_extension("GL_ARB_multi_draw_indirect");
  GLContext::shader_draw_parameters_support = epoxy_has_gl_extension(
      "GL_ARB_shader_draw_parameters");
  GLContext::stencil_texturing_support = epoxy_gl_version() >= 43;
  GLContext::texture_cube_map_array_support = epoxy_has_gl_extension(
      "GL_ARB_texture_cube_map_array");
  GLContext::texture_filter_anisotropic_support = epoxy_has_gl_extension(
      "GL_EXT_texture_filter_anisotropic");
  GLContext::texture_gather_support = epoxy_has_gl_extension("GL_ARB_texture_gather");
  GLContext::texture_storage_support = epoxy_gl_version() >= 43;
  GLContext::vertex_attrib_binding_support = epoxy_has_gl_extension(
      "GL_ARB_vertex_attrib_binding");

  detect_workarounds();

  /* Disable this feature entirely when not debugging. */
  if ((G.debug & G_DEBUG_GPU) == 0) {
    GLContext::debug_layer_support = false;
    GLContext::debug_layer_workaround = false;
  }
  #endif
}


}  // namespace blender::gpu
