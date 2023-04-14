/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#pragma once

#include <optional>

#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "vk_common.hh"
#include "vk_descriptor_set.hh"
#include "vk_pipeline_state.hh"
#include "vk_push_constants.hh"

namespace blender::gpu {
class VKContext;
class VKShader;
class VKVertexAttributeObject;
class VKBatch;

/**
 * Pipeline can be a compute pipeline or a graphic pipeline.
 *
 * Compute pipelines can be constructed early on, but graphics
 * pipelines depends on the actual GPU state/context.
 *
 * - TODO: we should sanitize the interface. There we can also
 *   use late construction for compute pipelines.
 */
class VKPipeline : NonCopyable {
  VkPipeline vk_pipeline_ = VK_NULL_HANDLE;
  VKDescriptorSetTracker descriptor_set_;
  VKPushConstants push_constants_;
  VKPipelineStateManager state_manager_;
  Vector<VkPipeline>    vk_pipelines;
 public:
  VKPipeline() = default;

  virtual ~VKPipeline();
  VKPipeline(VkDescriptorSetLayout vk_descriptor_set_layout, VKPushConstants &&push_constants);
  VKPipeline(VkPipeline vk_pipeline,
             VkDescriptorSetLayout vk_descriptor_set_layout,
             VKPushConstants &&push_constants);
  VKPipeline &operator=(VKPipeline &&other)
  {
    vk_pipeline_ = other.vk_pipeline_;
    other.vk_pipeline_ = VK_NULL_HANDLE;
    descriptor_set_ = std::move(other.descriptor_set_);
    push_constants_ = std::move(other.push_constants_);
    return *this;
  }

  static VKPipeline create_compute_pipeline(VKContext &context,
                                            VkShaderModule compute_module,
                                            VkDescriptorSetLayout &descriptor_set_layout,
                                            VkPipelineLayout &pipeline_layouts,
                                            const VKPushConstants::Layout &push_constants_layout);
  static VKPipeline create_graphics_pipeline(VkDescriptorSetLayout &descriptor_set_layout,
                                             const VKPushConstants::Layout &push_constants_layout);

  VKDescriptorSetTracker &descriptor_set_get()
  {
    return descriptor_set_;
  }

  VKPushConstants &push_constants_get()
  {
    return push_constants_;
  }

  VKPipelineStateManager &state_manager_get()
  {
    return state_manager_;
  }

  VkPipeline vk_handle() const;
  bool is_valid() const;

  void finalize(VKContext &context,
                          VkShaderModule vertex_module,
                          VkShaderModule fragment_module,
                          VkPipelineLayout &pipeline_layout,
                          GPUPrimType topology,
                          const VKVertexAttributeObject &vertex_attribute_object);

  void finalize(VKContext &context,
                VkShaderModule vertex_module,
                VkShaderModule fragment_module,
                VkPipelineLayout &pipeline_layout,
                const VKBatch &batch,
                const VKVertexAttributeObject &vertex_attribute_object);
};

}  // namespace blender::gpu
