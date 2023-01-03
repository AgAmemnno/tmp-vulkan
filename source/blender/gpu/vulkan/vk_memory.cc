
/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_global.h"

#include "DNA_userdef_types.h"

#include "vk_context.hh"
#include "vk_debug.hh"
#include "vk_memory.hh"

#define VMA_IMPLEMENTATION
#if NDEBUG
#pragma warning( push )
#pragma warning( disable : 4189 )
#include <vk_mem_alloc.h>
#pragma warning( pop )
#else
#include <vk_mem_alloc.h>
#endif

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)
#define ERR_GUARD_VULKAN(expr) \
  do { \
    if ((expr) < 0) { \
      assert(0 && #expr); \
      throw std::runtime_error(__FILE__ "(" LINE_STRING "): VkResult( " #expr " ) < 0"); \
    } \
  } while (false)


namespace blender::gpu {

  void *GetMappedData(VmaAllocation &allo)
{
   return allo->GetMappedData();
}



static int BufferNums = 0;
VKBuffer::VKBuffer(uint64_t size, uint alignment, VmaMemoryUsage usage)
{
  vk_buffer_ = VK_NULL_HANDLE;
  BLI_assert(alignment > 0);
  if (usage == VMA_MEMORY_USAGE_GPU_ONLY)
    options_.setDeviceLocal(0);
  if (usage == VMA_MEMORY_USAGE_CPU_ONLY)
    options_.setHostVisible(0);
  Create(size, alignment, options_);

}

/* Construct a gpu::MTLBuffer wrapper around a newly created metal::MTLBuffer. */
VKBuffer::VKBuffer(uint64_t size, uint alignment, VKResourceOptions& options)
{
  vk_buffer_ = VK_NULL_HANDLE;
  Create(size, alignment, options);

}
gpu::VKBuffer::~VKBuffer()
{
  free();
}
void gpu::VKBuffer::Create(uint64_t size, uint alignment , VKResourceOptions& options)
{
  BLI_assert(alignment > 0);
  BLI_assert(vk_buffer_ == VK_NULL_HANDLE);

  auto Name  = ("VKBuffer_" + std::to_string(BufferNums)).c_str();
 
  options.bufferInfo.size = size;
  options.allocInfo = {};
  options.allocInfo.pName = Name;
  options.allocInfo.offset = 0;
  options.allocInfo.size = size;
  options.allocCreateInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
  
  VmaAllocator mem_allocator = ((VKContext *)Context::get())->mem_allocator_get();

  vmaCreateBufferWithAlignment(mem_allocator,
                               &options.bufferInfo,
                               &options.allocCreateInfo,
                               alignment,
                               &vk_buffer_,
                               &allocation,
                               &options.allocInfo);



  /// <summary>
  /// TODO :: Check Aligment Mismatch  
  /// </summary>
  /*
  printf("AllocationError:: size %u   alignment mismatch    request = %u   valSize = %u  valAli  %u     \n",size,
         alignment,
         allocation->GetSize(),
         allocation->GetAlignment());
 */
  ///BLI_assert(alignment == allocation->GetAlignment());
  BLI_assert(size <= allocation->GetSize());

  options_ = options;
}
void gpu::VKBuffer::free()
{
  VmaAllocator mem_allocator = ((VKContext *)Context::get())->mem_allocator_get();
  auto size = allocation->GetSize();
  if (vk_buffer_ != VK_NULL_HANDLE) {
      vmaDestroyBuffer(mem_allocator, vk_buffer_, allocation);
     vk_buffer_ = VK_NULL_HANDLE;
     size = 0;
  };

 
}
void gpu::VKBuffer::Flush()
{
  VmaAllocator mem_allocator = ((VKContext *)Context::get())->mem_allocator_get();
  VK_CHECK(vmaFlushAllocation(mem_allocator, allocation, 0, VK_WHOLE_SIZE));

}
void gpu::VKBuffer::Resize(VkDeviceSize size, uint alignment){

  VmaAllocator mem_allocator = ((VKContext *)Context::get())->mem_allocator_get();
  auto cursize = options_.bufferInfo.size;
  BLI_assert(vk_buffer_ != VK_NULL_HANDLE);
  bool recr = false;
  if (cursize < size) {
    recr = true;
  }
 
  if (recr){
    vmaDestroyBuffer(mem_allocator, vk_buffer_, allocation);
    vk_buffer_ = VK_NULL_HANDLE;
    Create(size, alignment, options_);
  }

};

VkBuffer VKBuffer::get_vk_buffer() const
{
  return vk_buffer_;
}
uint64_t VKBuffer::get_size() const
{
  return allocation->GetSize();
}

uint64_t VKBuffer::get_buffer_size() const {
  return options_.bufferInfo.size;

};
void *VKBuffer::get_host_ptr() const {
  void *mappedData = nullptr;
  VmaAllocator mem_allocator = ((VKContext *)Context::get())->mem_allocator_get();
  VK_CHECK(vmaMapMemory(mem_allocator, allocation, &mappedData));
  return mappedData;
};

void VKBuffer::unmap() const
{
  VmaAllocator mem_allocator = ((VKContext *)Context::get())->mem_allocator_get();
  vmaUnmapMemory(mem_allocator, allocation);
};
void VKBuffer::Copy(void *data, VkDeviceSize size)
{
  VmaAllocator mem_allocator = ((VKContext *)Context::get())->mem_allocator_get();

  void *mappedData = nullptr;
  VK_CHECK(vmaMapMemory(mem_allocator, allocation, &mappedData));
  memcpy(mappedData, &data, size);
  vmaUnmapMemory(mem_allocator, allocation);
}
VKStagingBufferManager::~VKStagingBufferManager()
{
  destroy();
  this->free();
};

VKStagingBufferManager::VKStagingBufferManager(VKContext &context) : context_(context)
{
  for (int sb = 0; sb < vk_max_staging_buffers_; sb++) {
    staging_buffers_[sb] = nullptr;
  }
  current_cmd_ = VK_NULL_HANDLE;
  queue_type_ = VK_STGBUFFER_QUEUE_TYPE_GRAPHICS;
  createCommandPool();
  createTempCmdBuffer(1);
};
void VKStagingBufferManager::init()
{
 
  if (!this->initialised_) {
    for (int sb = 0; sb < vk_max_staging_buffers_; sb++) {
      staging_buffers_[sb] = new VKBuffer(vk_staging_buffer_initial_size_,vk_staging_buffer_min_alignment,VMA_MEMORY_USAGE_CPU_ONLY);
    }
    current_staging_buffer_ = 0;
    initialised_ = true;
  }

}

void VKStagingBufferManager::free()
{
  initialised_ = false;

  /* Release staging buffers */
  for (int sb = 0; sb < vk_max_staging_buffers_; sb++) {
    if (staging_buffers_[sb]) {
      if (staging_buffers_[sb]) {
        delete staging_buffers_[sb];
        staging_buffers_[sb] = nullptr;
      }
    };
  }
  current_staging_buffer_ = 0;
}

  GHOST_TSuccess VKStagingBufferManager::createTempCmdBuffer(uint32_t N)
{

  auto device = context_.device_get();
  BLI_assert(cmds_.size() == 0);
  cmds_.resize(N);
  VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
  commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandPool = cmdPool_;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  commandBufferAllocateInfo.commandBufferCount = N;

  VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, cmds_.data()));
  current_cmd_ = VK_NULL_HANDLE;

  return GHOST_kSuccess;
}
GHOST_TSuccess VKStagingBufferManager::createCommandPool()
{

  VkCommandPoolCreateInfo cmdPoolInfo = {};
  cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmdPoolInfo.queueFamilyIndex = context_.get_graphicQueueIndex();
  cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  VK_CHECK(vkCreateCommandPool(context_.device_get(), &cmdPoolInfo, nullptr, &cmdPool_));

  return GHOST_kSuccess;
};

VkCommandBuffer VKStagingBufferManager::begin(int i)
{
  BLI_assert(current_cmd_ == VK_NULL_HANDLE);
  BLI_assert(cmds_[i] != VK_NULL_HANDLE);
  /*
  typedef enum VkCommandBufferUsageFlagBits {
    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = 0x00000001,
    VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT = 0x00000002,
    VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT = 0x00000004,
  } VkCommandBufferUsageFlagBits;
  */

  current_cmd_ = cmds_[i];
  static VkCommandBufferBeginInfo cmdBufInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  cmdBufInfo.pInheritanceInfo = NULL;
  VK_CHECK(vkBeginCommandBuffer(current_cmd_, &cmdBufInfo));
  return current_cmd_;
};

void VKStagingBufferManager::end()
{

  VK_CHECK(vkEndCommandBuffer(current_cmd_));
  submit();
  wait();
  current_cmd_ = VK_NULL_HANDLE;
};

void VKStagingBufferManager::submit()
{
  VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &current_cmd_;
  VK_CHECK(vkQueueSubmit(context_.queue_get(queue_type_), 1, &submitInfo, VK_NULL_HANDLE));
};

void VKStagingBufferManager::wait()
{
  VK_CHECK(vkQueueWaitIdle(context_.queue_get(queue_type_)));
}

void VKStagingBufferManager::destroy()
{
  /// <summary>
  /// Create flag  reset or free
  /// </summary>
  vkFreeCommandBuffers(context_.device_get(), cmdPool_, cmds_.size(), cmds_.data());
  cmds_.clear();
  if (cmdPool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(context_.device_get(), cmdPool_, nullptr);
    cmdPool_ = VK_NULL_HANDLE;
  }
};



VKBuffer* VKStagingBufferManager::Create( uint64_t alloc_size, uint alignment)
{
  /* Ensure staging buffer allocation alignment adheres to offset alignment requirements. */

  BLI_assert_msg(current_staging_buffer_ >= 0, "staging Buffer index not set");
  VKBuffer *current_staging_buff = this->staging_buffers_[current_staging_buffer_];
  if (current_staging_buff == nullptr) {
    init();
    current_staging_buff = this->staging_buffers_[current_staging_buffer_];
  }

  BLI_assert_msg(current_staging_buff != nullptr, "staging Buffer does not exist");


  alignment = max_uu(alignment, 256);
  current_staging_buff->Resize(alloc_size,alignment);

  BLI_assert(current_staging_buff->get_vk_buffer() != VK_NULL_HANDLE);
  BLI_assert(current_staging_buff->get_size() >= alloc_size);


  return current_staging_buff;
}

void *VKBuffer::get_contents()
{
  return allocation->GetMappedData();
};

}  // blender::gpu



