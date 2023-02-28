/* SPDX-License-Identifier: Apache-2.0 */
#ifndef DRAW_TESTING_HEADER
#define DRAW_TESTING_HEADER

//#define DRAW_GTEST_SUITE

#ifndef DRAW_GTEST_SUITE
#  define DRAW_TESTING_CAPA 1
#  define DRAW_TESTING_ICON 1
#  define DRAW_TESTING_MT 1
#endif
#include "testing/testing.h"

#include "BKE_global.h"
#include "CLG_log.h"
#include "GHOST_C-api.h"
#include "vulkan/vk_context.hh"

#ifdef DRAW_TESTING_CAPA
#  include "gpu_capabilities_private.hh"
#endif

#ifdef DRAW_TESTING_ICON

#  include "BKE_appdir.h"
#  include "BKE_icons.h"
#  include "BLF_api.h"
#  include "BLI_threads.h"
#  include "BLI_vector.hh"

#  include "UI_interface.h"
#  include "UI_resources.h"

#  include "gpu_immediate_private.hh"

#endif

#ifdef DRAW_TESTING_MT

#  include "BLI_listbase.h"
#  include "BLI_mempool.h"
#  include "BLI_task.h"
#  include "BLI_task.hh"
#  include "BLI_utildefines.h"
#  include "MEM_guardedalloc.h"
#  include "atomic_ops.h"
#  include <atomic>
#  include <cstring>

#  include "BLI_atomic_disjoint_set.hh"
#  include "BLI_enumerable_thread_specific.hh"
#  include "BLI_map.hh"
#  include "BLI_sort.hh"

#endif

#ifndef DRAW_GTEST_SUITE

struct GPUContext;

namespace blender::draw {
class GPUTest {

 private:
  GHOST_SystemHandle ghost_system;
  GHOST_ContextHandle ghost_context;
  GHOST_WindowHandle ghost_window;
  GPUContext *context;
  GHOST_TDrawingContextType draw_context_type = GHOST_kDrawingContextTypeVulkan;
  bContext *C;

 public:
  GPUTest()
  {
  }

  void SetUp();
  void TearDown();
  GHOST_WindowHandle get_ghost_window()
  {
    return ghost_window;
  }
  GPUContext *get_gpu_context()
  {
    return context;
  }
  void test_capabilities();
  void test_icon();
  void test_RangeIter();
  void test_MempoolIter();
  void test_ListBaseIter();
  void test_ParallelInvoke();
  void test_Task();
  void test_sphere();
};
};  // namespace blender::draw
#else

struct GPUContext;

namespace blender::draw {
class GPUTest : public ::testing::Test {

 private:
  GHOST_SystemHandle ghost_system;
  GHOST_ContextHandle ghost_context;
  GHOST_WindowHandle ghost_window;
  GPUContext *context;
  GHOST_TDrawingContextType draw_context_type = GHOST_kDrawingContextTypeNone;

 protected:
  GPUTest(GHOST_TDrawingContextType draw_context_type) : draw_context_type(draw_context_type)
  {
  }

  void SetUp() override;
  void TearDown() override;
};
class GPUVulkanTest : public GPUTest {
 public:
  GPUVulkanTest() : GPUTest(GHOST_kDrawingContextTypeVulkan)
  {
  }
};

#  define GPU_VULKAN_TEST(test_name) \
    TEST_F(GPUVulkanTest, test_name) \
    { \
      test_##test_name(); \
    }

/* Base class for draw test cases. It will setup and tear down the GPU part around each test. */
class DrawVulkanTest : public GPUVulkanTest {
 public:
  void SetUp() override;
};

#  define DRAW_TEST(test_name) \
    TEST_F(DrawVulkanTest, test_name) \
    { \
      test_##test_name(); \
    }

}  // namespace blender::draw
#endif

#endif