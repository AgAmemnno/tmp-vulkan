/* SPDX-License-Identifier: Apache-2.0 */
#include "draw_testing.hh"

#ifdef DRAW_GTEST_SUITE
#  include "draw_manager_testing.h"
#  include "gpu_testing.hh"
#endif

#include "GPU_init_exit.h"
#include "draw_manager.hh"

#include "BLI_system.h"
#include "intern/GHOST_ContextVK.h"
#include "intern/GHOST_Window.h"

#include "BKE_appdir.h"

static char *argv0 = nullptr;

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
static void callback_mem_error(const char *errorStr)
{
  fputs(errorStr, stderr);
  fflush(stderr);
}
static void callback_clg_fatal(void *fp)
{
  BLI_system_backtrace((FILE *)fp);
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
  GHOST_GLSettings glSettings = {0, GHOST_kDrawingContextTypeVulkan};
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
      0,
      1920,
      1024,
      // 1920,
      // 1080,
      //(GHOST_TWindowState)GHOST_kWindowStateMinimized,
      (GHOST_TWindowState)GHOST_kWindowStateNormal,  // GHOST_kWindowStateMinimized,
      false,
      glSettings);

  if (ghost_window && ghost_context == NULL) {
    GHOST_Window *ghostWin = reinterpret_cast<GHOST_Window *>(ghost_window);
    ghost_context = (GHOST_ContextHandle)((ghostWin ? ghostWin->getContext() : NULL));
  }
  STUB_WM_window_set_dpi(ghost_window);

  BLI_assert(ghost_context);

  context = (GPUContext *)GPU_context_create(ghost_window, ghost_context);
  GHOST_ActivateOpenGLContext(ghost_context);

  GPU_init();

  GPU_context_active_set((GPUContext *)context);
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

}  // namespace blender::draw

#define STACK_TRACE
#ifdef STACK_TRACE

#  include "dbghelp.h"
#  pragma comment(lib, "DbgHelp.lib ")
#  define MAX_INFO 200000
#  define MAX_INFO_MSG 256

//#include <pthread.h>
// static pthread_mutex_t _thread_lock = ((pthread_mutex_t)(size_t)-1);

typedef struct MEM_info {
  uintptr_t ptr;
  bool is_free;
  size_t len;
  char msg[MAX_INFO_MSG];
} MEM_info;
static MEM_info *minfo;
static int minfo_alloc = 0;
static int minfo_id = -1;
static int push_cnt_id = 0;
static int pop_cnt_id = 0;
static pthread_mutex_t thread_lock = PTHREAD_MUTEX_INITIALIZER;

static void mem_lock_thread(void)
{
  pthread_mutex_lock(&thread_lock);
}

