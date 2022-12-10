#include "vk_layout.hh"
#include "vk_context.hh"
#include "intern/GHOST_ContextVK.h"


namespace blender::gpu {

bool  VKGraphicsPipelineStateDescriptor::create_pipeline_cache()
{
  VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
  pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  VK_CHECK2(vkCreatePipelineCache(VK_DEVICE, &pipelineCacheCreateInfo, nullptr, &vkPC));
  return true;
};

VkPipelineCache VKGraphicsPipelineStateDescriptor::get_pipeline_cache()
{
  if (vkPC == VK_NULL_HANDLE)
    create_pipeline_cache();
  return vkPC;
};
void VKGraphicsPipelineStateDescriptor::destroy_pipeline_cache()
{
  if (vkPC != VK_NULL_HANDLE)
    vkDestroyPipelineCache(VK_DEVICE, vkPC, nullptr);
  vkPC = VK_NULL_HANDLE;
}


}  // namespace blender::gpu
