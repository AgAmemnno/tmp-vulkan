#ifdef WITH_VULKAN_BACKEND


#  pragma warning(push)
#  pragma warning(disable : 5038 4477 4474 4313 )



#  include "BLI_assert.h"
#include "GHOST_ContextVK.h"

#  include <vulkan/vulkan.h>
#include <unordered_map>
#include <functional>
#include <fstream>
#  define DESC_MAX 300000
typedef std::string_view LayoutType;
struct _PoolSize {
  uint32_t size;
  uint32_t ssbo;
  uint32_t ubo;
  uint32_t tex;
};
#  define GHOST_MAT_PRINTF_ON 1

#  define GHOST_MAT_PRINTF(...) \
    if (GHOST_MAT_PRINTF_ON) { \
      printf(__VA_ARGS__); \
    }

#  define __Delete__(obj) \
    { \
      if (obj != nullptr) { \
        delete obj; \
        obj = nullptr; \
      }; \
    }
const std::string getAssetPath()
{
  return "D:/blender/blender/tests/vulkan_extensions/shaders/";

}
GHOST_TSuccess loadShader(const char *fileName, VkDevice device, VkShaderModule &shaderModule)
{
  std::ifstream is(fileName, std::ios::binary | std::ios::in | std::ios::ate);

  if (is.is_open()) {
    size_t size = is.tellg();
    is.seekg(0, std::ios::beg);
    char *shaderCode = new char[size];
    is.read(shaderCode, size);
    is.close();

    BLI_assert(size > 0);

    VkShaderModuleCreateInfo moduleCreateInfo{};
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.codeSize = size;
    moduleCreateInfo.pCode = (uint32_t *)shaderCode;

    VK_CHECK(vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderModule));

    delete[] shaderCode;

    return GHOST_kSuccess;
  }
  else {
    std::cerr << "Error: Could not open shader file \"" << fileName << "\"" << std::endl;
    return GHOST_kFailure;
  }
}
static VkPipelineShaderStageCreateInfo loadShader(VkDevice device,std::string fileName,
                                                  VkShaderStageFlagBits stage)
{
  VkPipelineShaderStageCreateInfo shaderStage = {};
  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStage.stage = stage;
  VkShaderModule shaderModule;
  loadShader(fileName.c_str(), device, shaderModule);
  shaderStage.module = shaderModule;
  shaderStage.pName = "main";  // todo : make param
  BLI_assert(shaderStage.module != VK_NULL_HANDLE);
  return shaderStage;
};
typedef struct MBvk {
  VkDevice device;
  VkDeviceMemory memory;
  VkBuffer buffer = VK_NULL_HANDLE;
  VkIndexType idxType;
  int version = {-1};
  uint32_t count;
  MBvk(VkDevice device_)
      : device(device_),
        memory(VK_NULL_HANDLE),
        buffer(VK_NULL_HANDLE),
        version(-1),
        count(0),
        idxType(VK_INDEX_TYPE_UINT32){};
  bool isValid()
  {
    return buffer != VK_NULL_HANDLE;
  }
  void dealloc()
  {
    if (buffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, buffer, nullptr);
      vkFreeMemory(device, memory, nullptr);
      buffer = VK_NULL_HANDLE;
      memory = VK_NULL_HANDLE;
    };
  };

} MBvk;
typedef struct MIVSIvk {
  size_t size;
  uint32_t w, h, d, l, c, mipLevel;
  VkMemoryRequirements memReqs;
  VkDeviceMemory memory;
  VkImage image;
  VkImageView view;
  VkSampler sampler;
  VkDescriptorImageInfo Info;
  VkFormat format;
  BYTE *mapped;
  bool valid;
  int version = {-1};
  VkDevice device;
  MIVSIvk *next = nullptr;
  MIVSIvk(VkDevice device_ )
      : device(device_),memory(VK_NULL_HANDLE),
        image(VK_NULL_HANDLE),
        view(VK_NULL_HANDLE),
        sampler(VK_NULL_HANDLE),
        memReqs({}),
        Info({}),
        valid(false),
        mapped(nullptr),
        next(nullptr)
  {
    size = 0;
    w = h = d = l = c = 0;
  };
  void dealloc()
  {
    if (sampler != VK_NULL_HANDLE)
      vkDestroySampler(device, sampler, nullptr);
    if (view != VK_NULL_HANDLE)
      vkDestroyImageView(device, view, nullptr);
    if (image != VK_NULL_HANDLE)
      vkDestroyImage(device, image, nullptr);
    if (memory != VK_NULL_HANDLE)
      vkFreeMemory(device, memory, nullptr);

    memory = VK_NULL_HANDLE, image = VK_NULL_HANDLE, view = VK_NULL_HANDLE,
    sampler = VK_NULL_HANDLE;
    size = 0;
    w = h = d = l = c = 0;
    if (next != nullptr) {
      next->dealloc();
      delete next;
      next = nullptr;
    }
  };
  bool isValid()
  {
    return memory != VK_NULL_HANDLE;
  }
} MIVSIvk;

typedef struct MBIVSIvk {
  VkDevice device;
  size_t size;
  uint32_t w, h, d, l, c, mipLevel;
  VkMemoryRequirements memReqs;
  VkDeviceMemory memory;
  VkImage image;
  VkImageView view;
  VkSampler sampler;
  VkBuffer buffer;
  VkDescriptorImageInfo Info;
  VkDescriptorBufferInfo bInfo;
  VkFormat format;
  bool valid;
  int version = {-1};
  MBIVSIvk(VkDevice device_)
      : device(device_),
       memory(VK_NULL_HANDLE),
        image(VK_NULL_HANDLE),
        view(VK_NULL_HANDLE),
        sampler(VK_NULL_HANDLE),
        buffer(VK_NULL_HANDLE),
        memReqs({}),
        Info({}),
        valid(false)
  {
    size = 0;
    w = h = d = l = c = 0;
  };
  void dealloc()
  {

    if (sampler != VK_NULL_HANDLE)
      vkDestroySampler(device, sampler, nullptr);
    if (view != VK_NULL_HANDLE)
      vkDestroyImageView(device, view, nullptr);
    if (image != VK_NULL_HANDLE)
      vkDestroyImage(device, image, nullptr);
    if (buffer != VK_NULL_HANDLE)
      vkDestroyBuffer(device, buffer, nullptr);
    if (memory != VK_NULL_HANDLE)
      vkFreeMemory(device, memory, nullptr);

    memory = VK_NULL_HANDLE, image = VK_NULL_HANDLE, view = VK_NULL_HANDLE,
    buffer = VK_NULL_HANDLE, sampler = VK_NULL_HANDLE;
  };
  bool isValid()
  {
    return memory != VK_NULL_HANDLE;
  }
} MBIVSIvk;

struct ImmidiateCmdPool {
 public:
  struct {
    VkDeviceMemory memory;
    VkBuffer buffer;
    uint8_t *data;
    mutable int Nums;
    VkMemoryAllocateInfo allocInfo;
    VkMemoryRequirements memReqs;
    VkBufferCreateInfo bufferCreateInfo;
  } staging = {VK_NULL_HANDLE, VK_NULL_HANDLE, nullptr, 0, {}, {}, {}};

  VkDevice device;
  VkCommandBuffer cmd;
  bool commit;
  VkCommandPool cmdPool;
  VkQueue queue;
  uint32_t queue_index;

  VkSemaphore semaphore;
  VkFence fence;

  GHOST_TSuccess alloc();
  void free();

  GHOST_TSuccess createCommandPool();
  void destroyCommandPool();

  GHOST_TSuccess createSemaphore()
  {

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore));

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = 0;
    VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));
  };
  void destroySemaphore()
  {
    vkDestroySemaphore(device, semaphore, nullptr);
    vkDestroyFence(device, fence, nullptr);
  };

  GHOST_TSuccess allocStaging(size_t size);
  template<class INFO> GHOST_TSuccess allocStaging(INFO &_)
  {
    return allocStaging(_.size);
  }
  void freeStaging();

  GHOST_TSuccess begin();
  GHOST_TSuccess end();
  GHOST_TSuccess submit(int i = 0);
  GHOST_TSuccess wait();

 GHOST_TSuccess Map(void *src, VkDeviceSize offset, VkDeviceSize size)
  {

    char *dst;
    VK_CHECK(vkMapMemory(device, staging.memory, offset, size, 0, (void **)&dst));
    memcpy(dst, src, size);
    vkUnmapMemory(device, staging.memory);
    return GHOST_kSuccess;
  };
  template<class B>
  bool Copy(B &_, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset)
  {
    VkBufferCopy copyRegion = {srcOffset, dstOffset, size};
    vkCmdCopyBuffer(cmd, staging.buffer, _.buffer, 1, &copyRegion);
    return true;
  };


};