static void mem_unlock_thread(void)
{
  pthread_mutex_unlock(&thread_lock);
}
static DWORD thread_main;
void MEM_StackInfo(void *ptr, const char *str, uint64_t len)
{
  // printStack();
#  if 1
  mem_lock_thread();
  if (push_cnt_id == 0) {
    {
      thread_main = GetCurrentThreadId();
      minfo = (MEM_info *)malloc(MAX_INFO * sizeof(MEM_info));
      for (int j = 0; j < MAX_INFO; j++) {
        minfo[j].is_free = true;
        minfo[j].len = 0;
        minfo[j].ptr = 1;
      };
      minfo_alloc = MAX_INFO;
    };
  }
  DWORD thread_id = GetCurrentThreadId();
  if (thread_main != thread_id) {
  }

  minfo_id = -1;
  for (int i = 0; i < minfo_alloc; i++) {
    if (minfo[i].is_free) {
      minfo_id = i;
      push_cnt_id++;
      break;
    }
  }

  if (minfo_id == -1) {
    exit(-1);
  }

  minfo[minfo_id].ptr = (uintptr_t)ptr;
  minfo[minfo_id].len = len;
  minfo[minfo_id].is_free = false;
  unsigned int i;
#    define STACK_NUMS 30
  void *stack[STACK_NUMS];
  unsigned short frames;

  HANDLE process;

  process = GetCurrentProcess();

  SymInitialize(process, NULL, TRUE);
  memset(&stack, 0, sizeof(uintptr_t) * STACK_NUMS);

  frames = CaptureStackBackTrace(0, STACK_NUMS, stack, NULL);

  char *dst = &minfo[minfo_id].msg[0];
  memset(minfo[minfo_id].msg, 0, MAX_INFO_MSG);
  int total = 0;
  SYMBOL_INFO *symbol = (SYMBOL_INFO *)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
  symbol->MaxNameLen = 255;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  static int CALLEE_CNT = 0;
  CALLEE_CNT++;

  for (i = 0; i < frames; i++) {
    if (!stack[i])
      break;
    // mem_lock_thread();
    SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
    // mem_unlock_thread();
    if (total + symbol->NameLen > MAX_INFO_MSG) {
      break;
    }
    memcpy(dst, symbol->Name, symbol->NameLen);
    dst[symbol->NameLen] = '\n';
    dst += (symbol->NameLen + 1);
    total += (symbol->NameLen + 1);
    if (strcmp(symbol->Name, "main") == 0) {
      break;
    }
    /*  printf("%i: %s - 0x%0X\n", frames - i - 1, symbol->Name, symbol->Address); */
  }
  free(symbol);

#  endif

  mem_unlock_thread();
}
void MEM_PopInfo(void *ptr)
{

  mem_lock_thread();

  uintptr_t p = (uintptr_t)ptr;
  for (int i = 0; i < minfo_alloc; i++) {
    if (minfo[i].ptr == p) {
      minfo[i].is_free = true;
      minfo[i].ptr = 0;
      minfo[i].len = 0;
      pop_cnt_id++;
      break;
    }
  }
  mem_unlock_thread();
};

void MEM_PrintInfo()
{

  int cnt = 0;
  for (int i = 0; i < minfo_alloc; i++) {
    if (!minfo[i].is_free) {
      cnt++;
      // printf("Memory leak  found ADR %llx   size  %zu  \n  %s    \n",minfo[i].ptr, minfo[i].len,
      // minfo[i].msg);
    }
  }
  // printf("Memory  push  %d   pop   %d  leak %d  \n", push_cnt_id, pop_cnt_id,cnt);
  free(minfo);
  minfo_alloc = 0;
  minfo_id = -1;
  push_cnt_id = 0;
  pop_cnt_id = 0;
};
#else
void MEM_PrintInfo()
{
}
#endif

#ifndef DRAW_GTEST_SUITE

#  include "draw_capa_test.cc"
#  include "draw_icon_test.cc"
#  include "draw_multithread.hh"

#  define DRAW_TEST_STAND_ALONE(NAME) \
    { \
      blender::draw::GPUTest *t = new blender::draw::GPUTest; \
      t->SetUp(); \
      t->test_##NAME(); \
      t->TearDown(); \
      delete t; \
    }

#  define DRAW_TEST_STAND_ALONE_RAW(NAME) \
    { \
      blender::draw::GPUTest *t = new blender::draw::GPUTest; \
      t->test_##NAME(); \
      delete t; \
    }

int main(int argc, const char **argv)
{

  G.debug |= G_DEBUG_GPU;

  argv0 = const_cast<char *>(argv[0]);
  /*
  DRAW_TEST_STAND_ALONE_RAW(RangeIter)
  DRAW_TEST_STAND_ALONE_RAW(MempoolIter)
  DRAW_TEST_STAND_ALONE_RAW(ListBaseIter)
  DRAW_TEST_STAND_ALONE_RAW(ParallelInvoke)
  DRAW_TEST_STAND_ALONE_RAW(Task)
  DRAW_TEST_STAND_ALONE(capabilities)
 */

  DRAW_TEST_STAND_ALONE(icon)

  return 0;
};

#endif
