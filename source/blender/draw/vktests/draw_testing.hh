/* SPDX-License-Identifier: Apache-2.0 */
#ifndef DRAW_TESTING_HEADER
#define DRAW_TESTING_HEADER

//#define DRAW_GTEST_SUITE


#include "CLG_log.h"
#include "BKE_global.h"
#include "GHOST_C-api.h"
#include  "vulkan/vk_context.hh"
#include "gpu_capabilities_private.hh"
#include "gpu_immediate_private.hh"


#ifndef DRAW_GTEST_SUITE


struct GPUContext;

namespace blender::draw {
    class GPUTest {

    private:
        GHOST_SystemHandle ghost_system;
        GHOST_ContextHandle ghost_context;
        GHOST_WindowHandle ghost_window;
        GPUContext* context;
        GHOST_TDrawingContextType draw_context_type = GHOST_kDrawingContextTypeVulkan;

    public:
        GPUTest()
        {
        }

        void SetUp();
        void TearDown();

        void test_capabilities();
        void test_icon();
    };
};
#else
#include "testing/testing.h"

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

#define GPU_VULKAN_TEST(test_name) \
  TEST_F(GPUVulkanTest, test_name) \
  { \
    test_##test_name(); \
  }

/* Base class for draw test cases. It will setup and tear down the GPU part around each test. */
class DrawVulkanTest : public GPUVulkanTest {
 public:
  void SetUp() override;
};

#define DRAW_TEST(test_name) \
  TEST_F(DrawVulkanTest, test_name) \
  { \
    test_##test_name(); \
  }

}  // namespace blender::draw
#endif

#endif