void setImageLayout(VkCommandBuffer cmdbuffer,
                VkImage image,
                VkImageLayout oldImageLayout,
                VkImageLayout newImageLayout,
                VkImageSubresourceRange subresourceRange,
                    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
{
// Create an image barrier object

VkImageMemoryBarrier imageMemoryBarrier{};
imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

imageMemoryBarrier.oldLayout = oldImageLayout;
imageMemoryBarrier.newLayout = newImageLayout;
imageMemoryBarrier.image = image;
imageMemoryBarrier.subresourceRange = subresourceRange;

// Source layouts (old)
// Source access mask controls actions that have to be finished on the old layout
// before it will be transitioned to the new layout
switch (oldImageLayout) {
case VK_IMAGE_LAYOUT_UNDEFINED:
  // Image layout is undefined (or does not matter)
  // Only valid as initial layout
  // No flags required, listed only for completeness
  imageMemoryBarrier.srcAccessMask = 0;
  break;

case VK_IMAGE_LAYOUT_PREINITIALIZED:
  // Image is preinitialized
  // Only valid as initial layout for linear images, preserves memory contents
  // Make sure host writes have been finished
  imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
  break;

case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
  // Image is a color attachment
  // Make sure any writes to the color buffer have been finished
  imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  break;

case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
  // Image is a depth/stencil attachment
  // Make sure any writes to the depth/stencil buffer have been finished
  imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  break;

case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
  // Image is a transfer source
  // Make sure any reads from the image have been finished
  imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  break;

case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
  // Image is a transfer destination
  // Make sure any writes to the image have been finished
  imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  break;

case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
  // Image is read by a shader
  // Make sure any shader reads from the image have been finished
  imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  break;
default:
  // Other source layouts aren't handled (yet)
  break;
}

// Target layouts (new)
// Destination access mask controls the dependency for the new image layout
switch (newImageLayout) {
case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
  // Image will be used as a transfer destination
  // Make sure any writes to the image have been finished
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  break;

case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
  // Image will be used as a transfer source
  // Make sure any reads from the image have been finished
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  break;

case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
  // Image will be used as a color attachment
  // Make sure any writes to the color buffer have been finished
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  break;

case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
  // Image layout will be used as a depth/stencil attachment
  // Make sure any writes to depth/stencil buffer have been finished
  imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask |
                                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  break;

case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
  // Image will be read in a shader (sampler, input attachment)
  // Make sure any writes to the image have been finished
  if (imageMemoryBarrier.srcAccessMask == 0) {
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
  }
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  break;
default:
  // Other source layouts aren't handled (yet)
  break;
}

// Put barrier inside setup command buffer
vkCmdPipelineBarrier(
  cmdbuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
}

template<typename T, typename T2> void TransX(T &pool, T2 &dst, VkImageLayout O, VkImageLayout X)
{

  VkImageSubresourceRange subresourceRange = {};
  subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  subresourceRange.baseMipLevel = 0;
  subresourceRange.levelCount = dst.mipLevel;
  subresourceRange.layerCount = dst.l;

  pool.begin();

  setImageLayout(pool.cmd, dst.image, O, X, subresourceRange);

  pool.end();
  pool.submit();
  pool.wait();

  dst.Info.imageLayout = X;
};

class VKCommandBufferManager : public ImmidiateCmdPool {
  friend class VKContext;

 public:
  /* Event to coordinate sequential execution across all "main" command buffers. */
  static VkEvent sync_event;
  static uint64_t event_signal_val;

  /* Counter for active command buffers. */
  static int num_active_cmd_bufs;

 private:
  /* Associated Context and properties. */
 // VKContext &context_;
  bool supports_render_ = false;

  /* CommandBuffer tracking. */
  VkCommandBuffer active_command_buffer_ = VK_NULL_HANDLE;
  VkCommandBuffer last_submitted_command_buffer_ = VK_NULL_HANDLE;

  /* Active MTLCommandEncoders. */
  enum {
    MTL_NO_COMMAND_ENCODER = 0,
    MTL_RENDER_COMMAND_ENCODER = 1,
    MTL_BLIT_COMMAND_ENCODER = 2,
    MTL_COMPUTE_COMMAND_ENCODER = 3
  } active_command_encoder_type_ = MTL_NO_COMMAND_ENCODER;

  // VkRenderCommandEncoder  active_render_command_encoder_ = nil;
  // VkBlitCommandEncoder        active_blit_command_encoder_ = nil;
  // VkComputeCommandEncoder active_compute_command_encoder_ = nil;

  /* State associated with active RenderCommandEncoder. */
  // VKRenderPassState render_pass_state_;
  // VKFrameBuffer *active_frame_buffer_ = nullptr;
  // VKRenderPassDescriptor *active_pass_descriptor_ = nullptr;

  /* Workload heuristics - We may need to split command buffers to optimize workload and balancing.
   */
  int current_draw_call_count_ = 0;
  int encoder_count_ = 0;
  int vertex_submitted_count_ = 0;
  bool empty_ = true;

 public:
  /*
  VKCommandBufferManager(VKContext &context)
      : context_(context)  //, render_pass_state_(context, *this){};
        {};
  */
  VKCommandBufferManager(VkDevice device_) 
  {
    device = device_;
  }
  ~VKCommandBufferManager()
  {
    free();
  };
  void prepare(GHOST_ContextVK *ctx);

  /* If wait is true, CPU will stall until GPU work has completed. */
  // bool submit(bool wait);

  /* Fetch/query current encoder. */
  // bool is_inside_render_pass();
  // bool is_inside_blit();
  // bool is_inside_compute();
  /*
  VKRenderCommandEncoder get_active_render_command_encoder();
  VKBlitCommandEncoder get_active_blit_command_encoder();
   VKComputeCommandEncoder get_active_compute_command_encoder();
  VKFrameBuffer *get_active_framebuffer();
  */

  /* RenderPassState for RenderCommandEncoder.
  VKRenderPassState &get_render_pass_state()
  {
    // Render pass state should only be valid if we are inside a render pass.
    BLI_assert(this->is_inside_render_pass());
    return render_pass_state_;
  }
  */

  /* Rendering Heuristics. */
  // void register_draw_counters(int vertex_submission);
  // void reset_counters();
  // bool do_break_submission();

  /* Encoder and Pass management. */
  /* End currently active MTLCommandEncoder. */
  // bool end_active_command_encoder();
  /*
  VKRenderCommandEncoder ensure_begin_render_command_encoder(VKFrameBuffer *ctx_framebuffer,
                                                                  bool force_begin,
                                                                  bool *new_pass);
  VKBlitCommandEncoder ensure_begin_blit_encoder();
  VKComputeCommandEncoder ensure_begin_compute_encoder();
  */
  /* Workload Synchronization.
  bool insert_memory_barrier(eGPUBarrier barrier_bits,
                             eGPUStageBarrierBits before_stages,
                             eGPUStageBarrierBits after_stages);
                             */
  /* TODO(Metal): Support fences in command buffer class. */

  /* Debug.
  void push_debug_group(const char *name, int index);
  void pop_debug_group();
*/
 private:
  /* Begin new command buffer. */
  // VkCommandBuffer ensure_begin();
  // void register_encoder_counters();
};



GHOST_TSuccess ImmidiateCmdPool::createCommandPool()
{

  VkCommandPoolCreateInfo cmdPoolInfo = {};
  cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmdPoolInfo.queueFamilyIndex = queue_index;  // ctx->device.Qvillage.index[0];
  cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  VK_CHECK(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));
  printf("======================ImmidiateCmdPool COMMAND BUFFER   %llx \n", (int64_t)cmdPool);
  return GHOST_kSuccess;
};

void ImmidiateCmdPool::destroyCommandPool()
{
  if (cmdPool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device, cmdPool, nullptr);
    cmdPool = VK_NULL_HANDLE;
  }
};

GHOST_TSuccess ImmidiateCmdPool::allocStaging(size_t size)
{
  if (staging.Nums == 0) {
    VkMemoryAllocateInfo memAllocInfo{};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    staging.allocInfo = memAllocInfo;
    staging.allocInfo.allocationSize = 0;
    VkBufferCreateInfo bufCreateInfo{};
    bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging.bufferCreateInfo = bufCreateInfo;
    staging.bufferCreateInfo.size = 0;
    staging.bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging.bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  else
    GHOST_MAT_PRINTF("Bridge Test ==>>   NUMS == %d   \n", staging.Nums);

  staging.Nums++;
  if (staging.bufferCreateInfo.size < size) {

    if (staging.bufferCreateInfo.size != 0) {
      GHOST_MAT_PRINTF("Bridge Test ==>>  destroy  stagingBuffer   %p  \n", staging.buffer);
      vkDestroyBuffer(device, staging.buffer, nullptr);
    }
    staging.bufferCreateInfo.size = size;
    VK_CHECK(vkCreateBuffer(device, &staging.bufferCreateInfo, nullptr, &staging.buffer));
    vkGetBufferMemoryRequirements(device, staging.buffer, &staging.memReqs);

    staging.allocInfo.memoryTypeIndex = getMemoryType(staging.memReqs.memoryTypeBits,
                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                      nullptr);

    GHOST_MAT_PRINTF(
        "Bridge Test ==>>  recreate  stagingBuffer   %zu   %p    TypeBits   %u   TypeIndex  %u \n",
        staging.bufferCreateInfo.size,
        staging.buffer,
        (uint32_t)staging.memReqs.memoryTypeBits,
        (uint32_t)staging.allocInfo.memoryTypeIndex);

    /// if (staging.allocInfo.allocationSize < staging.memReqs.size) {
    /// if (staging.allocInfo.allocationSize != 0) {

    if (staging.allocInfo.allocationSize != 0) {
      GHOST_MAT_PRINTF("Bridge Test ==>> Free  stagingMemory   %p  \n", staging.memory);
      vkFreeMemory(device, staging.memory, nullptr);
    }
    staging.allocInfo.allocationSize = staging.memReqs.size;
    VK_CHECK(vkAllocateMemory(device, &staging.allocInfo, nullptr, &staging.memory));
    VK_CHECK(vkBindBufferMemory(device, staging.buffer, staging.memory, 0));
    GHOST_MAT_PRINTF("Bridge Test ==>>  reallocate  stagingMemory   %zu    %p  \n",
                        staging.memReqs.size,
                        staging.memory);
  }

  return GHOST_kSuccess;
};
void ImmidiateCmdPool::freeStaging()
{

  if (staging.buffer != VK_NULL_HANDLE) {

    vkDestroyBuffer(device, staging.buffer, nullptr);
  }

  if (staging.memory != VK_NULL_HANDLE) {
    vkFreeMemory(device, staging.memory, nullptr);
  }

  staging = {VK_NULL_HANDLE, VK_NULL_HANDLE, nullptr};
};

GHOST_TSuccess ImmidiateCmdPool::begin()
{

  static VkCommandBufferBeginInfo cmdBufInfo{};
  cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  GHOST_MAT_PRINTF("IMCM    queue  BEGIN   %llx    pool    %llx    cmd    %llx    \n ",
                      (int64_t)queue,
                      (int64_t)cmdPool,
                      (int64_t)cmd);
  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBufInfo));
  commit = true;
  return GHOST_kSuccess;
}

GHOST_TSuccess ImmidiateCmdPool::end()
{

  VK_CHECK(vkEndCommandBuffer(cmd));
  GHOST_MAT_PRINTF("IMCM    Make  CopyAfterX      %llx    \n ", (int64_t)cmd);
  return GHOST_kSuccess;
};

GHOST_TSuccess ImmidiateCmdPool::submit(int i)
{

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmd;

  VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
  return GHOST_kSuccess;

  /// log_imcm("IMCM     QUEUE submit   [%x]     %p  \n", _threadid, queue);
};

GHOST_TSuccess ImmidiateCmdPool::alloc()
{

  createCommandPool();

  VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
  commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandPool = cmdPool;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  commandBufferAllocateInfo.commandBufferCount = 1;

  VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &cmd));
  GHOST_MAT_PRINTF("IMCM    queue   %llx    pool    %llx    cmd    %llx    \n ",
                      (int64_t)queue,
                      (int64_t)cmdPool,
                      (int64_t)cmd);
  return GHOST_kSuccess;
};

void ImmidiateCmdPool::free()
{
  freeStaging();
  if (cmd != VK_NULL_HANDLE) {
    GHOST_MAT_PRINTF("dealloc  Immidiate    %p  \n", cmd);
    vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
    cmd = VK_NULL_HANDLE;
  }
  destroyCommandPool();
};

GHOST_TSuccess ImmidiateCmdPool::wait()
{
  VK_CHECK(vkQueueWaitIdle(queue));
  commit = false;
  return GHOST_kSuccess;
}

void VKCommandBufferManager::prepare(GHOST_ContextVK *ctx)
{


  BLI_assert(ctx);
  device = ctx->getDevice();
  ctx->getQueue(2, &queue, &queue_index);
  alloc();
};

