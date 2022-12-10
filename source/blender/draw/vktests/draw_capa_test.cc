#include "draw_testing.hh"



#include "CLG_log.h"
#include "BKE_global.h"

#include "gpu_capabilities_private.hh"

#include  "vulkan/vk_context.hh"


namespace blender::draw {

void test_capabilities()
{
  using namespace blender::gpu;

  VKContext *ctx = (VKContext *)(Context::get());
  VkPhysicalDevice physical_device = ctx->get_physical_device();


  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physical_device, &properties);
  VkPhysicalDeviceLimits limits = properties.limits;
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
  GCaps.extensions_len = 0;

#define PRINT_GCAPS(name) printf("GCaps          " #name " =    %d  \n", GCaps.##name);

  PRINT_GCAPS(max_texture_size);

  PRINT_GCAPS(max_texture_layers);

  PRINT_GCAPS(max_textures_frag);

  PRINT_GCAPS(max_textures_vert);

  PRINT_GCAPS(max_textures_geom);

  PRINT_GCAPS(max_textures);

  PRINT_GCAPS(max_uniforms_vert);

  PRINT_GCAPS(max_uniforms_frag);

  PRINT_GCAPS(max_batch_indices);

  PRINT_GCAPS(max_batch_vertices);

  PRINT_GCAPS(max_vertex_attribs);
}
DRAW_TEST(capabilities)

};



