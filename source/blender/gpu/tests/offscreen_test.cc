#include "testing/testing.h"

#include "gpu_testing.hh"

#include "atomic_ops.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_framebuffer.h"
#include "GPU_matrix.h"
#include "GPU_debug.h"
#include "GPU_context.h"
#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_shader_shared.h"

#include "gpu_shader_create_info.hh"
#include "gpu_shader_create_info_private.hh"
#include "gpu_shader_dependency_private.h"


#include "BLI_math_vector.hh"
#include "BLI_rand.h"
#include "BLI_length_parameterize.hh"
#include "BLI_task.hh"
#include "BLI_task.h"
#include "BLI_array.hh"

#include "intern/draw_cache.h"

#include "vulkan/vk_context.hh"
#include "vulkan/vk_immediate.hh"
#include "vulkan/vk_shader.hh"
#include "vulkan/vk_debug.hh"

#include <sstream>

#include <BLI_map.hh>

#include "vertex_input.hh"

namespace blender::gpu::tests {

const size_t SizeW = 1;
const size_t SizeH  = 1;
static void test_vertex_input()
{


  using namespace shader;


  auto test_print = [&](int i){

  auto tdata = vi_test::TestData[i];

    GPUVertFormat *format = immVertexFormat();

		typedef std::pair<GPUVertCompType,GPUVertFetchMode> RetTy2;
		typedef  std::pair<uint,RetTy2> RetTy; 
		auto CompTypeConv = [](Type ty) -> RetTy {
			uint comp_l = 0;
			GPUVertCompType comp_t  = (GPUVertCompType)0;
			GPUVertFetchMode fmode   =  GPU_FETCH_FLOAT;
			GPUVertFetchMode imode   =   GPU_FETCH_INT;
			GPUVertFetchMode iumode   = GPU_FETCH_INT_TO_FLOAT_UNIT;
			GPUVertFetchMode ifmode   =  GPU_FETCH_INT_TO_FLOAT;
			switch(ty){
        case Type::FLOAT:
					return  std::make_pair(1, std::make_pair(GPU_COMP_F32,fmode));
        case Type::VEC2:
					return  std::make_pair(2, std::make_pair(GPU_COMP_F32,fmode));
        case Type::VEC3:
					return  std::make_pair(3, std::make_pair(GPU_COMP_F32,fmode));
				case Type::VEC4:
					return  std::make_pair(4, std::make_pair(GPU_COMP_F32,fmode));
				case Type::MAT3:
					return  std::make_pair(9, std::make_pair(GPU_COMP_F32,fmode));
				case Type::MAT4:
					return  std::make_pair(16, std::make_pair(GPU_COMP_F32,fmode));
				case Type::UINT:
					return  std::make_pair(1, std::make_pair(GPU_COMP_U32,iumode));
				case Type::UVEC2:
					return  std::make_pair(2, std::make_pair(GPU_COMP_U32,iumode));
				case Type::UVEC3:
					return  std::make_pair(3, std::make_pair(GPU_COMP_U32,iumode));
				case Type::INT:
					return  std::make_pair(1, std::make_pair(GPU_COMP_I32,imode));
				case Type::IVEC2:
					return  std::make_pair(2, std::make_pair(GPU_COMP_I32,imode));
				case Type::IVEC3:
					return  std::make_pair(3, std::make_pair(GPU_COMP_I32,imode));
				case Type::IVEC4:
					return  std::make_pair(4, std::make_pair(GPU_COMP_I32,imode));
				case Type::BOOL:
					return  std::make_pair(1, std::make_pair(GPU_COMP_I32,imode));
        default:
				return std::make_pair(100, std::make_pair(GPU_COMP_I32,imode));
				BLI_assert_unreachable();
			}
			};
    auto immFunc = [](uint loc,GPUVertCompType comp_t,uint comp_l,GPUVertFetchMode mode = GPU_FETCH_FLOAT)
    {
			float Upload[16] =  {1.,2.,3.,4.,5.,6.,7.,8.,9.,10.,11.,12.,13.,14.,15.,16.};
      switch(comp_t){
        case GPU_COMP_F32:
            switch(comp_l){
            case 1:
                immAttr1f(loc, Upload[0]);
                break;
            case 2:
                immAttr2f(loc, Upload[0],Upload[1]);
                break;
            case 3:
                immAttr3f(loc, Upload[0],Upload[1],Upload[2]);
                break;
            case 4:
                immAttr4f(loc, Upload[0],Upload[1],Upload[2],Upload[3]);
                break;
           }
            break;
        case GPU_COMP_U32:
            switch(comp_l){
              case 1:
                  immAttr1u(loc, Upload[0]);
                  break;
           }
            break;
        case GPU_COMP_I32:
            switch(comp_l){
              case 2:
                  immAttr2i(loc, (int)Upload[0],(int)Upload[1]);
                  break;
           }
            break;
        case GPU_COMP_I16:
            switch(comp_l){
              case 2:
                  immAttr2s(loc, (short)Upload[0],(short)Upload[1]);
                  break;
           }
            break;
        case GPU_COMP_U8:
            switch(comp_l){
              case 3:
                  immAttr3ub(loc, (uchar)Upload[0],(uchar)Upload[1],(uchar)Upload[2]);
                  break;
              case 4:
                  immAttr4ub(loc, (uchar)Upload[0],(uchar)Upload[1],(uchar)Upload[2],(uchar)Upload[3]);
                  break;
           }
            break;
      }
    };

		ShaderCreateInfo create_info("vertex_input");
		bool fail = false;
		blender::Map<RetTy, uint> up_map;
    for(auto& vinput :tdata.inputs){
      create_info.vertex_in(vinput.location,vinput.type,vinput.name.c_str());
			auto conv_t = CompTypeConv(vinput.type);
			if(conv_t.first == 100){
				fail = true;
				break;
			}
			uint loc = GPU_vertformat_attr_add(format, vinput.name.c_str(),conv_t.second.first,  conv_t.first, conv_t.second.second );
			up_map.add(conv_t,loc);
    }
		if(fail){
			return ;
		}


		GPUOffScreen *offscreen = GPU_offscreen_create(
    SizeW, SizeH, false, GPU_RGBA8, NULL);
    GPU_offscreen_bind(offscreen, false);
    GPUTexture *texture = GPU_offscreen_color_texture(offscreen);
    
    create_info.vertex_source("gpu_vert_none_test.glsl");
    create_info.transform_feedback_mode(GPU_SHADER_TFB_POINTS);



    VKShader::debug_print = [tdata](){
        return tdata.debugPrintf +"\n";
     };

    GPUShader *shader = GPU_shader_create_from_info(reinterpret_cast<GPUShaderCreateInfo *>(&create_info));
    immBindShader(shader);
    GPU_raster_discard(true);
    immBeginAtMost(GPU_PRIM_POINTS, 1);

		for( auto& conv_t : up_map.keys()){
			immFunc( up_map.lookup_as(conv_t) , conv_t.second.first, conv_t.first, conv_t.second.second);
	  };

    immEnd();
    immUnbindProgram();
    GPU_shader_free(shader);
    GPU_finish();
    GPU_offscreen_free(offscreen);
    printf("Used By =====================\n%s",tdata.shaderUsed.c_str());
  };

  for(int i =0;i < vi_test::test_all ;i++){
    test_print(i);
  }

}
GPU_TEST(vertex_input);

}  // namespace blender::gpu::tests
