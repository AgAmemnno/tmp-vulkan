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
#include <regex>


#include <BLI_map.hh>

#include "vertex_input.hh"

namespace blender::gpu::tests {

const size_t SizeW = 1;
const size_t SizeH  = 1;
static void test_vertex_input()
{


  using namespace shader;


  auto test_print = [&](int i_){

    int i_mod = i_%3;
    int i    =  i_/3;
    printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Test %d \n",i_);
    if(i_ == 82){
      printf(">>>>>");
    }

    auto tdata = vi_test::TestData[i];

    Vector<float> expected = {};

    GPUVertFormat *format = immVertexFormat();

		typedef std::pair<GPUVertCompType,GPUVertFetchMode> RetTy2;
		typedef  std::pair<uint,RetTy2> RetTy;
    auto to_string = [](GPUVertCompType comp_) -> const char*{
      #define FORMAT_COMP_TYPE_STRING(x)\
case x: \
    { return "" #x; };
      switch(comp_){
      FORMAT_COMP_TYPE_STRING(GPU_COMP_I8)
      FORMAT_COMP_TYPE_STRING(GPU_COMP_U8)
      FORMAT_COMP_TYPE_STRING(GPU_COMP_I16)
      FORMAT_COMP_TYPE_STRING(GPU_COMP_U16)
      FORMAT_COMP_TYPE_STRING(GPU_COMP_I32)
      FORMAT_COMP_TYPE_STRING(GPU_COMP_U32)
      FORMAT_COMP_TYPE_STRING( GPU_COMP_F32)
      FORMAT_COMP_TYPE_STRING(GPU_COMP_I10)
      default:
      BLI_assert_unreachable();
        }
      return "";
      #undef FORMAT_COMP_TYPE_STRING
      };
    auto to_string_fmode = [](GPUVertFetchMode comp_) -> const char*{
    #define FORMAT_COMP_TYPE_STRING(x)\
    case x: \
    { return "" #x; };
    switch(comp_){
    FORMAT_COMP_TYPE_STRING(GPU_FETCH_FLOAT)
    FORMAT_COMP_TYPE_STRING(GPU_FETCH_INT)
    FORMAT_COMP_TYPE_STRING(GPU_FETCH_INT_TO_FLOAT_UNIT)
    FORMAT_COMP_TYPE_STRING(GPU_FETCH_INT_TO_FLOAT)
    default:
    BLI_assert_unreachable();
    }
    return "";
    #undef FORMAT_COMP_TYPE_STRING
    };


		auto CompTypeConv = [i_mod](Type ty)-> RetTy {
			uint comp_l = 0;

      switch(ty){
        case Type::FLOAT:
        case Type::UINT:
        case Type::INT:
        case Type::BOOL:
        comp_l = 1;
        break;
        case Type::VEC2:
        case Type::UVEC2:
        case Type::IVEC2:
        comp_l = 2;
        break;
        case Type::VEC3:
        case Type::UVEC3:
        case Type::IVEC3:
        comp_l = 3;
        break;
        case Type::VEC4:
        case Type::UVEC4:
        case Type::IVEC4:
        comp_l = 4;
        break;
        case Type::MAT3:
        comp_l= 9;
        break;
        case Type::MAT4:
        comp_l = 16;
        break;
        };
			GPUVertCompType comp_t  = (GPUVertCompType)0;

      GPUVertFetchMode fmode   =  (i_mod==0)?GPU_FETCH_FLOAT :  (i_mod==1)?GPU_FETCH_INT_TO_FLOAT_UNIT: GPU_FETCH_INT_TO_FLOAT ;
      GPUVertFetchMode imode   =   GPU_FETCH_INT;
      GPUVertFetchMode mode     =  GPU_FETCH_FLOAT;
      switch(ty){
        case Type::FLOAT:
        case Type::VEC2:
        case Type::VEC3:
        case Type::VEC4:
        case Type::MAT3:
        case Type::MAT4:
        if(fmode  ==  GPU_FETCH_INT_TO_FLOAT){
          if(comp_l!=2){
            return std::make_pair(100, std::make_pair(comp_t,mode));
          }
        }
         mode   =  fmode;
         break;
         case Type::UINT:
         case Type::UVEC2:
         case Type::UVEC3:
         case Type::UVEC4:
         case Type::INT:
         case Type::IVEC2:
         case Type::IVEC3:
         case Type::IVEC4:
         case Type::BOOL:
         mode  =  imode;
         break;
        }




      GPUVertCompType comp_t_f  = (mode == GPU_FETCH_INT_TO_FLOAT_UNIT) ? GPU_COMP_U8 : (mode == GPU_FETCH_INT_TO_FLOAT) ?  GPU_COMP_I32 : GPU_COMP_F32;
      GPUVertCompType comp_t_u  = (mode == GPU_FETCH_INT_TO_FLOAT_UNIT) ? GPU_COMP_U8 :GPU_COMP_U32; 
      GPUVertCompType comp_t_i  =  (mode == GPU_FETCH_INT_TO_FLOAT_UNIT) ?GPU_COMP_I8 :GPU_COMP_I32;
      switch(ty){
        case Type::FLOAT:
        case Type::VEC2:
        case Type::VEC3:
				case Type::VEC4:
				case Type::MAT3:
				case Type::MAT4:
         comp_t = comp_t_f;
         break;
				case Type::UINT:
				case Type::UVEC2:
				case Type::UVEC3:
        case Type::UVEC4:
        comp_t = comp_t_u;
        break;
        case Type::BOOL:
				case Type::INT:
				case Type::IVEC2:
				case Type::IVEC3:
				case Type::IVEC4:
        comp_t = comp_t_i;
        break;
			}

      if(mode  == GPU_FETCH_FLOAT){
        comp_t = GPU_COMP_F32;
      }


			switch(ty){
        case Type::FLOAT:
					return  std::make_pair(comp_l, std::make_pair(comp_t,mode));
        case Type::VEC2:
					return  std::make_pair(comp_l, std::make_pair(comp_t,mode));
        case Type::VEC3:
					return  std::make_pair(comp_l, std::make_pair(comp_t,mode));
				case Type::VEC4:
					return  std::make_pair(comp_l, std::make_pair(comp_t,mode));
				case Type::MAT3:
					return  std::make_pair(comp_l, std::make_pair(comp_t,mode));
				case Type::MAT4:
					return  std::make_pair(comp_l, std::make_pair(comp_t,mode));
				case Type::UINT:
					return  std::make_pair(comp_l, std::make_pair(comp_t,mode));
				case Type::UVEC2:
					return  std::make_pair(comp_l, std::make_pair(comp_t,mode));
				case Type::UVEC3:
					return  std::make_pair(comp_l, std::make_pair(comp_t,mode));
				case Type::INT:
					return  std::make_pair(comp_l, std::make_pair(comp_t,mode));
				case Type::IVEC2:
					return  std::make_pair(comp_l, std::make_pair(comp_t,mode));
				case Type::IVEC3:
					return  std::make_pair(comp_l, std::make_pair(comp_t,mode));
				case Type::IVEC4:
					return  std::make_pair(comp_l, std::make_pair(comp_t,mode));
				case Type::BOOL:
					return  std::make_pair(comp_l, std::make_pair(comp_t,mode));
        default:
				return std::make_pair(comp_l, std::make_pair(comp_t,mode));
				BLI_assert_unreachable();
			}
			};
    auto immFunc = [&expected](uint loc,GPUVertCompType comp_t,uint comp_l,GPUVertFetchMode mode = GPU_FETCH_FLOAT)
    {
			float Upload[16] =  {1.,2.,3.,4.,5.,6.,7.,8.,9.,10.,11.,12.,13.,14.,15.,16.};
      uchar UploadUchar[16];
      for(int i =0;i<16;i++){
        UploadUchar[i] = (uchar)Upload[i];
      }
      auto set_expected = [&expected,mode,comp_t](float val){
      switch( mode){
        case GPU_FETCH_FLOAT:
        case GPU_FETCH_INT_TO_FLOAT:
        case GPU_FETCH_INT:
          expected.append(val);
          break;
        case GPU_FETCH_INT_TO_FLOAT_UNIT:
        switch(comp_t){
            case GPU_COMP_I16:
              expected.append(val/float(INT16_MAX));
              break;
            case GPU_COMP_U8:
              expected.append(val/float(UINT8_MAX));
              break;
        }
        
        }
      };

      switch(comp_t){
        case GPU_COMP_F32:
            switch(comp_l){
            case 1:
                immAttr1f(loc, Upload[0]);
                set_expected(Upload[0]);
                break;
            case 2:
                immAttr2f(loc, Upload[0],Upload[1]);
                set_expected(Upload[0]);
                set_expected(Upload[1]);
                break;
            case 3:
                immAttr3f(loc, Upload[0],Upload[1],Upload[2]);
                set_expected(Upload[0]);
                set_expected(Upload[1]);
                set_expected(Upload[2]);
                break;
            case 4:
                immAttr4f(loc, Upload[0],Upload[1],Upload[2],Upload[3]);
                set_expected(Upload[0]);
                set_expected(Upload[1]);
                set_expected(Upload[2]);
                set_expected(Upload[3]);
                break;
            case 16:

                immAttr16f(loc, Upload);
                for(int i=0;i<16;i++){
                  set_expected(Upload[i]);
                }
                break;
           }
            break;
        case GPU_COMP_U32:
            switch(comp_l){

              case 1:
                  immAttr1u(loc, Upload[0]);
                  set_expected(Upload[0]);
                  break;
              case 2:
                  immAttr2u(loc, Upload[0],Upload[1]);
                  set_expected(Upload[0]);
                  set_expected(Upload[1]);
                  break;
              case 4:
                  immAttr4u(loc, Upload[0],Upload[1], Upload[2],Upload[3]);
                  set_expected(Upload[0]);
                  set_expected(Upload[1]);
                  set_expected(Upload[2]);
                  set_expected(Upload[3]);
                  break;

           }
            break;
        case GPU_COMP_I32:
            switch(comp_l){
              case 1:
                  immAttr1i(loc, (int)Upload[0]);
                  set_expected(Upload[0]);
                  break;
              case 2:
                  immAttr2i(loc, (int)Upload[0],(int)Upload[1]);
                  set_expected(Upload[0]);
                  set_expected(Upload[1]);
                  break;
           }
            break;
        case GPU_COMP_I16:
            switch(comp_l){
              case 2:
                  immAttr2s(loc, (short)Upload[0],(short)Upload[1]);
                  set_expected(Upload[0]);
                  set_expected(Upload[1]);
                  break;
           }
            break;
        case GPU_COMP_U8:
            switch(comp_l){
                case 1:
                  immAttr1ub(loc, (uchar)Upload[0]);
                  set_expected(Upload[0]);
                  break;
               case 2:
                  immAttr2ub(loc, (uchar)Upload[0],(uchar)Upload[1]);
                  set_expected(Upload[0]);
                  set_expected(Upload[1]);
                  break;
              case 3:
                  immAttr3ub(loc, (uchar)Upload[0],(uchar)Upload[1],(uchar)Upload[2]);
                  set_expected(Upload[0]);
                  set_expected(Upload[1]);
                  set_expected(Upload[2]);
                  break;
              case 4:
                  immAttr4ub(loc, (uchar)Upload[0],(uchar)Upload[1],(uchar)Upload[2],(uchar)Upload[3]);
                  set_expected(Upload[0]);
                  set_expected(Upload[1]);
                  set_expected(Upload[2]);
                  set_expected(Upload[3]);
                  break;
            case 16:
                  immAttr16ubv(loc, UploadUchar);
                  for(int i=0;i<16;i++){
                    set_expected(Upload[i]);
                  }
                  break;
           }
            break;
      }
    };

		ShaderCreateInfo create_info("vertex_input");
		bool fail = false;
		blender::Map<uint,RetTy> up_map;
    for(auto& vinput :tdata.inputs){
      create_info.vertex_in(vinput.location,vinput.type,vinput.name.c_str());
			auto conv_t = CompTypeConv(vinput.type);
			if(conv_t.first == 100){
				fail = true;
				break;
			}
      
			uint loc = GPU_vertformat_attr_add(format, vinput.name.c_str(),conv_t.second.first,  conv_t.first, conv_t.second.second );
			up_map.add(loc,conv_t);
    }
		if(fail){
      printf(">>>>>>>>>>>>>> Pass though not suitable case. %s n",tdata.shaderUsed.c_str());
			return ;
		}
    if(up_map.size()>10){
      printf("");
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

		for( auto& loc : up_map.keys()){
       RetTy conv_t = up_map.lookup_as(loc);
			immFunc( loc, conv_t.second.first, conv_t.first, conv_t.second.second);
      printf(">>>>>>>>>>>>>>UPLOAD    Location [%d] >>>>>>>>>>> CompType[%s]   CompLen [%d]  GPUVertFetchMode[%s] \n", loc,to_string(conv_t.second.first),conv_t.first,to_string_fmode(conv_t.second.second) );
	  };

    auto& tools = VKContext::get()->debugging_tools_get();
   
    tools.post_proc = [expected](const char* message)
    {
      std::string str(message);
      std::regex r("^(.*) VertexInput Checkers (.*)"); // entire match will be 2 numbers
    
      std::smatch m;
      std::regex_search(str, m, r);
      if(m.size()>2){
         auto s = m[2].str();
         std::regex r("^([/s/S ]*)v_([0-9]+) (.*)");
            
      std::smatch res;

      std::string::const_iterator searchStart( s.cbegin() );
      int n = -1;
      while ( std::regex_search( searchStart, s.cend(), res, r ) )
      {
        
        int loc = 0;

        if(res.size() >3){
          auto Loc = res[2].str();
          loc = std::stoi(Loc);
        auto Val = res[3].str();
        std::string delimiter = ",";
        size_t pos = 0;
        std::string token;


        bool next = false;
        while ((pos = Val.find(delimiter)) != std::string::npos) {
            token = Val.substr(0, pos);
            std::stringstream stream(token);
            float f;
            if (stream >> f) {
              n++;
              //EXPECT_FLOAT_EQ(f,expected[n]);
              EXPECT_NEAR(f,expected[n],0.001);
              printf("Expected  %f  Val %f\n",expected[n],f);
            }else{
              next = true;
              break;
            }
            std::cout << token << std::endl;
            Val.erase(0, pos + delimiter.length());
            for(auto c :Val){
              if(c == 'v'){
                  next = true;
                  break;
              }else if(c != ' '){
                  break;
              }
            }
            if(next){
              break;
            }
        }
        if( !next && Val.length() > 2){
            std::stringstream stream(Val);
            float f;
            if (stream >> f) {
              n++;
              EXPECT_NEAR(f,expected[n],0.001);
              printf("Expected  %f  Val %f\n",expected[n],f);
            }else{
              BLI_assert_unreachable();
            }
            Val.clear();
        }
        s = Val;
        searchStart= s.cbegin();
        }else{
          break;
        }
      }

      }

    };

    immEnd();
    immUnbindProgram();
    GPU_shader_free(shader);
    GPU_finish();
    GPU_offscreen_free(offscreen);
    printf("Used By =====================\n%s",tdata.shaderUsed.c_str());
  };

  for(int i =0;i < vi_test::test_all*3 ;i++){
    test_print(i);
  }

}
GPU_TEST(vertex_input);

}  // namespace blender::gpu::tests
