#ifdef DRAW_GTEST_SUITE
#define DRAW_TESTING_ICON 1
#include "draw_testing.hh"
#endif
#include "GPU_batch_presets.h"
#include "GPU_batch.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "GPU_viewport.h"
#include "GPU_framebuffer.h"
#include "GPU_shader_shared.h"
#include "DNA_screen_types.h"
#include "wm_draw.h"

#include "vulkan/vk_framebuffer.hh"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"


namespace blender::gpu {

};
struct GPUOffScreen;

void BKE_blender_userdef_data_free(UserDef* userdef, bool clear_fonts)
{
#define U BLI_STATIC_ASSERT(false, "Global 'U' not allowed, only use arguments passed in!")
#ifdef U /* quiet warning */
#endif
    /*
    userdef_free_keymaps(userdef);
    userdef_free_keyconfig_prefs(userdef);
    userdef_free_user_menus(userdef);
    userdef_free_addons(userdef);
    */

    if (clear_fonts) {
        LISTBASE_FOREACH(uiFont*, font, &userdef->uifonts) {
            BLF_unload_id(font->blf_id);
        }
        BLF_default_set(-1);
    }

    BLI_freelistN(&userdef->autoexec_paths);
    BLI_freelistN(&userdef->asset_libraries);

    BLI_freelistN(&userdef->uistyles);
    BLI_freelistN(&userdef->uifonts);
    BLI_freelistN(&userdef->themes);

#undef U
}

void Blender_Init_Stub() {
    G.debug_value = -7777;
    BLI_threadapi_init();
    BKE_icons_init(BIFICONID_LAST);
    BKE_appdir_init();
    BLF_init();
    UI_theme_init_default();
    UI_style_init_default();
    UI_init();
    //UI_icons_init();
    immActivate();
    
    UI_icons_reload_internal_textures();
}

void Blender_Exit_Stub() {
    immDeactivate();
    UI_icons_free();
    UI_exit();
    BLF_exit();
    BKE_blender_userdef_data_free(&U, true);
    BKE_appdir_exit();
    BKE_icons_free();
    BLI_threadapi_exit();


}

namespace blender::draw {
  struct IconImage {
    int w;
    int h;
    uint* rect;
    const uchar* datatoc_rect;
    int datatoc_size;
  };

  struct DrawInfo {
    int type;
    union {

      struct {
        ImBuf* image_cache;
        bool inverted;
      } geom;
      struct {
        IconImage* image;
      } buffer;
      struct {
        int x, y, w, h;
        int theme_color;
      } texture;
      struct {
        /* Can be packed into a single int. */
        short event_type;
        short event_value;
        int icon;
        /* Allow lookups. */
        struct DrawInfo* next;
      } input;
    } data;
  };
  static void wm_draw_offscreen_texture_parameters(GPUOffScreen* offscreen)
  {
    /* Setup offscreen color texture for drawing. */
    GPUTexture* texture = GPU_offscreen_color_texture(offscreen);

    /* No mipmaps or filtering. */
    GPU_texture_mipmap_mode(texture, false, false);
  }