template<typename T>
GHOST_TSuccess createImage(VKCommandBufferManager &cmm, T &desc, uint32_t w, uint32_t h)
{
    auto FORMAT = VK_FORMAT_R8G8B8A8_UNORM;

    VkImageCreateInfo imageCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent.width = desc.w = w;
    imageCreateInfo.extent.height = desc.h = h;
    imageCreateInfo.extent.depth = desc.d = 1;
    imageCreateInfo.mipLevels = desc.mipLevel = 1;
    imageCreateInfo.arrayLayers = desc.l = 1;
    imageCreateInfo.format = FORMAT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;  ///(VkSampleCountFlagBits)m_nMSAASampleCount;
    imageCreateInfo.usage = (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    imageCreateInfo.flags = 0;

    VkResult nResult;
    nResult = vkCreateImage(cmm.device, &imageCreateInfo, nullptr, &desc.image);
    if (nResult != VK_SUCCESS) {
      fprintf(stderr,"vkCreateImage failed for eye image with error %d\n", nResult);
      return GHOST_kFailure;
    }

    desc.Info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkMemoryRequirements memoryRequirements = {};
    vkGetImageMemoryRequirements(cmm.device, desc.image, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    if (!MemoryTypeFromProperties(memoryRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                               &memoryAllocateInfo.memoryTypeIndex)) {
      fprintf(stderr, "Failed to find memory type matching requirements.\n");
      return GHOST_kFailure;
    }

    nResult = vkAllocateMemory(cmm.device, &memoryAllocateInfo, nullptr, &desc.memory);
    if (nResult != VK_SUCCESS) {
      fprintf(stderr, "Failed to find memory for image.\n");
      return GHOST_kFailure;
    }

    nResult = vkBindImageMemory(cmm.device, desc.image, desc.memory, 0);
    if (nResult != VK_SUCCESS) {
      fprintf(stderr, "Failed to bind memory for image.\n");
      return GHOST_kFailure;
    }

    VkImageViewCreateInfo imageViewCI = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCI.format = FORMAT;
    imageViewCI.flags = 0;
    imageViewCI.subresourceRange = {};
    imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCI.subresourceRange.baseMipLevel = 0;
    imageViewCI.subresourceRange.levelCount = 1;
    imageViewCI.subresourceRange.baseArrayLayer = 0;
    imageViewCI.subresourceRange.layerCount = 1;
    imageViewCI.image = desc.image;
    VK_CHECK(vkCreateImageView(cmm.device, &imageViewCI, nullptr, &desc.view));

    // Create sampler to sample from the attachment in the fragment shader
    VkSamplerCreateInfo samplerCI{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    
    samplerCI.maxAnisotropy = 1.0f;
    samplerCI.magFilter = VK_FILTER_NEAREST;
    samplerCI.minFilter = VK_FILTER_NEAREST;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = samplerCI.addressModeU;
    samplerCI.addressModeW = samplerCI.addressModeU;
    samplerCI.mipLodBias = 0.0f;
    samplerCI.maxAnisotropy = 1.0f;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = 1.0f;
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK(vkCreateSampler(cmm.device, &samplerCI, nullptr, &desc.sampler));

    // Fill a descriptor for later use in a descriptor set
    desc.Info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    desc.Info.imageView = desc.view;
    desc.Info.sampler = desc.sampler;

    TransX(cmm, desc, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  return GHOST_kSuccess;
};


struct vkDSLMem {

  std::unordered_map<std::string, VkDescriptorSetLayout> Layout;
  SRWLOCK SlimLock;
  VkDevice device;
  vkDSLMem()
  {
    InitializeSRWLock(&SlimLock);
    device = VK_NULL_HANDLE;
  }

  void setDevice(VkDevice device_)
  {
    device = device_;
  };

  void destroy();

  bool get(VkDescriptorSetLayout &layout, LayoutType Lty);

  bool $set$(LayoutType type, std::vector<VkDescriptorSetLayoutBinding> &);
};

//extern
vkDSLMem DSL;
#  define $DSL DSL

bool vkDSLMem::get(VkDescriptorSetLayout &layout, LayoutType Lty)
{

  if (Layout.count(Lty.data()) > 0) {
    layout = Layout[Lty.data()];
    return true;
  };

  return false;
};
void vkDSLMem::destroy()
{
  AcquireSRWLockExclusive(&SlimLock);
  for (auto &[k, v] : Layout) {
    vkDestroyDescriptorSetLayout(device, v, nullptr);
  };
  Layout.clear();
  ReleaseSRWLockExclusive(&SlimLock);
};
bool vkDSLMem::$set$(LayoutType type, std::vector<VkDescriptorSetLayoutBinding> &DSLB)
{


  AcquireSRWLockExclusive(&SlimLock);

  if (Layout.count(type.data()) > 0) {
    ReleaseSRWLockExclusive(&SlimLock);
    return true;
  }

  bool Default = false;
  size_t size = DSLB.size();
  if (size == 0) {
    size = type.size();
    Default = true;
  };

  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, (uint32_t)size};
       
   
  if (Default) {
    VkDescriptorSetLayoutBinding *setLayoutBindings = nullptr;
    setLayoutBindings = new VkDescriptorSetLayoutBinding[size];
    _PoolSize pSize = {(uint32_t)size, 0, 0, 0};
    for (int i = 0; i < size; i++) {
      if (type[i] == 's') {

        setLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        setLayoutBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |
                                          VK_SHADER_STAGE_VERTEX_BIT;
        setLayoutBindings[i].binding = (uint32_t)i;
        setLayoutBindings[i].descriptorCount = 1;
        pSize.ssbo++;
      }
      else if (type[i] == 'u') {

        setLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        setLayoutBindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        setLayoutBindings[i].binding = (uint32_t)i;
        setLayoutBindings[i].descriptorCount = 1;
        pSize.ubo++;
      }
      else if (type[i] == 't') {

        setLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        setLayoutBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        setLayoutBindings[i].binding = (uint32_t)i;
        setLayoutBindings[i].descriptorCount = 1;
        setLayoutBindings[i].pImmutableSamplers = NULL;
        pSize.tex++;
      }
      else {
        fprintf(stderr,"Descriptor_CreateIO::ParseError  \n");
        exit(-1);
      };
    }
    descriptorSetLayoutCreateInfo.pBindings = &(setLayoutBindings[0]);
  }
  else {
    descriptorSetLayoutCreateInfo.pBindings = DSLB.data();
  };
  /// uint32_t ID = createIO(type, pSize, setLayoutBindings);

  VkDescriptorSetLayout _io;
  VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, &_io));

  Layout[type.data()] = _io;

  if (Default)
    delete[] descriptorSetLayoutCreateInfo.pBindings;

  ReleaseSRWLockExclusive(&SlimLock);

  GHOST_MAT_PRINTF("Create  DSL   %s     %p   \n", type.data(), Layout[type.data()]);
  return true;
};


struct PipelineConfigure {

  VkPipelineLayout vkPL;
  VkRenderPass vkRP;
  VkPipelineCache vkPC;
  VkPipelineVertexInputStateCreateInfo *vkPVISci;
  std::string spv;
  //arth::GEOMETRY defulettype;
  uint32_t multisample;
};
struct DescriptorVk {

 protected:
 public:
  VkDevice device;

  struct ioSP {
    VkDescriptorPool Pool;
    VkDescriptorSet Set;
  };
  typedef std::tuple<LayoutType, ioSP> ioType;
  VkPipelineLayout draft;
  std::vector<ioSP> io;
  std::vector<VkDescriptorSetLayout> layoutSet;

  struct {
    long Layout;
    long Set;
  } Nums = {0, 0};

  _PoolSize PoolSize = {0, 0};

  DescriptorVk(VkDevice device_);
  ~DescriptorVk();


  bool $createLayout$(LayoutType type)
  {
    std::vector<VkDescriptorSetLayoutBinding> _ = {};
    if ($DSL.$set$(type, _)) {
      VkDescriptorSetLayout layout;
      $DSL.get(layout, type);
      layoutSet.push_back(layout);
    }
    else {
      fprintf(stderr,"Failed to create DSL.  \n");
      exit(-1);
    }
    return true;
  };

  size_t $createLayout$(LayoutType type, std::vector<VkDescriptorSetLayoutBinding> &dslb)
  {

    VkDescriptorSetLayout layout;
    if ($DSL.$set$(type, dslb)) {
      $DSL.get(layout, type);
      layoutSet.push_back(layout);
      return layoutSet.size() - 1;
    };
    return -1;
  };




  template<class De> GHOST_TSuccess $createPuddle$(De &desc)
  {
    long orig = InterlockedCompareExchange(&(desc.hach.id), INT32_MAX, -1);
    if (orig != -1)
      return GHOST_kFailure;

    LayoutType type = desc.type;

    /// log_scr("LOCK_OPERATOR CreatePuddle      %s  \n ", type.data());
    VkDescriptorSetLayout layout;

    std::vector<VkDescriptorSetLayoutBinding> _ = {};
    if ($DSL.$set$(type, _)) {
      desc.hach.id = io.size();
      if (desc.hach.id >= DESC_MAX) {
        fprintf(stderr,"Create Layout  OVER Limit. \n");
        exit(-1);
      };
      $DSL.get(layout, type);
      /// appendLayoutSet(layout);
    }
    else {
      fprintf(stderr, "Failed to create DSL.  \n");
      exit(-1);
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.clear();
    size_t size = type.size();
    VkDescriptorPoolSize descriptorPoolSize{};
    

    for (int i = 0; i < size; i++) {
      if (type[i] == 's') {
        descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorPoolSize.descriptorCount = 1;

      }
      else if (type[i] == 'u') {
        descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorPoolSize.descriptorCount = 1;

      }
      else if (type[i] == 't') {
        descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorPoolSize.descriptorCount = 1;
      }
      else {
       fprintf(stderr,"Descriptor_CreateIO::ParseError  \n");
        exit(-1);
      };
      poolSizes.push_back(descriptorPoolSize);
    };

    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolInfo.pPoolSizes = poolSizes.data();
    descriptorPoolInfo.maxSets = 1;

    ioSP puddle = {};
    VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &puddle.Pool));
    allocInfo.descriptorPool = puddle.Pool;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &puddle.Set));
    io.push_back(puddle);

     return GHOST_kSuccess;
  };

  template<class De>
  GHOST_TSuccess $createPuddle$(De &desc, std::vector<VkDescriptorSetLayoutBinding> &dslb)
  {

    long orig = InterlockedCompareExchange(&(desc.hach.id), INT32_MAX, -1);
    if (orig != -1)
      return GHOST_kFailure;


    LayoutType type = desc.type;

    std::vector<VkDescriptorPoolSize> poolSizes;
    for (auto &v : dslb) {
      VkDescriptorPoolSize descriptorPoolSize{};
      descriptorPoolSize.type = v.descriptorType;
      descriptorPoolSize.descriptorCount = 1;
      poolSizes.push_back(descriptorPoolSize);
    }

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;

    if ($DSL.$set$(type, dslb)) {
      $DSL.get(layout, type);
      desc.hach.id = io.size();
      // appendLayoutSet(layout);
      if (desc.hach.id >= DESC_MAX) {
        fprintf(stderr,"Create Layout  OVER Limit. \n");
        exit(-1);
      };
    }
    else {
      fprintf(stderr, "Create Layout  OVER Limit. \n");
      exit(-1);
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolInfo.pPoolSizes = poolSizes.data();
    descriptorPoolInfo.maxSets = 1;

    ioSP puddle = {};
    VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &puddle.Pool));
    allocInfo.descriptorPool = puddle.Pool;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &puddle.Set));
    io.push_back(puddle);

         return GHOST_kSuccess;
  };


  template<class Ma> void destroyPuddle(Ma &mat)
  {
    ioSP &sp = io[&mat].Pool;
    if (sp.Pool != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool($device, sp.Pool, nullptr);
      sp.Pool == VK_NULL_HANDLE;
      mat.active.desc = -1;
    };
  };

  VkDescriptorSet &getSet(long id)
  {
    return io[id].Set;
  };

  GHOST_TSuccess createDraft(std::vector<VkPushConstantRange> pushConstantRange = {});
  void destroy();

};
struct UniformVk;

typedef struct Hache {
  long id;
  long version;
  size_t hash;
} Hache;
typedef struct Bache {

  long id;
  long refCnt;
  Hache buffer;

  VkDeviceSize align;
  VkDeviceSize reqAlign;

  VkDeviceSize size;
  VkDeviceSize offset;
  VkDeviceSize reqSize;

 
  std::string_view type;

  SRWLOCK excl;
  VkBuffer vkBuffer;
  void *mapped;

  Bache();
  Bache(size_t hash, VkDeviceSize align, LayoutType type);
  Bache &operator=(const Bache &other);

  #if 0
   std::queue<uint32_t> vacancy;
  template<class T> void Undo(T desc)
  {

    AcquireSRWLockExclusive(&excl);
    vacancy.push(desc.id);
    desc.info.buffer = VK_NULL_HANDLE;
    refCnt--;
    ReleaseSRWLockExclusive(&excl);
  };

  template<class T> void Redo(T desc)
  {

    AcquireSRWLockExclusive(&excl);
    desc.id = vacancy.front();
    vacancy.pop();
    refCnt++;
    ReleaseSRWLockExclusive(&excl);
  };
#  endif
} Bache;


enum class VERTEX_INPUT : UINT32 {

