/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifndef VK_MEMORY_H
#  define VK_MEMORY_H
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>
#include "BLI_map.hh"


#include <vulkan/vk_mem_alloc.h>

namespace blender::gpu {

/**
 * `VK_ALLOCATION_CALLBACKS` initializes allocation callbacks for for host allocations.
 * The macro creates a local static variable with the name `vk_allocation_callbacks`
 * that can be passed to vulkan API functions that expect
 * `const VkAllocationCallbacks *pAllocator`.
 *
 * When Blender is compiled with `WITH_VULKAN_GUARDEDALLOC` this will use
 * `MEM_guardedalloc` for host allocations that the driver does on behalf
 * of blender. More internal allocations are still being allocated via the
 * implemention inside the vulkan device driver.
 *
 * When `WITH_VULKAN_GUARDEDALLOC=Off` the memory allocation implemented
 * in the vulkan device driver is used for both internal and application
 * focussed memory operations.
 */

#  ifdef WITH_VULKAN_GUARDEDALLOC_
VkAllocationCallbacks vk_allocation_callbacks_init(const char *name);

#    define VK_ALLOCATION_CALLBACKS \
      static const VkAllocationCallbacks vk_allocation_callbacks_ = vk_allocation_callbacks_init( \
          __func__); \
      static const VkAllocationCallbacks *vk_allocation_callbacks = &vk_allocation_callbacks_;
#  else
#    define VK_ALLOCATION_CALLBACKS \
      static const VkAllocationCallbacks *vk_allocation_callbacks = nullptr;
#  endif

}  // namespace blender::gpu


/* Debug memory statistics: Disabled by Macro rather than guarded for
 * performance considerations. */
#define VK_DEBUG_MEMORY_STATISTICS 0

/* Allows a scratch buffer to temporarily grow beyond its maximum, which allows submission
 * of one-time-use data packets which are too large. */
