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
      swfb->append_wait_semaphore(ofs_fb->get_signal());
    }

    GPU_framebuffer_restore();

    /*Blit to swapchain.*/
    {

      
      swfb->render_begin(VK_NULL_HANDLE, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, true);
      ofs_fb->blit_to(GPU_COLOR_BIT, 0, swfb, 0, 0, 0);
      swfb->render_end();
    };

  }


  //UI_icon_draw_cache_end();
  GPU_offscreen_free(offscreen);

  Blender_Exit_Stub();



};


#ifdef DRAW_GTEST_SUITE
        DRAW_TEST(icon)
#endif



#if 0
static void wm_draw_region_blit(ARegion* region, int view)
{
  
  if (!region->draw_buffer) {
    return;
  }

  if (view == -1) {
    /* Non-stereo drawing. */
    view = 0;
  }
  else if (view > 0) {
    if (region->draw_buffer->viewport == NULL) {
      /* Region does not need stereo or failed to allocate stereo buffers. */
      view = 0;
    }
  }

  if (region->draw_buffer->viewport) {
    GPU_viewport_draw_to_screen(region->draw_buffer->viewport, view, &region->winrct);
  }
  else {
    GPU_offscreen_draw_to_screen(
      region->draw_buffer->offscreen, region->winrct.xmin, region->winrct.ymin);
  }

}

static void wm_draw_window(bContext* C, GPUContext* gpuctx) // wmWindow* win)
{
  GPU_context_begin_frame(gpuctx);

  //bScreen* screen = WM_window_get_active_screen(win);
  //bool stereo = WM_stereo3d_enabled(win, false);

  /* Avoid any BGL call issued before this to alter the window drawin. */
  GPU_bgl_end();


  /* Draw area regions into their own frame-buffer. This way we can redraw
    * the areas that need it, and blit the rest from existing frame-buffers. */
  wm_draw_window_offscreen(C, win, stereo);

#if 0
  /* Now we draw into the window frame-buffer, in full window coordinates. */
  if (!stereo) {
    /* Regular mono drawing. */
    wm_draw_window_onscreen(C, win, -1);
  }
  else if (win->stereo3d_format->display_mode == S3D_DISPLAY_PAGEFLIP) {
    /* For page-flip we simply draw to both back buffers. */
    GPU_backbuffer_bind(GPU_BACKBUFFER_RIGHT);
    wm_draw_window_onscreen(C, win, 1);

    GPU_backbuffer_bind(GPU_BACKBUFFER_LEFT);
    wm_draw_window_onscreen(C, win, 0);
  }
  else if (ELEM(win->stereo3d_format->display_mode, S3D_DISPLAY_ANAGLYPH, S3D_DISPLAY_INTERLACE)) {
    /* For anaglyph and interlace, we draw individual regions with
      * stereo frame-buffers using different shaders. */
    wm_draw_window_onscreen(C, win, -1);
  }
  else {
    /* For side-by-side and top-bottom, we need to render each view to an
      * an off-screen texture and then draw it. This used to happen for all
      * stereo methods, but it's less efficient than drawing directly. */
    const int width = WM_window_pixels_x(win);
    const int height = WM_window_pixels_y(win);
    GPUOffScreen* offscreen = GPU_offscreen_create(width, height, false, GPU_RGBA8, NULL);

    if (offscreen) {
      GPUTexture* texture = GPU_offscreen_color_texture(offscreen);
      wm_draw_offscreen_texture_parameters(offscreen);

      for (int view = 0; view < 2; view++) {
        /* Draw view into offscreen buffer. */
        GPU_offscreen_bind(offscreen, false);
        wm_draw_window_onscreen(C, win, view);
        GPU_offscreen_unbind(offscreen, false);

        /* Draw offscreen buffer to screen. */
        GPU_texture_bind(texture, 0);

        wmWindowViewport(win);
        if (win->stereo3d_format->display_mode == S3D_DISPLAY_SIDEBYSIDE) {
          wm_stereo3d_draw_sidebyside(win, view);
        }
        else {
          wm_stereo3d_draw_topbottom(win, view);
        }

        GPU_texture_unbind(texture);
      }

      GPU_offscreen_free(offscreen);
    }
    else {
      /* Still draw something in case of allocation failure. */
      wm_draw_window_onscreen(C, win, 0);
    }
  }

  screen->do_draw = false;
#endif

  wm_draw_window_onscreen(C, win, -1);
  GPU_context_end_frame(gpuctx);
}


 void wm_window_swap_buffers(GHOST_WindowHandle ghostwin)
{
  GHOST_SwapWindowBuffers(ghostwin);
}


void wm_draw_update(GPUTest* test,bContext* C = nullptr)
{
  //Main* bmain = CTX_data_main(C);
  //wmWindowManager* wm = CTX_wm_manager(C);

  GPU_context_main_lock();

  GPU_render_begin();
  GPU_render_step();

  //BKE_image_free_unused_gpu_textures();
  
  /*
  LISTBASE_FOREACH(wmWindow*, win, &wm->windows) {
  */

  #ifdef WIN32
  
  GHOST_TWindowState state = GHOST_GetWindowState(test->get_ghost_window());// win->ghostwin);

      if (state == GHOST_kWindowStateMinimized) {
          /* do not update minimized windows, gives issues on Intel (see T33223)
              * and AMD (see T50856). it seems logical to skip update for invisible
              * window anyway.
              */
        return;
        //continue;
      }
  #endif

      //CTX_wm_window_set(C, win);

      if (true){  //wm_draw_update_test_window(bmain, C, win)) {
          //bScreen* screen = WM_window_get_active_screen(win);

          /* sets context window+screen */
          //wm_window_make_drawable(wm, win);

          /* notifiers for screen redraw */
          //ED_screen_ensure_updated(wm, win, screen);

          wm_draw_window(C, test->get_gpu_context());
         // wm_draw_update_clear_window(C, win);

          wm_window_swap_buffers(test->get_ghost_window());
      }
  //}

  //CTX_wm_window_set(C, NULL);

  /* Draw non-windows (surfaces) */
  //wm_surfaces_iter(C, wm_draw_surface);

  GPU_render_end();
  GPU_context_main_unlock();
}
#endif
};