  vertexPRS = 0b000001,
  vertexPC = 0b000010,
  vertexPUN = 0b000011,
  vertexPV = 0b000100,
  vertexPNC = 0b000101,
  vertexPQS = 0b000110,
  vertexPQS4 = 0b000111,
  vertexPN = 0b001000,
  vertexSprite = 0b001001,
  vertexP = 0b001010,
  ALL_TYPE,

  vertex_F = 0b0000000000010000,
  vertex_F_SCALE,
  vertex_V2 = 0b0000000100000000,
  vertex_V2_UV,
  vertex_V2_POSITION,
  vertex_V3 = 0b0001000000000000,
  vertex_V3_POSITION,
  vertex_V3_NORMAL,
  vertex_V3_COLOR,
  vertex_V3_TANGENT,
  vertex_V3_BITANGENT,
  vertex_V3_ROTATION,
  vertex_V3_SCALE,
  vertex_V4 = 0b10000000000000000,
  vertex_V4_POSITION,
  vertex_V4_QUATERNION,
  vertex_V4_VELOCITY,
  vertex_V4_SCALE,
  vertex_INDEX,
  ALL

};
struct vkPVISci {

  std::vector<VkVertexInputBindingDescription> vertexInputBindings;
  std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;
  VkPipelineVertexInputStateCreateInfo info;
  VERTEX_INPUT  type;
};



struct vkPVISciMem {

  /// vkPVISci                                           Info[arth::INPUT_TYPE_ALL];
  std::unordered_map<std::string, vkPVISci> Info;
  SRWLOCK SlimLock;
  vkPVISciMem(){};

 
  template<class FormatTy>
  void $set$(std::string Name,
             const FormatTy *format,
             VkVertexInputRate rate = VK_VERTEX_INPUT_RATE_VERTEX)
  {


    BLI_assert(Info.count(Name) == 0);
    vkPVISci& mem = Info[Name];
   
 

    mem.vertexInputBindings.clear();
    mem.vertexInputAttributes.clear();


    VkVertexInputBindingDescription vib;
    vib.binding = 0;
    vib.stride = (uint32_t)format->stride;
    vib.inputRate = rate;


    mem.vertexInputBindings.push_back(vib);

    for (int i = 0; i < format->attr_len; i++) {
      VkVertexInputAttributeDescription desc;
      desc.location = (UINT32)i;
      desc.binding = 0;
      const GPUVertAttr *a = &format->attrs[i];
      if (a->size == 4) {
        desc.format = VK_FORMAT_R32_SFLOAT;
      }
      else if (a->size == 8) {
        desc.format = VK_FORMAT_R32G32_SFLOAT;
      }
      else if (a->size == 12) {
        desc.format = VK_FORMAT_R32G32B32_SFLOAT;
      }
      else if (a->size == 16) {
        desc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
      }
      desc.offset = a->offset;
      mem.vertexInputAttributes.push_back(desc);
    };

    VkPipelineVertexInputStateCreateInfo &info = mem.info;

    info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    info.vertexBindingDescriptionCount = static_cast<uint32_t>(mem.vertexInputBindings.size()),
    info.pVertexBindingDescriptions = mem.vertexInputBindings.data(),
    info.vertexAttributeDescriptionCount = static_cast<uint32_t>(mem.vertexInputAttributes.size()),
    info.pVertexAttributeDescriptions = mem.vertexInputAttributes.data();
  

    GHOST_MAT_PRINTF(" Set VertexType   %x  \n", mem.info);


    ReleaseSRWLockExclusive(&SlimLock);
  };

  #if 0
  template<class Geom> void $setSprite$(Geom *geometry)
  {

    if (!enterAttr(geometry))
      return;

    vkPVISci mem;
    mem.type = ty;

    mem.vertexInputBindings = {
        {.binding = 0, .stride = (uint32_t)4 * 4, .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}};

    mem.vertexInputAttributes.clear();
    for (int i = 0; i < geometry->array.fieldNum; i++) {
      mem.vertexInputAttributes.push_back({.location = (UINT32)i,
                                           .binding = 0,
                                           .format = geometry->array.format[i],
                                           .offset = (UINT32)geometry->array.offset[i]});
    };

    mem.info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = static_cast<uint32_t>(mem.vertexInputBindings.size()),
        .pVertexBindingDescriptions = mem.vertexInputBindings.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(mem.vertexInputAttributes.size()),
        .pVertexAttributeDescriptions = mem.vertexInputAttributes.data()};

    log_obj(" Set VertexType   %x  \n", mem.info);
    Info[sty] = std::move(mem);

    ReleaseSRWLockExclusive(&SlimLock);
  };

  template<class Geom> void $setInstanced$(Geom *geometry)
  {

    auto insta = geometry->instance->buffer;
    if (geometry->attributes == nullptr) {
      $set$(insta, VK_VERTEX_INPUT_RATE_INSTANCE);
      return;
    }

    auto attr = geometry->attributes->buffer;
    if (!enterGeom(attr, insta))
      return;

    vkPVISci mem;
    mem.type = ty;

    mem.vertexInputBindings = {
        {.binding = 0,
         .stride = (uint32_t)attr->array.structSize,
         .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
        {.binding = 1,
         .stride = (uint32_t)insta->array.structSize,
         .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE},
    };

    mem.vertexInputAttributes.clear();
    int i = 0;
    for (i = 0; i < attr->array.fieldNum; i++) {
      mem.vertexInputAttributes.push_back({.location = (UINT32)i,
                                           .binding = 0,
                                           .format = attr->array.format[i],
                                           .offset = (UINT32)attr->array.offset[i]});
    };

    for (int j = 0; j < insta->array.fieldNum; j++) {
      mem.vertexInputAttributes.push_back({.location = (UINT32)i++,
                                           .binding = 1,
                                           .format = insta->array.format[j],
                                           .offset = (UINT32)insta->array.offset[j]});
    };

    mem.info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = static_cast<uint32_t>(mem.vertexInputBindings.size()),
        .pVertexBindingDescriptions = mem.vertexInputBindings.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(mem.vertexInputAttributes.size()),
        .pVertexAttributeDescriptions = mem.vertexInputAttributes.data()};

    log_obj(" Set VertexType   %x  \n", mem.info);
    Info[sty] = std::move(mem);

    ReleaseSRWLockExclusive(&SlimLock);
  };
  #endif
};

#  define $VInfo VInfo
vkPVISciMem VInfo;

    //template<class T, class Geom>
enum BUFFER_ATTR_TYPE {
  BUFFER_ATTR_VERTEX,
  BUFFER_ATTR_INDEX
};


template<typename T>
GHOST_TSuccess $createBuffer$(MBvk& input,VKCommandBufferManager &cmder,
                              T& geom,
                              BUFFER_ATTR_TYPE btype = BUFFER_ATTR_VERTEX)
{

  VkDeviceSize Size = geom.size;
  VkDevice device = cmder.device;

  //MBvk index(cmder.device);
  {

    VkMemoryRequirements memReqs;
    VkMemoryAllocateInfo memAlloc = {};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    VkBufferCreateInfo BufferInfo = {};
    BufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BufferInfo.size = Size;
    BufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VK_CHECK(vkCreateBuffer(device, &BufferInfo, nullptr, &input.buffer));
    vkGetBufferMemoryRequirements(device, input.buffer, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = getMemoryTypeIndex( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(device, &memAlloc, nullptr, &input.memory));
    VK_CHECK(vkBindBufferMemory(device, input.buffer, input.memory, 0));



    /*
    BufferInfo.size = geometry->Size.index;
    size += geometry->Size.index;
    BufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VK_CHECK(vkCreateBuffer(device, &BufferInfo, nullptr, &index.buffer));
    vkGetBufferMemoryRequirements(device, index.buffer, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = getMemoryTypeIndex(
        memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(device, &memAlloc, nullptr, &index.memory));
    VK_CHECK(vkBindBufferMemory(device, index.buffer, index.memory, 0));
    */

  }

  cmder.allocStaging(Size);
  cmder.Map(geom.data, 0, geom.size);
  cmder.begin();
  cmder.Copy(input, geom.size, 0, 0);
  cmder.end();
  cmder.submit();
  cmder.wait();
  


  return GHOST_kSuccess;
};









struct UniformVk {

  VkDevice device;
  struct Desc {
    Hache hach;
    LayoutType type;
    std::string _type;
  } desc;

  bool allocBach;
  Bache *bach;

  struct {
    long id;
    void *mapped;
    VkDescriptorBufferInfo info;
    VkBuffer vkBuffer;
  } ubo;

  VkDescriptorImageInfo image;

  UniformVk();
  UniformVk(long id, long version, VkDeviceSize size);
  UniformVk(const UniformVk &) = delete;
  UniformVk &operator=(UniformVk other) = delete;
  ~UniformVk();

  void dealloc();

  long swID;
  bool Upd[2];
  std::vector<VkDescriptorSet> descriptorSets[2];
  std::vector<VkWriteDescriptorSet> writeDescriptorSets[2];

  void setMaterialProperty(Bache *_bach)
  {
    bach = _bach;
    desc.type = bach->type;
    ubo.info.range = bach->align;
  };

  size_t createSet(DescriptorVk *descVk, std::string_view type = "", bool append = false);
  size_t createSet(DescriptorVk *descVk,
                   std::string_view type,
                   std::vector<VkDescriptorSetLayoutBinding> dslb,
                   bool append = false);

 // bool createUBO(VisibleObjectsVk *vobjVk, VkDeviceSize size = 0);
 // bool createICBO(VisibleObjectsVk *vobjVk, long Cnt, VkDeviceSize size = 0);

  bool setUBO(VkBuffer buffer);

  bool push_back(VkDescriptorSet &set);

  bool createWriteSets();
  bool createWriteSets(std::vector<VkDescriptorBufferInfo> uinfo, long update = 1);
  bool createWriteSets(VkDescriptorSet set,
                       std::vector<VkDescriptorBufferInfo> uinfo,
                       std::vector<VkDescriptorImageInfo> iinfo,
                       long update);
};



Bache::Bache()
    : id(-1),
      refCnt(0),
      buffer({-1, 0, 0}),
      align(0),
      reqAlign(0),
      size(0),
      reqSize(0),
    //  vacancy(),
      type(""),
      excl({}),
      vkBuffer(VK_NULL_HANDLE),
      mapped(nullptr){};

Bache::Bache(size_t hash, VkDeviceSize align, LayoutType type)
    : id(-1),
      refCnt(0),
      buffer({-1, 0, hash}),
      align(align),
      reqAlign(0),
      size(0),
      reqSize(0),
   //   vacancy(),
      type(type),
      excl({}),
      vkBuffer(VK_NULL_HANDLE),
      mapped(nullptr){};

Bache &Bache::operator=(const Bache &other)
{

  if (&other == this)
    return *this;

  this->align = other.align;
  this->buffer.hash = other.buffer.hash;
  this->type = other.type;

  return *this;
};


typedef struct Iache {

  VkFormat format;
  uint32_t multisample;
  VkImageLayout layout;
  VkDescriptorType type;
  long refCnt;
  const size_t hash;
  Hache hach;
  VkImageView vkI;
  std::string name;
  Iache() : hash(size_t(-1)), refCnt(0), multisample(1){};
  Iache(size_t hash) : hash(hash), hach({-1, 0, hash}), refCnt(0), multisample(1)
  {
    name = "new";
  
  };
  ~Iache()
  {
    GHOST_MAT_PRINTF(" Call  destructor   Iache   \n");
  };

  Iache &operator=(size_t hash)
  {
    if (this->hash == hash)
      return *this;
    if (refCnt > 0) {
      fprintf(stderr, "Iache  reference exists. you lost the hash.  \n");
      exit(-1);
    };
    this->~Iache();
    new (this) Iache(hash);
    return *this;
  };

  template<class T> Iache &operator=(T &iach)
  {

    GHOST_MAT_PRINTF(" Call Swap  Iache  %d    %x <=> %x  ,  %x <=> %x \n",
                     refCnt,
                     this->hach.id,
                     iach.hach.id,
                     this->hash,
                     iach.hash);

    if (this->hash == iach.hash)
      return *this;
    if (refCnt > 0) {
      fprintf(stderr, "Iache  reference exists. you lost the hash.  \n");
      exit(-1);
    };
    this->~Iache();
    new (this) Iache(iach.hash);
    this->hach.id = iach.hach.id;
    this->format = iach.format;
    this->multisample = iach.multisample;
    this->refCnt = iach.refCnt;
    this->vkI = iach.vkI;
    GHOST_MAT_PRINTF(" Call Swap  view %x   id  %x  hash  %x  version %x\n",
                     this->vkI,
                     this->hach.id,
                     this->hach.hash,
                     this->hach.version);
    return *this;
  };

  static size_t rehash(std::string imgName, size_t seed) noexcept
  {
    static std::hash<std::string> h_img{};
    static std::hash<size_t> h_seed{};

    std::size_t hash = 0;
    hash += h_img(imgName);
    hash += hash << 10;
    hash ^= hash >> 6;

    hash += h_seed(seed);
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
  };

  template<class I> void Delete(I *im)
  {
    long cnt = InterlockedDecrement(&refCnt);
    GHOST_MAT_PRINTF(" Decrement  Iache  %d   \n", cnt);
    if (cnt == 0) {
      im->$Delete(&hach);
    }
  };

} Iache;


DescriptorVk::DescriptorVk( VkDevice device_) :device(device_), draft(VK_NULL_HANDLE), Nums({0, 0})
{
  io.resize(0);
  layoutSet.resize(0);
  GHOST_MAT_PRINTF("Constructor   Descriptor   io  %zu   layoutSet  %x   \n", io.size(), layoutSet.size());
};

DescriptorVk::~DescriptorVk()
{
  destroy();
};

GHOST_TSuccess DescriptorVk::createDraft(std::vector<VkPushConstantRange> pushConstantRange)
{

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};

  uint32_t pcSize = (uint32_t)(pushConstantRange.size());
  if (pcSize != 0) {
    pipelineLayoutCreateInfo.pushConstantRangeCount = pcSize;
    pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRange.data();
  }

  pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCreateInfo.pSetLayouts = layoutSet.data();
  pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)layoutSet.size();
  VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &draft));
  return GHOST_kSuccess;
};

