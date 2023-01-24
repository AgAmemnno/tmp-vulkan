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
    immActivate();

}

void Blender_Exit_Stub() {
    immDeactivate();
    UI_exit();
    BLF_exit();
    BKE_blender_userdef_data_free(&U, true);
    BKE_appdir_exit();
    BKE_icons_free();
    BLI_threadapi_exit();


}

namespace blender::draw {

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


#ifndef DRAW_GTEST_SUITE
        void GPUTest::test_icon()
#else
        void test_icon()
#endif
{
  using namespace blender;
   using namespace blender::gpu;

   Blender_Init_Stub();
   UI_icons_reload_internal_textures();

/*For Immediate draw.*/
#if 1
  Vector<int> Icon3;
  for (int i = 775; i < BIFICONID_LAST; i++) {
   
    auto icon = BKE_icon_get(i);
    if (icon) {
      if (icon->drawinfo) {
        auto type = ((int *)icon->drawinfo)[0];
        if (type == 3) {
          Icon3.append(i);
          if (Icon3.size() == 1)
             break;
        }
      }
    }
  }
#endif



  ARegion region;
  region.winx = 512;// 1415;
  region.winy = 512;// 32;
  auto vkcontext = VKContext::get();
 GPUOffScreen* offscreen = GPU_offscreen_create(
   region.winx, region.winy, false, GPU_RGBA8, NULL);
 GPU_offscreen_bind(offscreen, false);
 VKFrameBuffer* ofs_fb = static_cast<VKFrameBuffer*> (vkcontext->active_fb);

  wm_draw_offscreen_texture_parameters(offscreen);
  wm_draw_region_bind(&region, 0);
  GPU_clear_color(0.5,0.1,0.1, 1.f);
  GPU_blend(GPU_BLEND_ALPHA_PREMULT);


  UI_icon_draw_cache_begin();



#if 1
  //vkcontext->begin_frame();
  //vkcontext->begin_submit(Icon3.size());

  if (true)
  {
    const float aspect = U.inv_dpi_fac;
    uchar color[4] = {200, 200, 200, 200};
    float X[4] = {-1., 0., -1., 0.};
    float Y[4] = {-1., -1., 0., 0.};
    float SX[4] = {0, 256, 0, 256};
    float SY[4] = {0, 0, 256,256};
    int cnt = 0;
    GPU_viewport(0, 0, 512, 512);

    for (auto &iconId : Icon3) {

      const int size = 1;
      /// UI_icon_draw_ex(0, 0, iconId, aspect, 1.f, 0.0f, color, false, nullptr);
      GPU_scissor(SX[cnt], SY[cnt], 256, 256);
      UI_icon_draw_preview(X[cnt], Y[cnt], iconId, 1.0f, 1.f, size);
      cnt++;
 
    }
  }

  GPU_framebuffer_restore();

 {
    VKFrameBuffer* swfb = static_cast<VKFrameBuffer*> (vkcontext->active_fb);
    swfb->append_wait_semaphore(ofs_fb->get_signal());
    swfb->render_begin(VK_NULL_HANDLE, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, true);
    ofs_fb->blit_to(GPU_COLOR_BIT, 0, swfb, 0, 0, 0);
    swfb->render_end();
 };

  //vkcontext->end_submit();
  //vkcontext->end_frame();
#endif


  UI_icon_draw_cache_end();


  GPU_offscreen_free(offscreen);
  UI_icons_free();
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


