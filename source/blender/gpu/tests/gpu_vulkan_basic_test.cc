/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "GPU_capabilities.h"
#include "GPU_compute.h"
#include "GPU_index_buffer.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_vertex_buffer.h"
#include "GPU_vertex_format.h"
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

