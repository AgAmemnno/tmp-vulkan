/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "GHOST_C-api.h"
#include "gpu_capabilities_private.hh"
#include "gpu_platform_private.hh"

#include "vk_backend.hh"
#include "vk_batch.hh"
#include "vk_context.hh"
#include "vk_drawlist.hh"
#include "vk_framebuffer.hh"
#include "vk_index_buffer.hh"
#include "vk_query.hh"
#include "vk_shader.hh"
#include "vk_texture.hh"
#include "vk_uniform_buffer.hh"
#include "vk_vertex_buffer.hh"

namespace blender::gpu {

namespace vulkan {

static VkPhysicalDeviceProperties properties;
VkPhysicalDeviceProperties &getProperties()
{
  /// BLI_assert(GPG.initialized);
  return properties;
};

};  // namespace vulkan
VKBackend::VKBackend()
{
  ofs_context_ = context_ = nullptr;
};

VKBackend::~VKBackend()
{
  GPG.clear();
}

void VKBackend::delete_resources()
{
  // VKContext::destroyMemAllocator();
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
  if (ghost_window) {
    BLI_assert(context_ == nullptr);
    context_ = new VKContext(ghost_window, ghost_context, shared_orphan_list_);
    return context_;
  }
  else {
    BLI_assert(ofs_context_ == nullptr);
    ofs_context_ = new VKContext(ghost_window, ghost_context, shared_orphan_list_);
  }
  return ofs_context_;
}

Batch *VKBackend::batch_alloc()
{
  return new VKBatch();
}

DrawList *VKBackend::drawlist_alloc(int list_length)
{
  return new VKDrawList(list_length);
}

FrameBuffer *VKBackend::framebuffer_alloc(const char *name)
{
  VKContext *vk_ctx = static_cast<VKContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(vk_ctx);
  return new VKFrameBuffer(name, vk_ctx);
}

IndexBuf *VKBackend::indexbuf_alloc()
{
  return new VKIndexBuf();
}

QueryPool *VKBackend::querypool_alloc()
{
  return new VKQueryPool();
}

Shader *VKBackend::shader_alloc(const char *name)
{
  return new VKShader(name);
}

Texture *VKBackend::texture_alloc(const char *name)
{
  VKContext *vk_ctx = static_cast<VKContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(vk_ctx);
  return new VKTexture(name, vk_ctx);
}

UniformBuf *VKBackend::uniformbuf_alloc(int size, const char *name)
{
  return new VKUniformBuf(size, name);
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

  VKContext *vk_ctx = static_cast<VKContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(vk_ctx);
  if (vk_ctx) {
    if (vk_ctx->is_swapchain_) {
      vk_ctx->begin_frame();
    }
  }
  else {
    BLI_assert_msg(false, "Context lost.");
  }
};

void VKBackend::render_end()
{

  VKContext *vk_ctx = static_cast<VKContext *>(unwrap(GPU_context_active_get()));

  if (vk_ctx) {
    /*BLI_assert(vk_ctx);*/
    if (vk_ctx->is_swapchain_) {
      vk_ctx->end_frame();
    }
  }
}

void VKBackend::render_step()
{
}

void VKBackend::platform_init(VKContext *ctx)
{

  if (GPG.initialized)
    return;
  // BLI_assert(!GPG.initialized);


  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  uint32_t r_graphic_queue_familly;

  GHOST_GetVulkanHandles(GHOST_ContextHandle(ctx->ghost_context_),
                         &instance,
                         &physical_device,
                         &device,
                         &r_graphic_queue_familly);

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
  }
  else if (strstr(renderer, "Mesa DRI R") ||
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
  vkGetPhysicalDeviceProperties2(physical_device, &properties2);

  BLI_assert(VkpdBOAprop.advancedBlendAllOperations);
}
template<typename T> static void get_properties2(VkPhysicalDevice physical_device, T &strct)
{

  VkPhysicalDeviceInlineUniformBlockPropertiesEXT prop_inline_ubo = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT};
  VkPhysicalDeviceProperties2 properties2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  properties2.pNext = &strct;

  vkGetPhysicalDeviceProperties2(physical_device, &properties2);
};

float VKContext::derivative_signs[2] = {1.0f, 1.0f};
uint32_t VKContext::max_geometry_shader_invocations = 0;

void VKBackend::capabilities_init(VKContext *ctx)
{
  /* If we assume multi-physical devices for multi-contexts, capabilities cannot be static. */

  if (GPG.initialized)
    return;

  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  uint32_t r_graphic_queue_familly;

  GHOST_GetVulkanHandles(GHOST_ContextHandle(ctx->ghost_context_),
                         &instance,
                         &physical_device,
                         &device,
                         &r_graphic_queue_familly);

  VkPhysicalDeviceFeatures device_features = {};
  vkGetPhysicalDeviceFeatures(physical_device, &device_features);
  VKContext::multi_draw_indirect_support = false;
  if (device_features.multiDrawIndirect) {
    VKContext::multi_draw_indirect_support = true;
  }

  VkPhysicalDeviceProperties &properties = vulkan::properties;
  vkGetPhysicalDeviceProperties(physical_device, &properties);
  VkPhysicalDeviceLimits limits = properties.limits;
  VkPhysicalDeviceInlineUniformBlockPropertiesEXT prop_inline_ubo = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT};
  get_properties2(physical_device, prop_inline_ubo);

  /* https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_vertex_attribute_divisor.html
   */
  VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT prop_va_divisor = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT};
  get_properties2(physical_device, prop_va_divisor);
  if (prop_va_divisor.maxVertexAttribDivisor > 0) {
    VKContext::vertex_attrib_binding_support = true;
  }

  VKContext::max_inline_ubo_size = prop_inline_ubo.maxInlineUniformBlockSize;
  VKContext::max_push_constants_size = limits.maxPushConstantsSize;

  GCaps.max_texture_size = __max(limits.maxImageDimension3D,
                               __max(limits.maxImageDimension1D, limits.maxImageDimension2D));
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

  /*GL_MAX_UNIFORM_BLOCK_SIZE*/
  VKContext::max_ubo_size = limits.maxUniformBufferRange;
  /*GL_MAX_FRAGMENT_UNIFORM_BLOCKS*/
  VKContext::max_ubo_binds = limits.maxPerStageDescriptorUniformBuffers;
  VKContext::max_geometry_shader_invocations = limits.maxGeometryShaderInvocations;

#if 0
  const char *version = (const char *)glGetString(GL_VERSION);
    /* dFdx/dFdy calculation factors, those are dependent on driver. */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY) &&
      strstr(version, "3.3.10750")) {
    VKContext::derivative_signs[0] = 1.0;
    VKContext::derivative_signs[1] = -1.0;
  }
  else if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_WIN, GPU_DRIVER_ANY)) {
    if (strstr(version, "4.0.0 - Build 10.18.10.3308") ||
        strstr(version, "4.0.0 - Build 9.18.10.3186") ||
        strstr(version, "4.0.0 - Build 9.18.10.3165") ||
        strstr(version, "3.1.0 - Build 9.17.10.3347") ||
        strstr(version, "3.1.0 - Build 9.17.10.4101") ||
        strstr(version, "3.3.0 - Build 8.15.10.2618")) {
      VKContext::derivative_signs[0] = -1.0;
      VKContext::derivative_signs[1] = 1.0;
    }
  }
#endif
/* TODO */
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