void DescriptorVk::destroy()
{

  if (draft != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, draft, nullptr);
    draft = VK_NULL_HANDLE;
  }

  for (auto &[pool, set] : io) {
    if (pool != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(device, pool, nullptr);
      pool = VK_NULL_HANDLE;
    };
  };
  Nums = {0, 0};

}

UniformVk::UniformVk()
{
  //desc = {.hach = {.id = -1, .version = -1, .hash = size_t(-1)}, .type = ""};
  desc = {{-1, -1, size_t(-1)},  ""};
  ubo.id = -1;
  ubo.mapped = nullptr;
  allocBach = false;
  bach = nullptr;
};

UniformVk::UniformVk(long id, long version, VkDeviceSize size)
    //: desc({.hach = {.id = -1, .version = -1, .hash = size_t(-1)}, .type = ""}){
        : desc({{-1, -1, size_t(-1)}, ""}){
  ubo.id = -1;
  ubo.mapped = nullptr;
  allocBach = false;
  bach = nullptr;
  Upd[0] = Upd[1] = false;
};

UniformVk::~UniformVk()
{
  dealloc();
}

void UniformVk::dealloc()
{
  if (allocBach) {
    __Delete__(bach);
    allocBach = false;
  }

  for (auto &v : descriptorSets)
    v.clear();
  for (auto &v : writeDescriptorSets)
    v.clear();
};

size_t UniformVk::createSet(DescriptorVk *descVk, std::string_view type, bool append)
{

  desc.hach = { -1, -1};

  if (type != "") {
    desc.type = type;
  }
  else {
    desc.type = bach->type;
  }

  GHOST_MAT_PRINTF("Create puddle %s    \n", desc.type.data(), desc.hach.id);

  descVk->$createPuddle$(desc);

  descriptorSets[swID].push_back(descVk->getSet(desc.hach.id));

  if (append)
    descVk->$createLayout$(desc.type);

  return descriptorSets[swID].size() - 1;
};

size_t UniformVk::createSet(DescriptorVk *descVk,
                            std::string_view type,
                            std::vector<VkDescriptorSetLayoutBinding> dslb,
                            bool append)
{

  if (swID != 1)
    swID = 0;

  //desc.hach = {.id = -1, .version = -1};
  desc.hach = {-1, -1};
  desc._type = type;
  desc.type = desc._type;
  GHOST_MAT_PRINTF("Create puddle  %d    %s    \n", desc.hach.id, desc.type.data());

  descVk->$createPuddle$(desc, dslb);
  descriptorSets[swID].push_back(descVk->getSet(desc.hach.id));

  if (append)
    descVk->$createLayout$(desc.type, dslb);

  return descriptorSets[swID].size() - 1;
};



bool UniformVk::setUBO(VkBuffer buffer)
{

  ubo.info.buffer = buffer;
  ubo.info.offset = ubo.id * bach->align;
  ubo.info.range = bach->align;
  char *begin = (char *)(bach->mapped);
  ubo.mapped = (void *)(begin + ubo.info.offset);

  return true;
};

bool UniformVk::push_back(VkDescriptorSet &set)
{
  descriptorSets[swID].push_back(set);
  return true;
};

bool UniformVk::createWriteSets()
{
  createWriteSets({ubo.info});
  return true;
};

