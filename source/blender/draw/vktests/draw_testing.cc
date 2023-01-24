/* SPDX-License-Identifier: Apache-2.0 */
#include "draw_testing.hh"

#ifdef DRAW_GTEST_SUITE
#include "gpu_testing.hh"
#include "draw_manager_testing.h"
#endif

#include "GPU_init_exit.h"
#include "draw_manager.hh"

#include  "intern/GHOST_ContextVK.h"
#include "intern/GHOST_Window.h"
#include "BLI_system.h"

#include "BKE_appdir.h"

static char* argv0 = nullptr;

namespace blender::draw {
void STUB_WM_window_set_dpi(GHOST_WindowHandle &ghostwin)
{
  float auto_dpi = GHOST_GetDPIHint(ghostwin);

  /* Clamp auto DPI to 96, since our font/interface drawing does not work well
   * with lower sizes. The main case we are interested in supporting is higher
   * DPI. If a smaller UI is desired it is still possible to adjust UI scale. */
  auto_dpi = max_ff(auto_dpi, 96.0f);

  /* Lazily init UI scale size, preserving backwards compatibility by
   * computing UI scale from ratio of previous DPI and auto DPI */
  if (U.ui_scale == 0) {
    int virtual_pixel = (U.virtual_pixel == VIRTUAL_PIXEL_NATIVE) ? 1 : 2;

    if (U.dpi == 0) {
      U.ui_scale = virtual_pixel;
    }
    else {
      U.ui_scale = (virtual_pixel * U.dpi * 96.0f) / (auto_dpi * 72.0f);
    }

    CLAMP(U.ui_scale, 0.25f, 4.0f);
  }

  /* Blender's UI drawing assumes DPI 72 as a good default following macOS
   * while Windows and Linux use DPI 96. GHOST assumes a default 96 so we
   * remap the DPI to Blender's convention. */
  auto_dpi *= GHOST_GetNativePixelSize(ghostwin);
  U.dpi = auto_dpi * U.ui_scale * (72.0 / 96.0f);

  /* Automatically set larger pixel size for high DPI. */
  int pixelsize = max_ii(1, (int)(U.dpi / 64));
  /* User adjustment for pixel size. */
  pixelsize = max_ii(1, pixelsize + U.ui_line_width);

  /* Set user preferences globals for drawing, and for forward compatibility. */
  U.pixelsize = pixelsize;
  U.virtual_pixel = (pixelsize == 1) ? VIRTUAL_PIXEL_NATIVE : VIRTUAL_PIXEL_DOUBLE;
  U.dpi_fac = U.dpi / 72.0f;
  U.inv_dpi_fac = 1.0f / U.dpi_fac;

  /* Widget unit is 20 pixels at 1X scale. This consists of 18 user-scaled units plus
   *  left and right borders of line-width (pixelsize). */
  U.widget_unit = (int)roundf(18.0f * U.dpi_fac) + (2 * pixelsize);
}

#ifdef DRAW_GTEST_SUITE
/* Base class for draw test cases. It will setup and tear down the GPU part around each test. */
void DrawVulkanTest::SetUp()
{
  GPUVulkanTest::SetUp();
  DRW_draw_state_init_gtests(GPU_SHADER_CFG_DEFAULT);
}
#endif
static void callback_mem_error(const char* errorStr)
{
  fputs(errorStr, stderr);
  fflush(stderr);
}
static void callback_clg_fatal(void* fp)
{
  BLI_system_backtrace((FILE*)fp);
}
static void main_callback_setup(void)
{
  /* Error output from the guarded allocation routines. */
  MEM_set_error_callback(callback_mem_error);
}

void GPUTest::SetUp()
{
  ghost_context = NULL;
  MEM_init_memleak_detection();
  MEM_use_memleak_detection(true);

  CLG_init();

  CLG_fatal_fn_set(callback_clg_fatal);


  C = CTX_create();

  BKE_appdir_program_path_init(argv0);

  ghost_system = GHOST_CreateSystem();
  GHOST_GLSettings glSettings = {0,GHOST_kDrawingContextTypeVulkan };
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

  /// context = GPU_context_create(nullptr, ghost_context);

  const bool debug_context = (glSettings.flags & GHOST_glDebugContext) != 0;
  ghost_window = GHOST_CreateWindow(
      ghost_system,
      NULL,
      "Blender gpu_test",
      0,
      0,1024,1024,
      //1920,
      //1080,
      //(GHOST_TWindowState)GHOST_kWindowStateMinimized,
      (GHOST_TWindowState)GHOST_kWindowStateNormal,  // GHOST_kWindowStateMinimized,
      false,
      glSettings);

  if (ghost_window && ghost_context == NULL) {
    GHOST_Window *ghostWin = reinterpret_cast<GHOST_Window *>(ghost_window);
    ghost_context = (GHOST_ContextHandle)((ghostWin ? ghostWin->getContext() : NULL));
  }
  STUB_WM_window_set_dpi( ghost_window);

  BLI_assert(ghost_context);


  context = (GPUContext*)GPU_context_create(ghost_window, ghost_context);
  GHOST_ActivateOpenGLContext(ghost_context);

  GPU_init();


  GPU_context_active_set((GPUContext*)context);


}

void GPUTest::TearDown()
{

  GPU_exit();

  GPU_context_discard(context);

  GHOST_DisposeWindow(ghost_system, ghost_window);

  /// GHOST_DisposeOpenGLContext(ghost_system, ghost_context);
  GHOST_DisposeSystem(ghost_system);

  CTX_free(C);


  CLG_exit();
}

}  // namespace blender::gpu


#ifndef DRAW_GTEST_SUITE

#include "draw_multithread.hh"
#include "draw_capa_test.cc"
#include "draw_icon_test.cc"

#define DRAW_TEST_STAND_ALONE(NAME){\
blender::draw::GPUTest* t = new blender::draw::GPUTest;\
t->SetUp();\
t->test_##NAME();\
t->TearDown();\
delete t;\
}

#define DRAW_TEST_STAND_ALONE_RAW(NAME){\
blender::draw::GPUTest* t = new blender::draw::GPUTest;\
t->test_##NAME();\
delete t;\
}

int main(int argc,

  const char** argv) {
  
    argv0 = const_cast<char*>(argv[0]);

    DRAW_TEST_STAND_ALONE_RAW(RangeIter)
    DRAW_TEST_STAND_ALONE_RAW(MempoolIter)
    DRAW_TEST_STAND_ALONE_RAW(ListBaseIter)
    DRAW_TEST_STAND_ALONE_RAW(ParallelInvoke)
    DRAW_TEST_STAND_ALONE_RAW(Task)

    DRAW_TEST_STAND_ALONE(capabilities)
   
    
    DRAW_TEST_STAND_ALONE(icon)
    
    return 0;
};

#endif