#define VK_SCRATCH_BUFFER_ALLOW_TEMPORARY_EXPANSION 1
#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)
#define VK_CHECK(expr) \
  do { \
    if ((expr) < 0) { \
      assert(0 && #expr); \
      throw std::runtime_error(__FILE__ "(" LINE_STRING "): VkResult( " #expr " ) < 0"); \
    } \
  } while (false)

namespace blender::gpu {

 const uint32_t VK_BUFFER_DEFAULT_ALIGNMENT = 256;

 void *GetMappedData(VmaAllocation &allo);
    /* Forward Declarations. */
class  VKContext;
class  VKCommandBufferManager;
class  VKUniformBuf;
class  VKStagingBufferManager;

struct VKResourceOptions {
  uint64_t                       id;
  VkBufferCreateInfo  bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  VmaAllocationCreateInfo allocCreateInfo = {};
  VmaAllocationInfo                       allocInfo;

  VKResourceOptions()
  {
    id = (uint64_t)this;
    bufferInfo.size = 0;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    allocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
  }

  void setHostVisible(int usage = 0)
  {
    ///https:// gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html#usage_patterns_staging_copy_upload
     bufferInfo.usage = (VkBufferUsageFlagBits)(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage) ;
     bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
     allocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
     allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                             VMA_ALLOCATION_CREATE_MAPPED_BIT;
  };
  void setDeviceLocal(int usage = 0)
  {
    bufferInfo.usage = (VkBufferUsageFlagBits)(VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  }

};


class VKBuffer {

 private:

  VkBuffer vk_buffer_ = VK_NULL_HANDLE;
  VmaAllocation allocation = nullptr;
  VKResourceOptions options_;
  bool  can_mapped_ = true;
  char* mapped_ = nullptr;
  VKContext *context_ = nullptr;
  std::string name_; 
 public:


  VKBuffer(uint64_t size, uint alignment, const char *name, VKResourceOptions &options);
  VKBuffer(uint64_t size,
           uint alignment,
           const char *name,
           VmaMemoryUsage usage = VMA_MEMORY_USAGE_GPU_ONLY);
  ~VKBuffer();

  void Create(uint64_t size, uint alignment , VKResourceOptions& options);
  void Resize(VkDeviceSize size, uint alignment = 0);
  void Copy(void *data, VkDeviceSize size, VkDeviceSize ofs = 0);



  void Flush();
  void Fill(uint32_t val);

  VkBuffer get_vk_buffer() const;
  void *get_host_ptr() const;
  void unmap();
  uint64_t get_size() const;
  uint64_t get_buffer_size() const;
  void *get_contents();


  VKResourceOptions get_resource_options() {
    return options_;
  };

  void free();

};


class VKStagingBufferManager {

 private:
  Vector<VkCommandBuffer> cmds_;
  VkCommandPool cmdPool_;
  VkCommandBuffer current_cmd_ = VK_NULL_HANDLE;
  static constexpr uint vk_max_staging_buffers_ = VK_NUM_SAFE_FRAMES;
 public:

  static constexpr uint vk_staging_buffer_max_size_ = 128 * 1024 * 1024;
  static constexpr uint vk_staging_buffer_initial_size_ = 16 * 1024 * 1024;
  static constexpr uint vk_staging_buffer_min_alignment = 256;
 private:
  VKContext &context_;
  bool  initialised_ = false;
  
  uint current_staging_buffer_ = 0;
  VKBuffer *staging_buffers_[vk_max_staging_buffers_];


 enum VK_STGBUFFER_QUEUE_TYPE {
    VK_STGBUFFER_QUEUE_TYPE_GRAPHICS = 0,
    VK_STGBUFFER_QUEUE_TYPE_COMPUTE =1,
    VK_STGBUFFER_QUEUE_TYPE_TRANSFER =2,
  };
  VK_STGBUFFER_QUEUE_TYPE  queue_type_;
 public:

  VKStagingBufferManager(VKContext &context);
  ~VKStagingBufferManager();
  void init();
  void free();

  /* Fetch information about backing MTLBuffer. */
  GHOST_TSuccess createTempCmdBuffer(uint32_t N);
  GHOST_TSuccess createCommandPool();



  VkCommandBuffer begin(int i = 0);

  void end();

  void submit();

  void wait();

  void destroy();



  VKBuffer* Create(uint64_t alloc_size, uint alignment);
  void Copy(VKBuffer& dst, VkBufferCopy vbCopyRegion)
  {
    VKBuffer &  staging = *staging_buffers_[current_staging_buffer_];

    VkDeviceSize  srcsize = staging.get_buffer_size();
    BLI_assert(srcsize >= (vbCopyRegion.srcOffset + vbCopyRegion.size));
    VkDeviceSize dstsize = dst.get_buffer_size();
    BLI_assert(dstsize >= (vbCopyRegion.dstOffset + vbCopyRegion.size));
  
       begin();
       vkCmdCopyBuffer(current_cmd_, staging.get_vk_buffer(), dst.get_vk_buffer(), 1, &vbCopyRegion);
       end();


  }
  void CopyBufferSubData(VKBuffer* read, VKBuffer* write, VkBufferCopy& vbCopyRegion)
  {
    VkBuffer read_buf    = read->get_vk_buffer();
    VkBuffer write_buf   = write->get_vk_buffer();
    BLI_assert(read_buf != VK_NULL_HANDLE);
    BLI_assert(write_buf != VK_NULL_HANDLE);
    VkDeviceSize  dstsize = write->get_buffer_size();
    VkDeviceSize  srcsize = read->get_buffer_size();

    if ((vbCopyRegion.size <= dstsize) && (vbCopyRegion.size <= srcsize)) {
      if (vbCopyRegion.size > 0) {
        begin();
        vkCmdCopyBuffer(current_cmd_, read->get_vk_buffer(), write->get_vk_buffer(), 1, &vbCopyRegion);
        end();
      }
      else {
        BLI_assert(false);
      }
    }
    else {
      BLI_assert(false);
    };

  }
};

}
#endif