bool UniformVk::createWriteSets(std::vector<VkDescriptorBufferInfo> uinfo, long update)
{

  if (desc.hach.id < 0)
    fprintf(stderr,"there is no descriptorSets.\n");

  if ((update > 0) || (update < 0)) {

    struct {
      uint32_t u;
      uint32_t t;
      uint32_t s;
    } idx = {uint32_t(-1), uint32_t(-1), uint32_t(-1)};

    size_t size = uinfo.size();

    writeDescriptorSets[swID].resize(size);

    for (int i = 0; i < (int)size; i++) {

      /*
      if (desc.type[i] == 's') {
        idx.s++;
      }
      else if (desc.type[i] == 'u') {
        */
      idx.u++;
      /// if (idx.u == 1) log_bad(" NIL  Create UBO   over limit. \n");
      writeDescriptorSets[swID][i] = {};
      writeDescriptorSets[swID][i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      writeDescriptorSets[swID][i].dstSet = descriptorSets[swID][i],
      writeDescriptorSets[swID][i].dstBinding = 0,
      writeDescriptorSets[swID][i].descriptorCount = 1,
      writeDescriptorSets[swID][i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      writeDescriptorSets[swID][i].pBufferInfo = &(uinfo[idx.u]);
    //  };

      /*
    }
    else if (desc.type[i] == 't') {
      idx.t++;
    }
    else {
      log_bad("Descriptor_CreateIO::ParseError  \n");
    };
    */
    }
  }

  if (update >= 0) {
    Upd[swID] = true;
    vkUpdateDescriptorSets(device,
                           static_cast<uint32_t>(writeDescriptorSets[swID].size()),
                           writeDescriptorSets[swID].data(),
                           0,
                           nullptr);
  };

  /// delete[] writeDescriptorSets;
  return true;
};


bool UniformVk::createWriteSets(VkDescriptorSet set,
                                std::vector<VkDescriptorBufferInfo> uinfo,
                                std::vector<VkDescriptorImageInfo> iinfo,
                                long update)
{

  if (desc.hach.id < 0) {
    fprintf(stderr, "there is no descriptorSets.\n");
    exit(-1);
  }
     

  if ((update > 0) || (update < 0)) {

    struct {
      uint32_t u;
      uint32_t t;
      uint32_t s;
    } idx = {uint32_t(-1), uint32_t(-1), uint32_t(-1)};

    size_t size = desc.type.size();

    writeDescriptorSets[swID].resize(size);

    GHOST_MAT_PRINTF("Descriptor WriteOut   Set %x   \n", set);

    for (uint32_t i = 0; i < size; i++) {

      if (desc.type[i] == 's') {
        idx.s++;
      }
      else if (desc.type[i] == 'u') {
        idx.u++;
        /// if (idx.u == 1) log_bad(" NIL  Create UBO   over limit. \n");
        writeDescriptorSets[swID][i] = {};
        writeDescriptorSets[swID][i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        writeDescriptorSets[swID][i].dstSet = set, writeDescriptorSets[swID][i].dstBinding = i,
        writeDescriptorSets[swID][i].descriptorCount = 1,
        writeDescriptorSets[swID][i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        writeDescriptorSets[swID][i].pBufferInfo = &(uinfo[idx.u]);
        // };
        GHOST_MAT_PRINTF("Descriptor WriteOut   Buffer %x   \n", uinfo[idx.u].buffer);
      }
      else if (desc.type[i] == 't') {
        idx.t++;
        writeDescriptorSets[swID][i] = {};
        writeDescriptorSets[swID][i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        writeDescriptorSets[swID][i].dstSet = set, writeDescriptorSets[swID][i].dstBinding = i,
        writeDescriptorSets[swID][i].descriptorCount = 1,
        writeDescriptorSets[swID][i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        writeDescriptorSets[swID][i].pImageInfo = &(iinfo[idx.t]);
        //};
        GHOST_MAT_PRINTF("Descriptor WriteOut   Image %u   %x   \n",
                iinfo[idx.t].imageLayout,
                iinfo[idx.t].imageView);
        }
      else {
           fprintf(stderr,"Descriptor_CreateIO::ParseError  \n");
      };
    }
  }

  if (update >= 0) {
    Upd[swID] = true;
    vkUpdateDescriptorSets(device,
                           static_cast<uint32_t>(writeDescriptorSets[swID].size()),
                           writeDescriptorSets[swID].data(),
                           0,
                           nullptr);

    GHOST_MAT_PRINTF("UPdateDescriptor [%s]   %zu  %x   \n",
            desc.type.data(),
            writeDescriptorSets[swID].size(),
            writeDescriptorSets[swID].data());
  };

  /// delete[] writeDescriptorSets;
  return true;
};



enum eConfigu_PIPE {
  MODE_TONEMAPPING,
  MODE_GEOMTEST,
  PIPE_MODE_ALL
};

extern constexpr UINT MAX_BLEND_OP = UINT(VK_BLEND_OP_BLUE_EXT) - (UINT)0x3b9d0c20;
extern constexpr UINT MAX_OVERLAP_OP = UINT(VK_BLEND_OVERLAP_CONJOINT_EXT) + 1;
struct Blend_ADV {
  VkBlendOp advance;
  VkBlendOverlapEXT overlap;
};

struct __Blend {
  Blend_ADV blend;
  VkColorComponentFlags component;
  float blendConstants[4];
};
namespace bop {

const char *String_VkBlendOp(VkBlendOp op)
{

#  define string_VkBlendOp(op) \
    case op: \
      return #op;
  switch (op) {
    default:
      string_VkBlendOp(VK_BLEND_OP_ADD) string_VkBlendOp(VK_BLEND_OP_SUBTRACT) string_VkBlendOp(
          VK_BLEND_OP_REVERSE_SUBTRACT) string_VkBlendOp(VK_BLEND_OP_MIN) string_VkBlendOp(VK_BLEND_OP_MAX)
          string_VkBlendOp(VK_BLEND_OP_ZERO_EXT) string_VkBlendOp(VK_BLEND_OP_SRC_EXT) string_VkBlendOp(
              VK_BLEND_OP_DST_EXT) string_VkBlendOp(VK_BLEND_OP_SRC_OVER_EXT) string_VkBlendOp(VK_BLEND_OP_DST_OVER_EXT)
              string_VkBlendOp(VK_BLEND_OP_SRC_IN_EXT) string_VkBlendOp(VK_BLEND_OP_DST_IN_EXT) string_VkBlendOp(
                  VK_BLEND_OP_SRC_OUT_EXT) string_VkBlendOp(VK_BLEND_OP_DST_OUT_EXT)
                  string_VkBlendOp(VK_BLEND_OP_SRC_ATOP_EXT) string_VkBlendOp(
                      VK_BLEND_OP_DST_ATOP_EXT) string_VkBlendOp(VK_BLEND_OP_XOR_EXT)
                      string_VkBlendOp(VK_BLEND_OP_MULTIPLY_EXT) string_VkBlendOp(
                          VK_BLEND_OP_SCREEN_EXT) string_VkBlendOp(VK_BLEND_OP_OVERLAY_EXT)
                          string_VkBlendOp(VK_BLEND_OP_DARKEN_EXT) string_VkBlendOp(
                              VK_BLEND_OP_LIGHTEN_EXT) string_VkBlendOp(VK_BLEND_OP_COLORDODGE_EXT)
                              string_VkBlendOp(VK_BLEND_OP_COLORBURN_EXT) string_VkBlendOp(
                                  VK_BLEND_OP_HARDLIGHT_EXT) string_VkBlendOp(VK_BLEND_OP_SOFTLIGHT_EXT)
                                  string_VkBlendOp(VK_BLEND_OP_DIFFERENCE_EXT) string_VkBlendOp(
                                      VK_BLEND_OP_EXCLUSION_EXT) string_VkBlendOp(VK_BLEND_OP_INVERT_EXT)
                                      string_VkBlendOp(VK_BLEND_OP_INVERT_RGB_EXT) string_VkBlendOp(
                                          VK_BLEND_OP_LINEARDODGE_EXT) string_VkBlendOp(VK_BLEND_OP_LINEARBURN_EXT)
                                          string_VkBlendOp(VK_BLEND_OP_VIVIDLIGHT_EXT) string_VkBlendOp(
                                              VK_BLEND_OP_LINEARLIGHT_EXT) string_VkBlendOp(VK_BLEND_OP_PINLIGHT_EXT)
                                              string_VkBlendOp(VK_BLEND_OP_HARDMIX_EXT) string_VkBlendOp(
                                                  VK_BLEND_OP_HSL_HUE_EXT) string_VkBlendOp(VK_BLEND_OP_HSL_SATURATION_EXT)
                                                  string_VkBlendOp(VK_BLEND_OP_HSL_COLOR_EXT) string_VkBlendOp(
                                                      VK_BLEND_OP_HSL_LUMINOSITY_EXT)
                                                      string_VkBlendOp(VK_BLEND_OP_PLUS_EXT) string_VkBlendOp(
                                                          VK_BLEND_OP_PLUS_CLAMPED_EXT)
                                                          string_VkBlendOp(
                                                              VK_BLEND_OP_PLUS_CLAMPED_ALPHA_EXT)
                                                              string_VkBlendOp(
                                                                  VK_BLEND_OP_PLUS_DARKER_EXT)
                                                                  string_VkBlendOp(VK_BLEND_OP_MINUS_EXT) string_VkBlendOp(
                                                                      VK_BLEND_OP_MINUS_CLAMPED_EXT)
                                                                      string_VkBlendOp(
                                                                          VK_BLEND_OP_CONTRAST_EXT)
                                                                          string_VkBlendOp(
                                                                              VK_BLEND_OP_INVERT_OVG_EXT)
                                                                              string_VkBlendOp(
                                                                                  VK_BLEND_OP_RED_EXT)
                                                                                  string_VkBlendOp(
                                                                                      VK_BLEND_OP_GREEN_EXT)
                                                                                      string_VkBlendOp(
                                                                                          VK_BLEND_OP_BLUE_EXT)
                                                                                          string_VkBlendOp(
                                                                                              VK_BLEND_OP_MAX_ENUM)
  };
  /// return "NOT-FONUD";
};

// constexpr
VkBlendOp getVkBlendOp(const uint32_t N) noexcept
{
  return VkBlendOp(N + (UINT)0x3b9d0c20);
};

// constexpr
UINT getVkBlendOpNum(const VkBlendOp op) noexcept
{
  return (UINT)op - (UINT)0x3b9d0c20;
};

const char *String_VkBlendOverlap(VkBlendOverlapEXT op)
{
#  define string_VkBlendOverlap(op) \
    case op: \
      return #op;
  switch (op) {
    default:
      string_VkBlendOverlap(VK_BLEND_OVERLAP_UNCORRELATED_EXT)
          string_VkBlendOverlap(VK_BLEND_OVERLAP_DISJOINT_EXT)
              string_VkBlendOverlap(VK_BLEND_OVERLAP_CONJOINT_EXT)
                  string_VkBlendOverlap(VK_BLEND_OVERLAP_MAX_ENUM_EXT)
  };
};

};  // namespace bop


typedef std::vector<VkPipelineShaderStageCreateInfo> &(*ShaderStagesTy)(void *);
typedef VkPipelineDynamicStateCreateInfo *(*DynamicTy)(void *);
typedef VkPipelineViewportStateCreateInfo *(*ViewportTy)(void *);
typedef VkPipelineMultisampleStateCreateInfo *(*MultisampleTy)(void *);
typedef VkPipelineInputAssemblyStateCreateInfo *(*InputAssemblyTy)(void *);
typedef VkPipelineRasterizationStateCreateInfo *(*RasterizationTy)(void *);
typedef VkPipelineDepthStencilStateCreateInfo *(*DepthStencilTy)(void *);
typedef VkPipelineColorBlendStateCreateInfo *(*ColorBlendTy)(void *);


struct PipelineStateCreateInfoVk {

  ShaderStagesTy ShaderStages;
  DynamicTy Dynamic;

  ViewportTy Viewport;
  MultisampleTy Multisample;

  InputAssemblyTy InputAssembly;
  RasterizationTy Rasterization;
  DepthStencilTy DepthStencil;
  ColorBlendTy ColorBlend;
};

struct MaterialVk {
  VkDevice  device;
  MaterialVk(VkDevice device_) : device(device_)
  {
    $DSL.setDevice(device_);
  };
  ~MaterialVk()
  {
    dealloc();
    __Delete__(descVk);
    $DSL.destroy();
  };
  struct Desc {
    LayoutType type;
    VkDeviceSize align;
    size_t hash;
    float reserveRatio;
  } desc;

    struct push4 {
    int tonemapper;
    float gamma;
    float exposure;
    float pad;
  } push4 = {2, 1., 1., 0.f};

  struct push5 {
    float size[2];
  } push5 = {{512.f, 512.f}};

  PipelineConfigure configu;
  Iache iach;
  SRWLOCK slim;
  bool scopedLock;
  eConfigu_PIPE mode;


  struct PIPE {
    VkPipeline pipe = VK_NULL_HANDLE;
  };

  struct PIPES {
    bool active = false;
    PIPE Next[eConfigu_PIPE::PIPE_MODE_ALL];
  };

  PIPES pipeRoot[MAX_BLEND_OP][MAX_OVERLAP_OP];

  __Blend makeblend;
__Blend Blend0;





  struct Raster {
    VkPolygonMode polygonMode;
    VkCullModeFlags cullMode;
    VkFrontFace frontFace;
    float lineWidth;
  };

  DescriptorVk *descVk;
  UniformVk uniform;

  struct {
    VkDeviceSize MaxSize;
    VkDeviceSize alignment;
    long refCnt;
    VkDeviceSize size;
  } cache;
  bool load_shader = false;

  std::function<void(void)> updateCB;

  VkShaderModule sh[2];
  PipelineStateCreateInfoVk PSci;


  void SetUp(eConfigu_PIPE _mode)
  {
    Blend0 = {{VK_BLEND_OP_SRC_OVER_EXT, VK_BLEND_OVERLAP_UNCORRELATED_EXT},
              VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                  VK_COLOR_COMPONENT_A_BIT,
              {1.f, 1.f, 1.f, 1.f}};

    mode = _mode;  /// FULLSCREEN2; ///
    init();
    arangeLayoutSet();
    createDraft();
  };
  void init();
  void initialState();
  //void loadPng(std::string name);
  //long Counting();
  long Allocate();
  template<class T> size_t hashLB(T const &s) noexcept
  {
    size_t h = 0;
    std::string type = s.type.data();
    size_t i = 0;
    for (char t : s.type) {
      h |= (size_t(t) - 115) << i * 2;
    };
    h |= (s.align << 16);
    return h;
  };

  bool arangeLayoutSet();
  void writeout(VkDescriptorBufferInfo camera = VkDescriptorBufferInfo());

  template<typename T> void writeout(T &desc)
  {


    std::vector<VkWriteDescriptorSet> write;
    write.clear();

    auto Set = uniform.descriptorSets[0];

    if ((mode == MODE_TONEMAPPING || mode == MODE_GEOMTEST )) {

      desc.Info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      VkWriteDescriptorSet wd = {};
      wd.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, wd.dstSet = Set[0], wd.dstBinding = 0,
      wd.dstArrayElement = 0, wd.descriptorCount = 1,
      wd.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, wd.pImageInfo = &desc.Info;
      write.push_back(wd);
      /*C++ 20
      write.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                       .dstSet = Set[0],
                       .dstBinding = 0,
                       .dstArrayElement = 0,
                       .descriptorCount = 1,
                       .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       .pImageInfo = &desc.Info});
      */
    }
    /* else if (mode == MODE_GEOMTEST)
      {
        VkWriteDescriptorSet wd = {};
        wd.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, wd.dstSet = Set[0], wd.dstBinding = 0,
        wd.dstArrayElement = 0, wd.descriptorCount = 1,
        wd.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, wd.pBufferInfo = &desc.bInfo;
        write.push_back(wd);
      }*/
    
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(write.size()), write.data(), 0, nullptr);
  };

  void createDraft();
  std::vector<VkPipelineShaderStageCreateInfo> &setShaderState(eConfigu_PIPE type);
  void dealloc();
  void setCommonInformation(VkGraphicsPipelineCreateInfo &pipelineCreateInfo);
#  if 1
  bool get(VkPipeline *&pipeline, eConfigu_PIPE prg, __Blend &blend);
  bool bind(VkCommandBuffer cmd, eConfigu_PIPE type);

  bool createPipeline(PipelineConfigure &config);
  bool createPipeline(eConfigu_PIPE _type, PipelineConfigure &config, __Blend &blend );
  bool $createExclusive(eConfigu_PIPE type,
                        __Blend &blend,
                        VkPrimitiveTopology topology,
                        Raster &raster,
                        VkGraphicsPipelineCreateInfo &pipelineCreateInfo);
  std::function<bool(VkCommandBuffer cmd, VkSemaphore sema)> make;
  //bool make(VkCommandBuffer cmd, VkSemaphore sema);
  bool make_fullscreen(VkCommandBuffer cmd);
  bool make_tonemapping(VkCommandBuffer cmd);

  template<typename T> bool make_geomtest(VkCommandBuffer cmd, T &vinfo,uint32_t count)
  {
    VkDeviceSize offsets[1] = {0};
    VkPipeline *pipe = nullptr;
    if (!(get(pipe, mode, makeblend))) {
      fprintf(stderr, "Not Found  Pipeline Instances  \n");
      exit(-1);
    }

    VkPipelineLayout draft = descVk->draft;
    VkShaderStageFlags pushStage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipe);

    vkCmdBindVertexBuffers(cmd, 0, 1, &(vinfo.buffer), offsets);

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draft, 0, 1, &uniform.descriptorSets[0][0], 0, 0);
    size_t psize = sizeof(push4);
    void *pptr = (void *)&push4;

    vkCmdPushConstants(cmd, draft, pushStage, 0, psize, pptr);
    // vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdDraw(cmd, count, 1, 0, 0);

    return true;
  }

  //bool make(VkCommandBuffer cmd, const std::vector<Object3D *> &child, uint32_t drawCount);
};

void MaterialVk::init()
{

  memset(&pipeRoot, 0, sizeof(pipeRoot));
  memset(sh, 0, sizeof(sh));
  configu.multisample = 2;

  InitializeSRWLock(&slim);
  initialState();
  descVk = new DescriptorVk(device);
  uniform.device = device;
};


void MaterialVk::initialState()
{

  configu.vkRP = VK_NULL_HANDLE;

  makeblend = Blend0;

  PSci.Viewport = [](void *) {
    static VkPipelineViewportStateCreateInfo  pipelineViewportStateCreateInfo = {};
    pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    pipelineViewportStateCreateInfo.viewportCount = 1;
    pipelineViewportStateCreateInfo.scissorCount = 1;
    pipelineViewportStateCreateInfo.flags = 0;
    return &pipelineViewportStateCreateInfo;
  };
  /*PSci.ColorBlend = [](void* wcolor) {

    ///bool tf = *((bool*)wcolor);
    static VkPipelineColorBlendAttachmentState blendAttachmentState =
vka::plysm::pipelineColorBlendAttachmentState(0xf, VK_FALSE);

    static VkPipelineColorBlendStateCreateInfo  Info = {
.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
///.pNext = NULL,
.flags = 0 ,
.logicOpEnable = VK_FALSE,
///.logicOp,
.attachmentCount = 1,
.pAttachments = &blendAttachmentState,
.blendConstants = { 1.f,1.f,1.f,1.f}
    };
    return &Info;
  };*/

  PSci.ColorBlend = [](void *args) mutable {
    __Blend *flag = (__Blend *)args;

    static VkPipelineColorBlendAttachmentState attach = {};
        attach.blendEnable = VK_TRUE,
    ///.srcColorBlendFactor,
    ///.dstColorBlendFactor,
        attach.colorBlendOp = flag->blend.advance,
    ///.srcAlphaBlendFactor,
    /// .dstAlphaBlendFactor,
        attach.alphaBlendOp = flag->blend.advance,
        attach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  //};
    attach.colorBlendOp = flag->blend.advance;
    attach.alphaBlendOp = flag->blend.advance;
    attach.colorWriteMask = flag->component;

    static VkPipelineColorBlendAdvancedStateCreateInfoEXT adInfo = {};
        adInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT,
        adInfo.pNext = NULL, adInfo.srcPremultiplied = VK_TRUE, adInfo.dstPremultiplied = VK_TRUE,
        adInfo.blendOverlap = flag->blend.overlap;
      //};

    adInfo.blendOverlap = flag->blend.overlap;

    static VkPipelineColorBlendStateCreateInfo Info = {};
        Info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, Info.pNext = &adInfo,
        Info.flags = 0,
    ///.logicOpEnable,
    ///.logicOp,
        Info.attachmentCount = 1, Info.pAttachments = &attach;
        for (int i = 0;i<4;i++)Info.blendConstants[i] = 1.f;
      //};

          Info.pNext = &adInfo;
          Info.pAttachments = &attach;

    memcpy(Info.blendConstants, flag->blendConstants, 4 * 4);

    return &Info;
  };
  PSci.DepthStencil = [](void *null) {
    static VkPipelineDepthStencilStateCreateInfo Info{};
    Info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    Info.depthTestEnable = VK_TRUE, Info.depthWriteEnable = VK_TRUE,
    Info.depthCompareOp = VK_COMPARE_OP_LESS, Info.depthBoundsTestEnable = VK_FALSE,
    Info.stencilTestEnable = VK_FALSE, Info.minDepthBounds = 0.0f, Info.maxDepthBounds = 1.0f;
    //};
    return &Info;
  };
  PSci.Dynamic = [](void *) {
    static std::vector<VkDynamicState> dynamicStateEnables = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH};
    static VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
    pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();
    pipelineDynamicStateCreateInfo.dynamicStateCount = dynamicStateEnables.size();
    pipelineDynamicStateCreateInfo.flags = 0;
    
    return &pipelineDynamicStateCreateInfo;
  };

  PSci.Multisample = [](void *) {
    static uint32_t nSampleMask = 0xFFFFFFFF;
    uint32_t multisample = 2;
    static VkPipelineMultisampleStateCreateInfo Info{};
    Info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    Info.rasterizationSamples = (VkSampleCountFlagBits)multisample,
    Info.sampleShadingEnable = VK_FALSE, Info.minSampleShading = 0.0f,
    Info.pSampleMask = &nSampleMask;
    
    return &Info;
  };

  PSci.Rasterization = [](void *_conf) mutable {
    Raster *conf = (Raster *)_conf;
    /// VkCullModeFlags* cullMode = (VkCullModeFlags*)cull;
    static VkPipelineRasterizationStateCreateInfo Info{};
    Info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    Info.flags = 0;


    Info.depthClampEnable = VK_TRUE;
    Info.depthBiasEnable = VK_FALSE;
    Info.depthBiasConstantFactor = 0.0;
    Info.depthBiasSlopeFactor = 0.0f;
    Info.depthBiasClamp = 0.0f;

    Info.polygonMode = conf->polygonMode;
    Info.cullMode = conf->cullMode;
    Info.frontFace = conf->frontFace;
    Info.lineWidth = conf->lineWidth;

    return &Info;
  };
  PSci.InputAssembly = [](void *top) mutable {
    VkPrimitiveTopology *topology = (VkPrimitiveTopology *)top;
    static VkPipelineInputAssemblyStateCreateInfo Info = {};
    Info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, Info.flags = 0,
    Info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, Info.primitiveRestartEnable = VK_FALSE;
    //};
    Info.topology = *topology;
    return &Info;
  };
};



#if 0
void MaterialVk::loadPng(std::string name)
{

  names->imageName = name;
  iach = Iache::rehash(names->imageName, material_com::getStamp());
  iach.format = VK_FORMAT_R8G8B8A8_SRGB;
  iach.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  iach.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

  ImagesVk *imgVk = nullptr;
  if (imgVk == nullptr) {
    if (!$tank.takeout(imgVk, 0)) {
      log_bad("tank failed to take out . \n");
    };
  };
  ImmidiateCmd<ImmidiateCmdPool> imcmVk;
  imgVk->createFromFile(imcmVk, names->imageName, iach);
}
#endif



void MaterialVk::createDraft()
{



  VkPushConstantRange pushRange = {};
  #if 0
  if (mode == eConfigu_PIPE::MODE_SELECT || mode == eConfigu_PIPE::MODE_SKY) {
    pushRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(push),
    };
  }
  else if (mode == eConfigu_PIPE::MODE_FULLSCREEN) {
    pushRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(push2),
    };
  }
  else if (mode == MODE_FULLSCREEN2) {

    pushRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(push3),
    };
  }
  else
    #endif
    if (mode == MODE_TONEMAPPING) {

      pushRange = {};
      pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      pushRange.offset = 0, pushRange.size = sizeof(push4);
      //};
  }
  else if (mode == MODE_GEOMTEST) {

      pushRange = {};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    pushRange.offset = 0, pushRange.size = sizeof(push4);
    //};
  }

  descVk->createDraft({pushRange});
};
/*
long MaterialVk::Counting()
{
  return InterlockedAdd(&cache.refCnt, 1) - 1;
};
*/

