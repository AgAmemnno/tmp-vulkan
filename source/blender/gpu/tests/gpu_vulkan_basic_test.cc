/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_threads.h"
#include "GPU_capabilities.h"
#include "GPU_compute.h"
#include "GPU_index_buffer.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_vertex_buffer.h"
#include "GPU_vertex_format.h"
#include "intern/gpu_vertex_buffer_private.hh"
#include "GPU_batch_presets.h"
#include "GPU_init_exit.h" /* interface */
#include "MEM_guardedalloc.h"

#include "gpu_testing.hh"

#include "CLG_log.h"

#include "GPU_context.h"

#include "GHOST_C-api.h"


#include   "intern/GHOST_SystemWin32.h"
#include   "intern/GHOST_ContextVK.h"
#include "intern/GHOST_Window.h"
#include   "intern/GHOST_MaterialVK.hh"
#include <limits.h>
#include "gtest/gtest.h"




namespace blender::gpu::tests {
GHOST_SystemHandle ghost_system;
GHOST_ContextHandle ghost_context;
GHOST_WindowHandle ghost_window;
struct GPUContext *context;
GHOST_TDrawingContextType draw_context_type = GHOST_kDrawingContextTypeNone;


static GHOST_ContextHandle test_ghost_context(VULKAN_DEBUG_TYPE ext_type, int mode)
{

  ghost_system = GHOST_CreateSystem();
  GHOST_GLSettings glSettings = {0};
#ifdef WITH_VULKAN_BACKEND

#  ifdef WITH_OPENGL_BACKEND
#    undef WITH_OPENGL_BACKEND
#  endif
  GPU_backend_type_selection_set(GPU_BACKEND_VULKAN);
  glSettings.context_type = GHOST_kDrawingContextTypeVulkan;
#endif
  glSettings.flags |= GHOST_glDebugContext;
  const bool debug_context = (glSettings.flags & GHOST_glDebugContext) != 0;

  #ifdef WITH_VULKAN_BACKEND
   GHOST_ContextVK *context = new GHOST_ContextVK(false, (HWND)0, 1, 0, debug_context, ext_type);

    if (!context->initializeDrawingContext()) {
      delete context;
      return (GHOST_ContextHandle)nullptr;
    }

    context->Check_Context(mode);
    return (GHOST_ContextHandle)context;
    #else
  return (GHOST_ContextHandle) nullptr;
    #endif



};

static GHOST_WindowHandle test_ghost_window(VULKAN_DEBUG_TYPE ext_type, int mode)
{


  ghost_system = GHOST_CreateSystem();
  GHOST_GLSettings glSettings = {0};

#ifdef WITH_VULKAN_BACKEND

#  ifdef WITH_OPENGL_BACKEND
#    undef WITH_OPENGL_BACKEND
#  endif
  GPU_backend_type_selection_set(GPU_BACKEND_VULKAN);
  glSettings.context_type = GHOST_kDrawingContextTypeVulkan;
  #else
  GPU_backend_type_selection_set(GPU_BACKEND_OPENGL);
  glSettings.context_type = GHOST_kDrawingContextTypeOpenGL;
#endif

  glSettings.flags |= GHOST_glDebugContext;
  const bool debug_context = (glSettings.flags & GHOST_glDebugContext) != 0;
  GHOST_WindowHandle ghostwin = GHOST_CreateWindow(ghost_system,
                                                   NULL,
                                                   "Blender gpu_test",
                                                   0,
                                                   0,
                                                   1920,
                                                   1080,
                                                   (GHOST_TWindowState)1,
                                                   false,
                                                   glSettings);



 return ghostwin;

}

static void test_CreateVulkanContext()
{
  

  GHOST_ActivateOpenGLContext(ghost_context);
  context = GPU_context_create(ghost_window, ghost_context);

  // GPU_init();
  return;
}


void test_Destroy()
{
  
  if (context) {
    GPU_context_discard(context);
  };
  context = nullptr;
  if (ghost_system) {
    if (ghost_context) {

      GHOST_DisposeOpenGLContext(ghost_system, ghost_context);
      ghost_context = nullptr;
    }
    GHOST_DisposeSystem(ghost_system);
    ghost_system = nullptr;
  }
  CLG_exit();
}

#if 0
TEST(ContextVk, RenderBegin)
{
  /*
  BLI_assert(17 < 16);

  #ifdef DEBUG
    printf("\n");
    printf("GL: Forcing workaround usage and disabling extensions.\n");
    printf("    OpenGL identification strings\n");
  #endif

  fprintf(stderr, "GPU failed to find function %s\n", "name");
  */
  CLG_init();
  ghost_window = test_ghost_window(VULKAN_DEBUG_UTILS, 0);
  if (ghost_window && ghost_context == NULL) {
    GHOST_Window *ghostWin = reinterpret_cast<GHOST_Window *>(ghost_window);
    ghost_context = (GHOST_ContextHandle)((ghostWin ? ghostWin->getContext() : NULL));
  }
  BLI_assert(ghost_context);
  test_CreateVulkanContext();
  GPU_render_begin();
  GPU_init();


  GPU_exit();
  test_Destroy();
}
#endif

#if 1
TEST(ContextVk, Pipeline)
{
  CLG_init();
  ghost_window = test_ghost_window(VULKAN_DEBUG_UTILS, 0);
  if (ghost_window && ghost_context == NULL) {
    GHOST_Window *ghostWin = reinterpret_cast<GHOST_Window *>(ghost_window);
    ghost_context = (GHOST_ContextHandle)((ghostWin ? ghostWin->getContext() : NULL));
  }
  BLI_assert(ghost_context);
  GHOST_ContextVK *context = (GHOST_ContextVK *)ghost_context;
  VkDevice device = context->getDevice();

  test_CreateVulkanContext();
  GPU_render_begin();
  GPU_init();


  VKCommandBufferManager *cmm = new VKCommandBufferManager(device);
  cmm->prepare(context);
  MBIVSIvk desc(device);
  createImage(*cmm, desc, 32, 32);

   auto tone = new MaterialVk(device);
   tone->SetUp(MODE_TONEMAPPING);

   PipelineConfigure config = {};
   VkImage image;
   VkFramebuffer framebuffer;
   VkCommandBuffer command_buffer;
   uint32_t fb_id;
   VkExtent2D extent;
       
   context->getVulkanBackbuffer((void *)&image,
                                (void *)&framebuffer,
                                (void *)&command_buffer,
                                (void *)&config.vkRP,
                                (void *)&extent,
                                (uint32_t *)&fb_id);
   //config.vkRP = swapChain->renderPass, config.vkPC = swapChain->pipelineCache;
  tone->createPipeline(config);
  tone->writeout(desc);
  tone->make = [&](VkCommandBuffer cmd, VkSemaphore sema) -> bool {
    tone->make_tonemapping(cmd);
    return true;
  };

   for (int i = 0; i < 12; i++) {

    context->commandLoop(*tone);
    context->swapBuffers();
  }

  desc.dealloc();
  delete cmm;
   delete tone;
  GPU_exit();
  test_Destroy();

};

#endif

#if 1
TEST(ContextVk, Input)
{
  BLI_threadapi_init();
  CLG_init();
  ghost_window = test_ghost_window(VULKAN_DEBUG_UTILS, 0);
  if (ghost_window && ghost_context == NULL) {
    GHOST_Window *ghostWin = reinterpret_cast<GHOST_Window *>(ghost_window);
    ghost_context = (GHOST_ContextHandle)((ghostWin ? ghostWin->getContext() : NULL));
  }
  BLI_assert(ghost_context);
  GHOST_ContextVK *context = (GHOST_ContextVK *)ghost_context;
  VkDevice device = context->getDevice();

  test_CreateVulkanContext();
  GPU_render_begin();
  GPU_init();
  /*
  typedef struct GPUBatch {
    // verts[0] is required, others can be NULL 
    GPUVertBuf *verts[GPU_BATCH_VBO_MAX_LEN];
    // Instance attributes. 
    GPUVertBuf *inst[GPU_BATCH_INST_VBO_MAX_LEN];
    // NULL if element list not needed 
    GPUIndexBuf *elem;
    // Resource ID attribute workaround. 
    GPUStorageBuf *resource_id_buf;
    // Bookkeeping. 
    eGPUBatchFlag flag;
    // Type of geometry to draw.
    GPUPrimType prim_type;
    /// Current assigned shader. DEPRECATED. Here only for uniform binding. 
    struct GPUShader *shader;
  } GPUBatch;
  */


  VKCommandBufferManager *cmm = new VKCommandBufferManager(device);
  cmm->prepare(context);
  MBIVSIvk desc(device);
  createImage(*cmm, desc, 32, 32);




  GPUBatch *   sphere =   GPU_batch_preset_sphere(1);
  GPUVertBuf *verts_ = sphere->verts[0];
  VertBuf *verts =  reinterpret_cast<VertBuf *>(verts_);
 
  const GPUVertFormat *format = &verts->format;
  const GPUVertAttr *a = &format->attrs[0];
  BLI_assert(0 < format->attr_len);
  BLI_assert(verts->data != nullptr);

  //verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  //verts->flag &= ~GPU_VERTBUF_DATA_UPLOADED;


  GPUVertBufRaw access;
  access.size = verts->vertex_len * format->stride;
  access.stride = format->stride;
  access.data = (uchar *)verts->data;
  access.data_init = access.data;
  MBvk input(cmm->device);


   $createBuffer$(input,*cmm, access);
   VInfo.$set$("Preset",format);
   vkPVISci &vinfo = VInfo.Info["Preset"];
   
   auto tone = new MaterialVk(device);
   tone->SetUp(MODE_GEOMTEST);
   tone->make = [&](VkCommandBuffer cmd, VkSemaphore sema) -> bool {
     tone->make_geomtest(cmd, input, verts->vertex_len);
     return true;
   };
  PipelineConfigure config = {};
  VkImage image;
  VkFramebuffer framebuffer;
  VkCommandBuffer command_buffer;
  uint32_t fb_id;
  VkExtent2D extent;

  context->getVulkanBackbuffer((void *)&image,
                               (void *)&framebuffer,
                               (void *)&command_buffer,
                               (void *)&config.vkRP,
                               (void *)&extent,
                               (uint32_t *)&fb_id);
  config.vkPVISci = &vinfo.info;
  tone->createPipeline(config);
  tone->writeout(desc);

  for (int i = 0; i < 12; i++) {

    context->commandLoop(*tone);
    context->swapBuffers();
  }






   input.dealloc();
   desc.dealloc();
   delete cmm;
   delete tone;

  GPU_exit();
  test_Destroy();
  BLI_threadapi_exit();
};

#endif



#if 0
TEST(ContextVk, Window)
{
  /*
  BLI_assert(17 < 16);

  #ifdef DEBUG
    printf("\n");
    printf("GL: Forcing workaround usage and disabling extensions.\n");
    printf("    OpenGL identification strings\n");
  #endif

  fprintf(stderr, "GPU failed to find function %s\n", "name");
  */
  CLG_init();
  ghost_window = test_ghost_window(VULKAN_DEBUG_UTILS, 0);
  if (ghost_window && ghost_context == NULL) {
    GHOST_Window *ghostWin = reinterpret_cast<GHOST_Window *>(ghost_window);
    ghost_context = (GHOST_ContextHandle) ((ghostWin ? ghostWin->getContext() : NULL));
  }
  BLI_assert(ghost_context);
  test_CreateVulkanContext();

  test_Destroy();
}

TEST(ContextVk, DEBUG_REPORT)
{
  CLG_init();

  ghost_context = test_ghost_context(VULKAN_DEBUG_REPORT,0);
  test_CreateVulkanContext();

  test_Destroy();

}

TEST(ContextVk, DEBUG_UTILS)
{

  CLG_init();
  ghost_context = test_ghost_context(VULKAN_DEBUG_UTILS,0);
  test_CreateVulkanContext();

  test_Destroy();

}

TEST(ContextVk, DEBUG_BOTH)
{

  CLG_init();
  ghost_context = test_ghost_context(VULKAN_DEBUG_BOTH, 0);
  test_CreateVulkanContext();

  test_Destroy();
}


TEST(ContextVk, DEBUG_REPORT2)
{
  CLG_init();

  ghost_context = test_ghost_context(VULKAN_DEBUG_REPORT, 1);
  test_CreateVulkanContext();

  test_Destroy();
}

TEST(ContextVk, DEBUG_UTILS2)
{

  CLG_init();
  ghost_context = test_ghost_context(VULKAN_DEBUG_UTILS, 1);
  test_CreateVulkanContext();

  test_Destroy();
}
#endif

}  // namespace

