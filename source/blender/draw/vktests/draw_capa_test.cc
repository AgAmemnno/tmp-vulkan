#ifdef DRAW_GTEST_SUITE
#define DRAW_TESTING_CAPA 1
#include "draw_testing.hh"
#endif



namespace blender{
   
    namespace draw {
   

#ifndef DRAW_GTEST_SUITE
        void GPUTest::test_capabilities()
#else
        void test_capabilities()
#endif
        {

#if 0
            gpu::VKContext* ctx = (gpu::VKContext*)(gpu::Context::get());
            VkPhysicalDevice physical_device = ctx->get_physical_device();


            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(physical_device, &properties);
            VkPhysicalDeviceLimits limits = properties.limits;
            gpu::GCaps.max_texture_size = __max(
                limits.maxImageDimension3D,
                __max(limits.maxImageDimension1D, limits.maxImageDimension2D));

    
            gpu::GCaps.max_texture_3d_size = limits.maxImageDimension3D;
            gpu::GCaps.max_texture_layers = limits.maxImageArrayLayers;
            gpu::GCaps.max_textures = limits.maxDescriptorSetSampledImages;
            gpu::GCaps.max_textures_vert = limits.maxPerStageDescriptorSampledImages;
            gpu::GCaps.max_textures_geom = limits.maxPerStageDescriptorSampledImages;
            gpu::GCaps.max_textures_frag = limits.maxPerStageDescriptorSampledImages;
            gpu::GCaps.max_samplers = limits.maxSamplerAllocationCount;
            for (int i = 0; i < 3; i++) {
                gpu::GCaps.max_work_group_count[i] = limits.maxComputeWorkGroupCount[i];
                gpu::GCaps.max_work_group_size[i] = limits.maxComputeWorkGroupSize[i];
            }
            gpu::GCaps.max_uniforms_vert = limits.maxPerStageDescriptorUniformBuffers;
            gpu::GCaps.max_uniforms_frag = limits.maxPerStageDescriptorUniformBuffers;
            gpu::GCaps.max_batch_indices = limits.maxDrawIndirectCount;
            gpu::GCaps.max_batch_vertices = limits.maxDrawIndexedIndexValue;
            gpu::GCaps.max_vertex_attribs = limits.maxVertexInputAttributes;
            gpu::GCaps.max_varying_floats = limits.maxVertexOutputComponents;
            gpu::GCaps.max_shader_storage_buffer_bindings = limits.maxPerStageDescriptorStorageBuffers;
            gpu::GCaps.max_compute_shader_storage_blocks = limits.maxPerStageDescriptorStorageBuffers;
            gpu::GCaps.extensions_len = 0;
            
#define PRINT_GCAPS(name) printf("GCaps          " #name " =    %d  \n", gpu::GCaps.##name);

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
#endif
        };
   



#ifdef DRAW_GTEST_SUITE
        DRAW_TEST(capabilities)
#endif





};


};