long MaterialVk::Allocate()
{
   /*
  static VisibleObjectsVk *vobjVk = nullptr;
  if (vobjVk == nullptr) {
    if (!$tank.takeout(vobjVk, 0)) {
      log_bad(" not found  VisibleUniformObjectsVk.");
    };
   
  };

  const VkDeviceSize maxChunk = 512 * 1024 * 1024;
  VkPhysicalDeviceProperties properties;

  vkGetPhysicalDeviceProperties($physicaldevice, &properties);
  VkPhysicalDeviceLimits &limits = properties.limits;
  VkDeviceSize _alignment =
      limits.minUniformBufferOffsetAlignment;  //, limits.minStorageBufferOffsetAlignment);
  VkDeviceSize multiple = 1;

  while (true) {
    if (desc.align < (_alignment * multiple)) {
      break;
    }
    multiple++;
  };

  const VkDeviceSize structMax = VkDeviceSize(cache.refCnt) * desc.align;

  cache.alignment = multiple * _alignment;
  cache.MaxSize = __min(structMax, maxChunk);

  uniform.swID = 0;
  uniform.createUBO(vobjVk, cache.alignment * cache.refCnt);
  uniform.ubo.info.range = cache.alignment;
  */
  return 0;
};
bool MaterialVk::arangeLayoutSet()
{

  //if (mode == MODE_FULLSCREEN)
  //  return true;

  bool draft = (descVk->layoutSet.size() == 0);
  std::vector<VkDescriptorSetLayoutBinding> Set(1);

  //if ((mode == MODE_FULLSCREEN2) |
  if (mode == MODE_TONEMAPPING || mode == MODE_GEOMTEST) {

    Set[0].binding = 0, Set[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    Set[0].descriptorCount = 1, Set[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    Set[0].pImmutableSamplers = NULL;

    BLI_assert(0 == uniform.createSet(descVk, "CombFrag", Set, draft));
    return true;
  }
  else  {

    Set[0].binding = 0, Set[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    Set[0].descriptorCount = 1, Set[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    Set[0].pImmutableSamplers = NULL;
    BLI_assert(0 == uniform.createSet(descVk, "Ssbo", Set, draft));
    return true;
  }

  Set[0].binding = 0, Set[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
  Set[0].descriptorCount = 1, Set[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
  Set[0].pImmutableSamplers = NULL;

  BLI_assert(0 == uniform.createSet(descVk, "GLOBAL", Set, draft));

  Set[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  Set[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  /*
  if (mode == MODE_SKY) {
    Set.push_back({.binding = 1,
                   .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   .descriptorCount = 1,
                   .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                   .pImmutableSamplers = NULL});
  }
  */

  BLI_assert(1 == uniform.createSet(descVk, "OBJECT", Set, draft));

  desc.hash = hashLB(desc);

  return true;
};
void MaterialVk::writeout(VkDescriptorBufferInfo camera)
{
  /*


  static std::vector<VkWriteDescriptorSet> write;
  static VkDescriptorImageInfo info;
  auto Set = uniform.descriptorSets[0];

  if ((mode == MODE_TONEMAPPING) ) {

    material_com::getInfo(iach, info);
    write.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                     .dstSet = Set[0],
                     .dstBinding = 0,
                     .dstArrayElement = 0,
                     .descriptorCount = 1,
                     .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                     .pImageInfo = &info});
  }
  else {

    write.resize(2);

    write[0] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = Set[0],
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &camera,
    };

    write[1] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = Set[1],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                .pBufferInfo = &uniform.ubo.info};

    if (mode == MODE_SKY) {

      material_com::getInfo(iach, info);
      write.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                       .dstSet = Set[1],
                       .dstBinding = 1,
                       .dstArrayElement = 0,
                       .descriptorCount = 1,
                       .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       .pImageInfo = &info});
    }
  }

  vkUpdateDescriptorSets($device, static_cast<uint32_t>(write.size()), write.data(), 0, nullptr);
  */
};


std::vector<VkPipelineShaderStageCreateInfo> &MaterialVk::setShaderState(eConfigu_PIPE mode)
{

  if (!load_shader) {

    std::string spv;
    VkShaderModule *shaders = sh;
    uint32_t stages = 2;

    std::vector<eConfigu_PIPE> pipes;
    pipes = {mode};

    for (auto type : pipes) {
      /*
      if ((type == MODE_SKY || type == eConfigu_PIPE::MODE_SELECT) && sh[0] == VK_NULL_HANDLE) {
        shaders = sh;
        spv = getAssetPath() + names->spv;
      }
      if ((type == eConfigu_PIPE::MODE_FULLSCREEN) && sh[0] == VK_NULL_HANDLE) {
        shaders = sh;
        spv = getAssetPath() + "fullscreen//prg";
      }
      if ((type == eConfigu_PIPE::MODE_FULLSCREEN2) && sh[0] == VK_NULL_HANDLE) {
        shaders = sh;
        spv = getAssetPath() + "fullscreen//prg3";
      }
      */
      if ((type == eConfigu_PIPE::MODE_TONEMAPPING) && sh[0] == VK_NULL_HANDLE) {
        shaders = sh;
        spv = getAssetPath() + "tonemapping";
      }
      if ((type == eConfigu_PIPE::MODE_GEOMTEST) && sh[0] == VK_NULL_HANDLE) {
        shaders = sh;
        spv = getAssetPath() + "geomtest";
      }

      int j = 0;
      shaders[j++] = loadShader(device,spv + ".vert.spv", VK_SHADER_STAGE_VERTEX_BIT).module;
      if (stages == 3)
        shaders[j++] = loadShader(device, spv + ".geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT).module;
      if (stages == 4)
        shaders[j++] =
            loadShader(device, spv + ".tesc.spv",
                                              VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
                           .module;
      if (stages == 4)
        shaders[j++] = loadShader(
                           device, spv + ".tese.spv",
                                              VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
                           .module;
      shaders[j++] = loadShader(device, spv + ".frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT).module;
    };

    load_shader = true;
  }

  static std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

  switch (mode) {
    #if 0
    case MODE_SKY:
    case MODE_SELECT:
    case MODE_FULLSCREEN:
    case MODE_FULLSCREEN2:
      #endif
    case MODE_TONEMAPPING:
    case MODE_GEOMTEST:
    case PIPE_MODE_ALL:
      shaderStages.resize(2);

      shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      shaderStages[0].module = sh[0];
      shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      shaderStages[1].module = sh[1];
      break;
  };

  for (auto &v : shaderStages) {
    v.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    v.pName = "main";
  };

  return shaderStages;
};
void MaterialVk::setCommonInformation(VkGraphicsPipelineCreateInfo &pipelineCreateInfo)
{

  pipelineCreateInfo = {};

   pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
   pipelineCreateInfo.layout = configu.vkPL, pipelineCreateInfo.renderPass = configu.vkRP;

   pipelineCreateInfo.pVertexInputState = configu.vkPVISci;

  if ((mode == eConfigu_PIPE::MODE_TONEMAPPING || mode == eConfigu_PIPE::MODE_GEOMTEST)) {

    static VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};
    pipelineMultisampleStateCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;  //VK_SAMPLE_COUNT_8_BIT;
       
    pipelineMultisampleStateCreateInfo.flags = 0;
    pipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;

  }
  else {
    pipelineCreateInfo.pMultisampleState = PSci.Multisample(nullptr);
  }

  pipelineCreateInfo.pViewportState = PSci.Viewport(nullptr);
  pipelineCreateInfo.pDynamicState = PSci.Dynamic(nullptr);
  // pipelineCreateInfo.pColorBlendState = PSci.ColorBlend(nullptr);
  pipelineCreateInfo.pDepthStencilState = PSci.DepthStencil(nullptr);
  GHOST_MAT_PRINTF("create GPci   %x     ", pipelineCreateInfo);
};

void MaterialVk::dealloc()
{

  for (auto &shaderModule : sh) {
    if (shaderModule != VK_NULL_HANDLE) {
      vkDestroyShaderModule(device, shaderModule, nullptr);
      shaderModule = VK_NULL_HANDLE;
    }
  };
  for (int i = 0; i < MAX_BLEND_OP; i++) {
    for (int j = 0; j < MAX_OVERLAP_OP; j++) {
      for (auto &p : pipeRoot[i][j].Next) {
        vkDestroyPipeline(device, p.pipe, nullptr);
        p.pipe = VK_NULL_HANDLE;
      };
    };
  };

  __Delete__(descVk);
};
bool MaterialVk::get(VkPipeline *&pipeline, eConfigu_PIPE prg, __Blend &flag)
{

  pipeline = &pipeRoot[bop::getVkBlendOpNum(flag.blend.advance)][(UINT)flag.blend.overlap]
                  .Next[(UINT)prg]
                  .pipe;
  if (*pipeline == VK_NULL_HANDLE) {
    if (configu.vkRP == VK_NULL_HANDLE)
      return false;
    return createPipeline(prg, configu, flag);
  }
  return true;
};
bool MaterialVk::bind(VkCommandBuffer cmd, eConfigu_PIPE type)
{

  VkPipeline *pipe;
  if (get(pipe, type,Blend0)) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipe);
  }
  else {
    fprintf(stderr, " there is no valid pipeline. type %u  \n", (UINT)type);
    exit(-1);
  }

  return true;
}
bool MaterialVk::createPipeline(PipelineConfigure &config)
{
  return createPipeline(mode, config, Blend0);
};
bool MaterialVk::createPipeline(eConfigu_PIPE _type, PipelineConfigure &config, __Blend &blend)
{

  float supersample = 64.f;
  configu.vkPL = descVk->draft;
  configu.vkRP = config.vkRP;
  configu.vkPVISci = config.vkPVISci;
  configu.vkPC = config.vkPC;

  MaterialVk::Raster rsterFill = {};
  rsterFill.polygonMode = VK_POLYGON_MODE_FILL,
  rsterFill.cullMode = VK_CULL_MODE_NONE,  // VK_CULL_MODE_BACK_BIT,
      rsterFill.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  //};

  MaterialVk::Raster rsterLine = {};
  rsterLine.polygonMode = VK_POLYGON_MODE_LINE,  // VK_POLYGON_MODE_FILL,
  rsterLine.cullMode = VK_CULL_MODE_BACK_BIT,
  rsterLine.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, rsterLine.lineWidth = supersample;


  VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};



  switch (mode) {
    #if 0
    case MaterialVk::MODE_SKY:
    case MaterialVk::MODE_SELECT:
      $createExclusive(
          mode, blend, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, rsterFill, pipelineCreateInfo);
      break;

    case MaterialVk::MODE_FULLSCREEN:
      $createExclusive(
          mode, blend, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, rsterFill, pipelineCreateInfo);
      break;

    case MaterialVk::MODE_FULLSCREEN2:
      #endif
    case eConfigu_PIPE::MODE_TONEMAPPING:
      $createExclusive(
          mode, blend, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, rsterFill, pipelineCreateInfo);
      break;
    case eConfigu_PIPE::MODE_GEOMTEST:
    case PIPE_MODE_ALL:
      $createExclusive(
          mode, blend, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, rsterFill, pipelineCreateInfo);
      break;
  };

  return true;
};
bool MaterialVk::$createExclusive(eConfigu_PIPE type,
                                   __Blend &blend,
                                   VkPrimitiveTopology topology,
                                   Raster &raster,
                                   VkGraphicsPipelineCreateInfo &pipelineCreateInfo)
{

  bool release = false;
  if (!scopedLock) {
    AcquireSRWLockExclusive(&slim);
    release = scopedLock = true;
  }

  VkPipeline *pipe =
      &pipeRoot[bop::getVkBlendOpNum(blend.blend.advance)][(UINT)blend.blend.overlap]
           .Next[(UINT)type]
           .pipe;
  if (*pipe != VK_NULL_HANDLE) {
    if (release) {
      scopedLock = false;
      ReleaseSRWLockExclusive(&slim);
    }
    return true;
  }

  setCommonInformation(pipelineCreateInfo);
  auto &shader = setShaderState(type);

  pipelineCreateInfo.pColorBlendState = PSci.ColorBlend(&blend);
  if (
      (type == eConfigu_PIPE::MODE_TONEMAPPING)) {
    static VkPipelineVertexInputStateCreateInfo vinfo;
    vinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    vinfo.vertexBindingDescriptionCount = 0, vinfo.vertexAttributeDescriptionCount = 0;
    pipelineCreateInfo.pVertexInputState = &vinfo;
  }
  else {
    pipelineCreateInfo.pVertexInputState = configu.vkPVISci;
  }
  pipelineCreateInfo.pStages = shader.data();
  pipelineCreateInfo.stageCount = (uint32_t)shader.size();
  pipelineCreateInfo.pInputAssemblyState = PSci.InputAssembly(&topology);
  pipelineCreateInfo.pRasterizationState = PSci.Rasterization(&raster);

  VK_CHECK(
      vkCreateGraphicsPipelines(device, configu.vkPC, 1, &pipelineCreateInfo, nullptr, pipe));
  // VK_CHECK_RESULT(vkCreateGraphicsPipelines($device, VK_NULL_HANDLE, 1, &pipelineCreateInfo,
  // nullptr, pipe));
  pipeRoot[bop::getVkBlendOpNum(blend.blend.advance)][(UINT)blend.blend.overlap]
      .Next[(UINT)type]
      .pipe = *pipe;
  printf("Create Pipeline    %llx    %s     \n ",
         (long long)(*pipe),
         bop::String_VkBlendOp(blend.blend.advance));

  if (release) {
    release = scopedLock = false;
    ReleaseSRWLockExclusive(&slim);
  }

  return true;
};


bool MaterialVk::make_fullscreen(VkCommandBuffer cmd)
{
  /*
  VkPipeline *pipe = nullptr;
  if (!(get(pipe, mode, makeblend)))
    log_bad("Not Found  Pipeline Instances  \n");

  VkPipelineLayout draft = descVk->draft;
  VkShaderStageFlags pushStage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipe);

  /// align = (UINT)(obj->draw.pid * cache.alignment);
  /// obj->draw.mapped = (void*)((char*)uniform.ubo.mapped + align);
  for (int i = 0; i < 3; i++)
    push2.color[i] = 0.5f;
  vkCmdPushConstants(cmd, draft, pushStage, 0, sizeof(push2), &push2);
  vkCmdDraw(cmd, 4, 1, 0, 0);
  */
  return true;
}
bool MaterialVk::make_tonemapping(VkCommandBuffer cmd)
{

  VkPipeline *pipe = nullptr;
  if (!(get(pipe, mode, makeblend))) {
    fprintf(stderr, "Not Found  Pipeline Instances  \n");
    exit(-1);
  }

  VkPipelineLayout draft = descVk->draft;
  VkShaderStageFlags pushStage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipe);

  vkCmdBindDescriptorSets(
      cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draft, 0, 1, &uniform.descriptorSets[0][0], 0, 0);
  size_t psize = 0;
  void *pptr = nullptr;
    #if 0
  if (mode == eConfigu_PIPE::MODE_FULLSCREEN2) {
    psize = sizeof(push3);
    pptr = &push3;
  }
  else
    #endif
    if (mode == eConfigu_PIPE::MODE_TONEMAPPING ) {
    psize = sizeof(push4);
    pptr = (void *)&push4;
  }

  vkCmdPushConstants(cmd, draft, pushStage, 0, psize, pptr);
  //vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdDraw(cmd, 4, 1, 0, 0);

  return true;
}


#endif


#  pragma warning(pop)
#endif
