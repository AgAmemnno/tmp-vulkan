#include "draw_testing.hh"



#include "CLG_log.h"
#include "BKE_global.h"

#include "BLI_threads.h"
#include "BLI_vector.hh"
#include "BKE_icons.h"
#include "BKE_appdir.h"
#include "BLF_api.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "gpu_immediate_private.hh"


#include "vulkan/vk_context.hh"
namespace blender::draw {
void BKE_blender_userdef_data_free(UserDef *userdef, bool clear_fonts)
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
    LISTBASE_FOREACH (uiFont *, font, &userdef->uifonts) {
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


void test_icon()
{
  using namespace blender;
   using namespace blender::gpu;

  G.debug_value = -7777;
  BLI_threadapi_init();
  BKE_icons_init(BIFICONID_LAST);
  BKE_appdir_init();
  BLF_init();
  UI_theme_init_default();
  UI_style_init_default();
  UI_init();
  immActivate();

  Vector<int> Icon3;
  for (int i = 0; i < BIFICONID_LAST; i++) {
   
    auto icon = BKE_icon_get(i);
    if (icon) {
      if (icon->drawinfo) {
        auto type = ((int *)icon->drawinfo)[0];
        if (type == 3) {
          Icon3.append(i);
          if (Icon3.size() == 4)
             break;
        }
      }
    }
  }


  auto vkcontext = VKContext::get();

  vkcontext->begin_frame();
  vkcontext->begin_submit(Icon3.size());

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

  vkcontext->end_submit();
  vkcontext->end_frame();


  immDeactivate();
  UI_exit();
  BLF_exit();
  BKE_blender_userdef_data_free(&U, true);
  BKE_appdir_exit();
  BKE_icons_free();
  BLI_threadapi_exit();



};

DRAW_TEST(icon)




};



