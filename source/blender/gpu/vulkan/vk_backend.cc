/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "gpu_capabilities_private.hh"
#include "gpu_platform_private.hh"

#include "vk_backend.hh"
#include "vk_batch.hh"
#include "vk_context.hh"
#include "vk_descriptor_pools.hh"
#include "vk_drawlist.hh"
#include "vk_fence.hh"
#include "vk_framebuffer.hh"
#include "vk_index_buffer.hh"
#include "vk_memory.hh"
#include "vk_pixel_buffer.hh"
#include "vk_query.hh"
#include "vk_shader.hh"
#include "vk_storage_buffer.hh"
#include "vk_texture.hh"
#include "vk_uniform_buffer.hh"
#include "vk_vertex_buffer.hh"

namespace blender::gpu {

VKContext *VKBackend::gpuctx_ = nullptr;
int VKBackend::context_ref_count_ = 0;
VmaAllocator VKBackend::mem_allocator_ = VK_NULL_HANDLE;
VkDevice VKBackend::mem_device_ = VK_NULL_HANDLE;
VKBackend::VKBackend()
{
  context_ref_count_ = 0;
  VKBackend::init_platform();
}
template<typename T> void VKBackend::desable_gpuctx(VKContext *context, T &descriptor_pools_)
{
  context_ref_count_--;
  if (context == gpuctx_) {
    gpuctx_ = nullptr;
  }

  static Vector<VkDescriptorPool> pools;
  static debug::VKDebuggingTools tools;
  for (auto &pool : descriptor_pools_.pools_get()) {
    pools.append(pool);
  }
  auto &cur_tools = context->debugging_tools_get();
  if (cur_tools.enabled) {
    tools = cur_tools;
  }

  if (context_ref_count_ == 0) {
    VK_ALLOCATION_CALLBACKS
    for (auto &pool : pools) {
      vkDestroyDescriptorPool(context->device_get(), pool, vk_allocation_callbacks);
    }
    VKTexture::samplers_free();
    vmaDestroyAllocator(mem_allocator_);
    mem_allocator_ = VK_NULL_HANDLE;
    debug::destroy_callbacks(context, tools);
    pools.clear();
  }
};

template void VKBackend::desable_gpuctx(VKContext *, VKDescriptorPools &);

void VKBackend::init_platform()
{
  BLI_assert(!GPG.initialized);

  eGPUDeviceType device = GPU_DEVICE_ANY;
  eGPUOSType os = GPU_OS_ANY;
  eGPUDriverType driver = GPU_DRIVER_ANY;
  eGPUSupportLevel support_level = GPU_SUPPORT_LEVEL_SUPPORTED;

#ifdef _WIN32
  os = GPU_OS_WIN;
#elif defined(__APPLE__)
  os = GPU_OS_MAC;
#else
  os = GPU_OS_UNIX;
#endif

  GPG.init(device, os, driver, support_level, GPU_BACKEND_VULKAN, "", "", "");
}

void VKBackend::platform_exit()
{
  BLI_assert(GPG.initialized);
  GPG.clear();
}

void VKBackend::delete_resources()
{
  VKTexture::samplers_free();
}

void VKBackend::samplers_update() {}

void VKBackend::compute_dispatch(int groups_x_len, int groups_y_len, int groups_z_len)
{
  VKContext &context = *VKContext::get();
  VKShader *shader = static_cast<VKShader *>(context.shader);
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  VKPipeline &pipeline = shader->pipeline_get();
  VKDescriptorSetTracker &descriptor_set = pipeline.descriptor_set_get();
  VKPushConstants &push_constants = pipeline.push_constants_get();

  push_constants.update(context);
  descriptor_set.update(context);
  command_buffer.bind(*descriptor_set.active_descriptor_set(),
                      shader->vk_pipeline_layout_get(),
                      VK_PIPELINE_BIND_POINT_COMPUTE);
  command_buffer.dispatch(groups_x_len, groups_y_len, groups_z_len);
}

void VKBackend::compute_dispatch_indirect(StorageBuf * /*indirect_buf*/) {}

VmaAllocator* VKBackend::vma_alloc(VKContext* context)
{
  VK_ALLOCATION_CALLBACKS;
  VmaAllocatorCreateInfo info = {};
  /* Should use same vulkan version as GHOST (1.2), but set to 1.0 as 1.2 requires
    * correct extensions and functions to be found by VMA, which isn't working as expected and
    * requires more research. To continue development we lower the API to version 1.0. */
  mem_device_ = context->device_get();
  info.vulkanApiVersion = VK_API_VERSION_1_0;
  info.physicalDevice = context->physical_device_get();
  info.device = mem_device_;
  info.instance = context->instance_get();
  info.pAllocationCallbacks = vk_allocation_callbacks;
  vmaCreateAllocator(&info, &mem_allocator_);
  return &mem_allocator_;
}

Context *VKBackend::context_alloc(void *ghost_window, void *ghost_context)
{
  VKContext *context = new VKContext(ghost_window, ghost_context);
  context_ref_count_++;
  if (ghost_window) {
    vma_alloc(context);
    VKTexture::samplers_init(context);
    VKBackend::gpuctx_ = context;
  }
  return context;
}

VmaAllocator &VKBackend::mem_allocator_get()
{
  if(!mem_allocator_){
    vma_alloc(VKContext::get());
  }
  return mem_allocator_;
};

bool VKBackend::exist_window()
{
  if(gpuctx_ )
  {
    return  true;
  }
  return false;
}

Batch *VKBackend::batch_alloc()
{
  return new VKBatch();
}

DrawList *VKBackend::drawlist_alloc(int /*list_length*/)
{
  return new VKDrawList();
}

Fence *VKBackend::fence_alloc()
{
  return new VKFence();
}

FrameBuffer *VKBackend::framebuffer_alloc(const char *name)
{
  return new VKFrameBuffer(name);
}

IndexBuf *VKBackend::indexbuf_alloc()
{
  return new VKIndexBuffer();
}

PixelBuffer *VKBackend::pixelbuf_alloc(uint size)
{
  return new VKPixelBuffer(size);
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
  return new VKTexture(name);
}

UniformBuf *VKBackend::uniformbuf_alloc(int size, const char *name)
{
  return new VKUniformBuffer(size, name);
}

StorageBuf *VKBackend::storagebuf_alloc(int size, GPUUsageType usage, const char *name)
{
  return new VKStorageBuffer(size, usage, name);
}

VertBuf *VKBackend::vertbuf_alloc()
{
  return new VKVertexBuffer();
}

void VKBackend::render_begin()
{
  if (gpuctx_) {
    BLI_assert(gpuctx_->validate_frame());
  }
}

void VKBackend::render_end()
{
  /*finalize Queue*/
}

void VKBackend::render_step() {}

shaderc::Compiler &VKBackend::get_shaderc_compiler()
{
  return shaderc_compiler_;
}

bool VKBackend::device_extensions_support( const char * extension_needed,Vector<VkExtensionProperties>& vk_extension_properties_)
{
  
  bool found = false;
  for (const auto &extension : vk_extension_properties_) {
    if (strcmp(extension_needed, extension.extensionName) == 0) {
      found = true;
      break;
    }
  }
  if (!found) {
    return false;
  }
  return true;
}

void VKBackend::capabilities_init(VKContext &context)
{

  VkPhysicalDevice vk_physical_device = context.physical_device_get();
  Vector<VkExtensionProperties> vk_extension_properties_;
  uint32_t ext_count;
  vkEnumerateDeviceExtensionProperties(vk_physical_device, NULL, &ext_count, NULL);
  vk_extension_properties_.resize(ext_count);
  vkEnumerateDeviceExtensionProperties(vk_physical_device, NULL, &ext_count, vk_extension_properties_.data());

  /* Reset all capabilities from previous context. */
  GCaps = {};
  GCaps.compute_shader_support = true;
  GCaps.shader_storage_buffer_objects_support = true;
  GCaps.shader_image_load_store_support = true;
  VKContext::base_instance_support = device_extensions_support(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,vk_extension_properties_);
  
  VkPhysicalDeviceLimits limits = context.physical_device_limits_get();

  // VKContext::max_push_constants_size = limits.maxPushConstantsSize;

  GCaps.max_texture_size = std::max(
      limits.maxImageDimension3D,
      std::max(limits.maxImageDimension1D, limits.maxImageDimension2D));
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
}

bool VKBackend::validate_frame()
{
  if(gpuctx_){
    return gpuctx_->validate_frame();
  }
  return false;
};
}  // namespace blender::gpu