  static void wm_draw_region_bind(ARegion* region, int view)
  {
#if  0
    if (!region->draw_buffer) {
      return;
    }

    if (region->draw_buffer->viewport) {
      GPU_viewport_bind(region->draw_buffer->viewport, view, &region->winrct);
  }
    else {
#endif
      /* GPU_offscreen_bind(region->draw_buffer->offscreen, false);  */

      /* For now scissor is expected by region drawing, we could disable it
       * and do the enable/disable in the specific cases that setup scissor. */
      GPU_scissor_test(true);
      GPU_scissor(0, 0, region->winx, region->winy);
    //}

    //region->draw_buffer->bound_view = view;
}

  static void icon_verify_datatoc(IconImage* iimg)
  {
    /* if it has own rect, things are all OK */
    if (iimg->rect) {
      return;
    }

    if (iimg->datatoc_rect) {
      ImBuf* bbuf = IMB_ibImageFromMemory(
        iimg->datatoc_rect, iimg->datatoc_size, IB_rect, nullptr, "<matcap icon>");
      /* w and h were set on initialize */
      if (bbuf->x != iimg->h && bbuf->y != iimg->w) {
        IMB_scaleImBuf(bbuf, iimg->w, iimg->h);
      }

      iimg->rect = bbuf->rect;
      bbuf->rect = nullptr;
      IMB_freeImBuf(bbuf);
    }

  }
  static void _icon_draw_texture(float x,
    float y,
    float w,
    float h,
    float alpha,
    const float rgb[3],
    GPUTexture* texture)
  {

    //const IconTextOverlay* text_overlay = nullptr;

    /* We need to flush widget base first to ensure correct ordering. */
    UI_widgetbase_draw_cache_flush();

    GPU_blend(GPU_BLEND_ALPHA_PREMULT);
    
    GPUShader* shader = GPU_shader_get_builtin_shader(GPU_SHADER_ICON);
    GPU_shader_bind(shader);

    const int img_binding = GPU_shader_get_texture_binding(shader, "image");
    const int color_loc = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_COLOR);
    const int rect_tex_loc = GPU_shader_get_uniform(shader, "rect_icon");
    const int rect_geom_loc = GPU_shader_get_uniform(shader, "rect_geom");

    if (rgb) {
      const float color[4] = { rgb[0], rgb[1], rgb[2], alpha };
      GPU_shader_uniform_vector(shader, color_loc, 4, 1, color);
    }
    else {
      const float color[4] = { alpha, alpha, alpha, alpha };
      GPU_shader_uniform_vector(shader, color_loc, 4, 1, color);
    }

    const float tex_color[4] = { 0.,0.,1.,1. };/// {x1, y1, x2, y2};
    const float geom_color[4] = { x, y, x + w, y + h };

    GPU_shader_uniform_vector(shader, rect_tex_loc, 4, 1, tex_color);
    GPU_shader_uniform_vector(shader, rect_geom_loc, 4, 1, geom_color);
    GPU_shader_uniform_1f(shader, "text_width", 0.f);

    GPU_texture_bind_ex(texture, GPU_SAMPLER_ICON, img_binding, false);

    GPUBatch* quad = GPU_batch_preset_quad();
    GPU_batch_set_shader(quad, shader);
    GPU_batch_draw(quad);

    GPU_texture_unbind(texture);

    GPU_blend(GPU_BLEND_ALPHA);
  }


#ifndef DRAW_GTEST_SUITE
        void GPUTest::test_icon()
#else
        void test_icon()
#endif
{
  using namespace blender;
   using namespace blender::gpu;

   Blender_Init_Stub();
 
  ARegion region;
  region.winx = 1024;// 1415;
  region.winy = 512;// 32;
  auto vkcontext = VKContext::get();
  VKFrameBuffer* swfb = static_cast<VKFrameBuffer*> (vkcontext->active_fb);

  GPUOffScreen* offscreen = GPU_offscreen_create(
    region.winx, region.winy, false, GPU_RGBA8, NULL);
  GPU_offscreen_bind(offscreen, false);
  VKFrameBuffer* ofs_fb = static_cast<VKFrameBuffer*> (vkcontext->active_fb);

  wm_draw_offscreen_texture_parameters(offscreen);
  wm_draw_region_bind(&region, 0);

  
  
  
  GPU_clear_color(0.5, 0.1, 0.1, 1.f);
  GPU_blend(GPU_BLEND_ALPHA_PREMULT);
  uchar color[4] = { 200, 200, 200, 200 };
  float X[4] = { 12.5, 0., -1., 0. };
  float Y[4] = { 5.5, -1., 0., 0. };
  float SX[4] = { 0, 256, 0, 256 };
  float SY[4] = { 0, 0, 256,256 };
  int cnt = 0;
 // float aspect = 0.8;
  const int draw_size = 16;
  GPU_viewport(0, 0, region.winx, region.winy);
  const uchar mono_color[4] = { 217,217 ,217, 255 };
  const bool mono_border = false;

  /* A test to render while toggling the queue. */
  for (int iconId = 780/*16*/; iconId < BIFICONID_LAST; iconId++) {

    //UI_icon_draw_cache_begin();
    bool draw = false;
    Icon* icon = BKE_icon_get(iconId);

    /*Offscreen draw.*/
    if (icon && icon->drawinfo && ((int*)icon->drawinfo)[0] == 3)
    {
      GPU_offscreen_bind(offscreen, false);

      GPU_clear_color(0.5, 0.1, 0.1, 1.f);
      GPU_scissor(0,0, region.winx, region.winy);

      /*VKImmediate draw*/
      {
        const int size = 1;
        UI_icon_draw_preview(-1., -1., iconId, 1.0f, 1.f, size);
       }
      /*VKBatch draw*/
      {
        GPUTexture* tex = nullptr;
        DrawInfo* di = static_cast<DrawInfo*>(icon->drawinfo);
        /* scale width and height according to aspect */
        icon_verify_datatoc(di->data.buffer.image);
        int w = (int)(di->data.buffer.image->w);
        int h = (int)(di->data.buffer.image->h);

        tex = GPU_texture_create_2d("batchtest.tex", w, h, 2, GPU_RGBA8, NULL);
        GPU_texture_update(tex, GPU_DATA_UBYTE, di->data.buffer.image->rect);
        const float rgb[3] = { 1.,1.,1. };
        _icon_draw_texture(0, 0, 1., 1., 1., rgb, tex);

        GPU_texture_unbind(tex);
        GPU_texture_free(tex);
        /* }
          else {
            if (G.debug & G_DEBUG) {
              printf("%s: Internal error, no icon for icon ID: %d\n", __func__, iconId);
            }
            return;
          }
          */

      }
      cnt++;
      draw = true;
    }
    if (draw) {

      GPU_framebuffer_restore();
      /*Blit to swapchain.*/
      {

        GPU_context_begin_frame(GPU_context_active_get());
        swfb->render_begin(VK_NULL_HANDLE, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, true);
        ofs_fb->blit_to(GPU_COLOR_BIT, 0, swfb, 0, 0, 0);
        swfb->render_end();
        GPU_context_end_frame(GPU_context_active_get());
      };
    }
  }



 


  //UI_icon_draw_cache_end();
  GPU_offscreen_free(offscreen);

  Blender_Exit_Stub();



};


#ifdef DRAW_GTEST_SUITE
        DRAW_TEST(icon)
#endif



};


