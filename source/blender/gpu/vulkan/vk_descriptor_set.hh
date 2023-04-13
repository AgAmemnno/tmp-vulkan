/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "gpu_shader_private.hh"

#include "vk_buffer.hh"
#include "vk_common.hh"
#include "vk_resource_tracker.hh"
#include "vk_uniform_buffer.hh"

namespace blender::gpu {
class VKIndexBuffer;
class VKShaderInterface;
class VKStorageBuffer;
class VKTexture;
class VKUniformBuffer;
class VKVertexBuffer;
class VKDescriptorSetTracker;

/**
 * In vulkan shader resources (images and buffers) are grouped in descriptor sets.
 *
 * The resources inside a descriptor set can be updated and bound per set.
 *
 * Currently Blender only supports a single descriptor set per shader, but it is planned to be able
 * to use 2 descriptor sets per shader. One for each #blender::gpu::shader::Frequency.
 */
class VKDescriptorSet : NonCopyable {

 public:
  /**
   * Binding location of a resource in a descriptor set.
   *
   * Locations and bindings are used for different reasons. In the Vulkan backend we use
   * ShaderInput.location to store the descriptor set + the resource binding inside the descriptor
   * set. To ease the development the VKDescriptorSet::Location will be used to hide this
   * confusion.
   *
   * NOTE: [future development] When supporting multiple descriptor sets the encoding/decoding can
   * be centralized here. Location will then also contain the descriptor set index.
   */
  struct Location {
   private:
    /**
     * References to a binding in the descriptor set.
     */
    uint32_t binding;

    Location(uint32_t binding) : binding(binding)
    {
    }

   public:
    Location() = default;

    bool operator==(const Location &other) const
    {
      return binding == other.binding;
    }

    operator uint32_t() const
    {
      return binding;
    }

    friend class VKDescriptorSetTracker;
    friend class VKShaderInterface;
  };

  VkDescriptorPool vk_descriptor_pool_ = VK_NULL_HANDLE;
  VkDescriptorSet vk_descriptor_set_ = VK_NULL_HANDLE;

 public:
  VKDescriptorSet() = default;
  VKDescriptorSet(VkDescriptorPool vk_descriptor_pool, VkDescriptorSet vk_descriptor_set)
      : vk_descriptor_pool_(vk_descriptor_pool), vk_descriptor_set_(vk_descriptor_set)
  {
  }
  VKDescriptorSet(VKDescriptorSet &&other);
  virtual ~VKDescriptorSet();

  VKDescriptorSet &operator=(VKDescriptorSet &&other)
  {
    vk_descriptor_set_ = other.vk_descriptor_set_;
    vk_descriptor_pool_ = other.vk_descriptor_pool_;
    other.mark_freed();
    return *this;
  }

  VkDescriptorSet vk_handle() const
  {
    return vk_descriptor_set_;
  }

  VkDescriptorPool vk_pool_handle() const
  {
    return vk_descriptor_pool_;
  }
  void mark_freed();
};

class VKDescriptorSetTracker : protected VKResourceTracker<VKDescriptorSet> {
  friend class VKDescriptorSet;

 public:
  struct Binding {
    VKDescriptorSet::Location location;
    VkDescriptorType type;

    VkBuffer vk_buffer = VK_NULL_HANDLE;
    VkDeviceSize buffer_size = 0;

    VkImageView vk_image_view = VK_NULL_HANDLE;
    VkSampler      vk_sampler      = VK_NULL_HANDLE;
    Binding()
    {
      location.binding = 0;
    }

    bool is_buffer() const
    {
      return ELEM(type, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    }

    bool is_image() const
    {
      return ELEM(type, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    }

    bool is_texture() const
    {
      return ELEM(type, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    }
  };

 private:
  /** A list of bindings that needs to be updated.*/
  Vector<Binding> bindings_;
  VkDescriptorSetLayout layout_;

 public:
  VKDescriptorSetTracker()
  {
  }

  VKDescriptorSetTracker(VkDescriptorSetLayout layout) : layout_(layout)
  {
  }

  void bind_as_ssbo(VKVertexBuffer &buffer, VKDescriptorSet::Location location);
  void bind_as_ssbo(VKIndexBuffer &buffer, VKDescriptorSet::Location location);
  void bind(VKStorageBuffer &buffer, VKDescriptorSet::Location location);
  void bind(VKUniformBuffer &buffer, VKDescriptorSet::Location location);
  void image_bind(VKTexture &texture, VKDescriptorSet::Location location);
  void texture_bind(VKTexture &texture, VKDescriptorSet::Location location,eGPUSamplerState sampler_type);

  void bindcmd(VKCommandBuffer& command_buffer,VkPipelineLayout pipeline_layout);
  /**
   * Update the descriptor set on the device.
   */
  void update(VKContext &context);

  std::unique_ptr<VKDescriptorSet> &active_descriptor_set()
  {
    return active_resource();
  }

 protected:
  std::unique_ptr<VKDescriptorSet> create_resource(VKContext &context) override;

 private:
  Binding &ensure_location(VKDescriptorSet::Location location);
};

}  // namespace blender::gpu
